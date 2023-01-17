
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>


#include "udp.h"

#include "ufs.h"

#include "mfs.h"

#include "message.h"


int sd;

int image_file_descp;

int *image_ptr;

super_t *s;

inode_t fail_node;

data_block_t *data_blocks;

inode_t * inodes;

void *address(int addr) {

    
    return (char *)s + addr*4096;
}

unsigned int get_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |= 0x1 << offset;
}

void clean_bit(unsigned int *bitmap, int position) {
    int index = position / 32;
    int offset = 31 - (position % 32);
    bitmap[index] &= ~(0x1 << offset);
}


int server_lookup(int pinum, char* name) {

    if(pinum < 0){
        
        return -1;
    }

    if (pinum >= s->num_inodes) { 

        
        return -1;
    }

    if(inodes[pinum].type == UFS_REGULAR_FILE) {

        return -1;
    }
    
    for(int i = 0; i < 30 ; i++) {

        if(inodes[pinum].direct[i] != -1) {
        
            int data_block_ptr = inodes[pinum].direct[i] - s->data_region_addr;

            for(int ent_i = 0; ent_i < 128; ent_i++) {

                dir_ent_t entry = data_blocks[data_block_ptr].entries[ent_i];

                if(strcmp(entry.name, name) == 0){

                    return entry.inum;
                }
            }
        }
    }
    return -1;
}

inode_t server_stat(int inum) {

    if(inum < 0 || inum >= s->num_inodes) { 

        fail_node.type = -1;
        fail_node.size = -1;
        return fail_node;
    }

    return inodes[inum];;
}

int server_write(int inum, char *buffer, int offset, int nbytes) {

    if( nbytes < 0 || nbytes > 4096) {

        return -1;
    }
    if(  offset/4096 >= 30) {

        return -1;
    }

    if(  inum < 0 || inum >= s->num_inodes) { 
        
        return -1;
    }

    if( inodes[inum].type == UFS_REGULAR_FILE){

        int data_block_ptr;

        data_block_ptr = inodes[inum].direct[offset/UFS_BLOCK_SIZE] - s->data_region_addr;
        
        memcpy(&data_blocks[data_block_ptr].entries, buffer, nbytes);

        inodes[inum].size += nbytes;

        return 0;
    }

    return -1;

}

int server_read(int inum, char *buffer, int offset, int nbytes) {

    int data_block_ptr;
    if(nbytes < 0 || nbytes > 4096) {
        return -1;
    }

    if( offset/4096 >= 30) {
        return -1;
    }

    data_block_ptr = inodes[inum].direct[offset/UFS_BLOCK_SIZE] - s->data_region_addr;

    if(inodes[inum].type == 1) {

        memcpy(buffer, &data_blocks[data_block_ptr].entries, nbytes);
    }
    else if (inodes[inum].type == 0){
        memcpy((MFS_DirEnt_t *)buffer, &data_blocks[data_block_ptr].entries, nbytes);
    }
    
    return 0;
}

int server_create(int pinum, int type, char *name) {

    if(inodes[pinum].type != MFS_DIRECTORY) {
        
        return -1;
    }

    if(strlen(name) > 28) {
        return -1;
    }

   

    int next_unused_inode;

    for(int i = 0; i< s->num_inodes; i++) {

        int bit = get_bit(address(s->inode_bitmap_addr), i);
        if(bit == 0) {

            next_unused_inode =  i;
            break;
        }
    }
    inodes[next_unused_inode].type = type;

    int data_block_ptr = inodes[pinum].direct[0] - s->data_region_addr;

    for(int i = 0; i< 128; i++) {

        //get unused
        if(data_blocks[data_block_ptr].entries[i].inum == -1) {

            data_blocks[data_block_ptr].entries[i].inum = next_unused_inode;

            strcpy(data_blocks[data_block_ptr].entries[i].name, name);

            set_bit(address(s->inode_bitmap_addr), next_unused_inode);

            inodes[pinum].size += sizeof(dir_ent_t);

            break;
        }
    }

    if(type != MFS_DIRECTORY) {
        for(int i = 0; i< 30; i++) {

            int new_data_block_ptr;

            for(int i = 0; i<s->num_data; i++) {

                int bit = get_bit(address(s->data_bitmap_addr), i);
                
                if(bit == 0) {
                    new_data_block_ptr = i;

                    break;
                }
            }

            inodes[next_unused_inode].direct[i] = new_data_block_ptr + s->data_region_addr;

            set_bit(address(s->data_bitmap_addr), new_data_block_ptr);
        }
    }
    else{

        inodes[next_unused_inode].size = 64;

        for(int i = 1; i < 30; i++) {

            inodes[next_unused_inode].direct[i] = -1;
        }

        dir_ent_t new_block_entries[128];

        strcpy(new_block_entries[0].name, ".");

        new_block_entries[0].inum = next_unused_inode;

        strcpy(new_block_entries[1].name, "..");

        new_block_entries[1].inum = pinum;

        for(int i = 2; i < 128; i++) {
            new_block_entries[i].inum = -1;
        }

        int new_data_block_ptr;

        for(int i = 0; i< s->num_data; i++) {

            int bit = get_bit(address(s->data_bitmap_addr), i);
             
            if(bit == 0) {
                new_data_block_ptr = i;

                break;
            }
        }
        inodes[next_unused_inode].direct[0] = new_data_block_ptr + s->data_region_addr;

        memcpy(&data_blocks[new_data_block_ptr].entries, new_block_entries, UFS_BLOCK_SIZE);

        set_bit(address(s->data_bitmap_addr), new_data_block_ptr);

    }

    return 0;
}

int server_unlink(int pinum, char *name){

    int data_block_ptr = inodes[pinum].direct[0] - s->data_region_addr;

    int not_found = 1;
    
    for(int i = 0; i < 128; i++) {

        if(strcmp(data_blocks[data_block_ptr].entries[i].name, name) == 0){


            not_found = 0;
        }
    }

    if(not_found == 1){

        return 0;
    }

    if(inodes[pinum].type != UFS_DIRECTORY){

        return -1;
    }

    for(int i = 0; i < 128; i++) {

        if(strcmp(data_blocks[data_block_ptr].entries[i].name, name) == 0){

            int deleted_inode = data_blocks[data_block_ptr].entries[i].inum;

            // Check if the file to be unlinked is a directory
            if(inodes[deleted_inode].type == UFS_DIRECTORY) {

                // Check if the directory is empty
                if(inodes[deleted_inode].size > 64) { 
                    
                    return -1;
                }

                int deleted_block_ptr = inodes[deleted_inode].direct[0] - s->data_region_addr;

                for(int i = 0; i<2; i++) {

                    data_blocks[deleted_block_ptr].entries[i].inum = -1;

                    strcpy(data_blocks[deleted_block_ptr].entries[i].name, "\0");

                    clean_bit(address(s->inode_bitmap_addr), data_blocks[deleted_block_ptr].entries[i].inum);
                }
                clean_bit(address(s->data_bitmap_addr), inodes[deleted_inode].direct[0]-s->data_region_addr);

                inodes[deleted_inode].direct[0] = -1;
            }
            else {
                for(int i = 0; i < 30; i++) {
                    int deleted_block_ptr = inodes[deleted_inode].direct[i] - s->data_region_addr;

                    for(int i_ = 0; i_<64; i_++) {

                        data_blocks[deleted_block_ptr].entries[i].inum = -1;

                        strcpy(data_blocks[deleted_block_ptr].entries[i].name, "\0");

                        clean_bit(address(s->inode_bitmap_addr), data_blocks[deleted_block_ptr].entries[i].inum);
                    }
                    clean_bit(address(s->data_bitmap_addr), inodes[deleted_inode].direct[i]-s->data_region_addr);

                    inodes[deleted_inode].direct[i] = -1;
                }
            }

            inodes[pinum].size -= sizeof(dir_ent_t);

            inodes[deleted_inode].type = 0;

            inodes[deleted_inode].size = 0;

            data_blocks[data_block_ptr].entries[i].inum = -1;

            strcpy(data_blocks[data_block_ptr].entries[i].name, "\0");

            break;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {

    int port = atoi(argv[1]);

    char* img = argv[2];

    sd = UDP_Open(port);
    
    image_file_descp = open(img, O_RDWR);

    if (sd < 0 || image_file_descp < 0){

		printf("image does not exist\n");
		exit(1);
	}
    
    struct stat fstat_;
    
    fstat(image_file_descp,&fstat_);

    image_ptr = mmap(NULL, fstat_.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, image_file_descp, 0);

    s = (super_t *) image_ptr;

    inodes = malloc(UFS_BLOCK_SIZE * s->inode_region_len);

    for(int i = 0; i< 32; i++) {

        lseek(image_file_descp, s->inode_region_addr * UFS_BLOCK_SIZE  + i*sizeof(inode_t), SEEK_SET);

        read(image_file_descp, &inodes[i], sizeof(inode_t));
    }

    data_blocks = malloc(UFS_BLOCK_SIZE * s->num_data);

    for(int i = 0; i < 32; i++) {

        lseek(image_file_descp, s->data_region_addr * UFS_BLOCK_SIZE + i*UFS_BLOCK_SIZE, SEEK_SET);

        read(image_file_descp, &(data_blocks[i].entries), UFS_BLOCK_SIZE);
    }

    while(1) {

        struct sockaddr_in addr;

        printf("server:: waiting...\n");

        message_t m_rcv;

        int rc = UDP_Read(sd, &addr, (char *)&m_rcv, sizeof(message_t));

        printf("server:: read message [size:%d mtype:%d name:%s inode:%d]\n", rc, m_rcv.mtype, (char *)m_rcv.name, m_rcv.inum);

        message_t m_reply;
        if( rc > 0 ) {
            if(m_rcv.mtype == 2 ){
                m_reply.rc = server_lookup(m_rcv.inum, m_rcv.name);

                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));
            }
            else if (m_rcv.mtype == 3){
                m_reply.rc = -1;

                inode_t inode = server_stat(m_rcv.inum); 

                m_reply.rc = 0;

                m_reply.type = inode.type;

                m_reply.nbytes = inode.size;


                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));
            }
            else if (m_rcv.mtype == 4){
                m_reply.rc = server_write(m_rcv.inum, m_rcv.buffer, m_rcv.offset, m_rcv.nbytes);
                
                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));
        
            }    
            else if (m_rcv.mtype == 5){
                char *read = (char *)malloc(UFS_BLOCK_SIZE);

                m_reply.rc = server_read(m_rcv.inum, read, m_rcv.offset, m_rcv.nbytes);

                memcpy(m_reply.buffer, read, UFS_BLOCK_SIZE);

                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));

                free(read);
            }
            else if (m_rcv.mtype == 6){
                m_reply.rc = server_create(m_rcv.inum, m_rcv.type, m_rcv.name);

                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));

            }
            else if(m_rcv.mtype == 7){


                m_reply.rc = server_unlink(m_rcv.inum, m_rcv.name);

                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));

            }
            else if (m_rcv.mtype == 8){
                lseek(image_file_descp, s->inode_bitmap_addr*UFS_BLOCK_SIZE, SEEK_SET);
                write(image_file_descp, address(s->inode_bitmap_addr), UFS_BLOCK_SIZE);

                for(int i = 0; i<s->num_inodes; i++) {
                    lseek(image_file_descp, i*sizeof(inode_t) + s->inode_region_addr*UFS_BLOCK_SIZE, SEEK_SET);
                    write(image_file_descp, &inodes[i], sizeof(inode_t));
                }

                lseek(image_file_descp, s->data_bitmap_addr*UFS_BLOCK_SIZE, SEEK_SET);
                write(image_file_descp, address(s->data_bitmap_addr), UFS_BLOCK_SIZE);

                for(int i = 0; i<s->num_data; i++) {
                    lseek(image_file_descp, i*UFS_BLOCK_SIZE+s->data_region_addr*UFS_BLOCK_SIZE, SEEK_SET);
                    write(image_file_descp, &(data_blocks[i].entries), UFS_BLOCK_SIZE);
                }

                m_reply.rc = 0;
                UDP_Write(sd, &addr, (char *)&m_reply, sizeof(message_t));
                fsync(image_file_descp);
                close(image_file_descp);
                exit(0);
            }
            
        }


    }

    lseek(image_file_descp, s->inode_bitmap_addr*UFS_BLOCK_SIZE, SEEK_SET);
    write(image_file_descp, address(s->inode_bitmap_addr), UFS_BLOCK_SIZE);

    for(int i = 0; i<s->num_inodes; i++) {
        lseek(image_file_descp, i*sizeof(inode_t) + s->inode_region_addr*UFS_BLOCK_SIZE, SEEK_SET);
        write(image_file_descp, &inodes[i], sizeof(inode_t));
    }

    lseek(image_file_descp, s->data_bitmap_addr*UFS_BLOCK_SIZE, SEEK_SET);
    write(image_file_descp, address(s->data_bitmap_addr), UFS_BLOCK_SIZE);

    for(int i = 0; i<s->num_data; i++) {
        lseek(image_file_descp, i*UFS_BLOCK_SIZE+s->data_region_addr*UFS_BLOCK_SIZE, SEEK_SET);
        write(image_file_descp, &(data_blocks[i].entries), UFS_BLOCK_SIZE);
    }
   
    free(image_ptr);

    free(data_blocks);

    free(inodes);

    fsync(image_file_descp);

    UDP_Close(sd);

    close(image_file_descp);
}


