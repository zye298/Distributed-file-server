#include <sys/select.h>
#include <sys/time.h>
#include "mfs.h"
#include "udp.c"
#include "message.h"

#define BUFFER_SIZE (4096)


message_t m_snd,m_rcv;

struct sockaddr_in addrSnd, addrRcv;

int sd;

int rc;

message_t communicate(message_t m_snd, message_t m_rcv) {

    struct sockaddr_in addrRcv2;

    fd_set fd;

    while (1) {

        UDP_Write(sd, &addrSnd, (char *)&m_snd, sizeof(message_t));

        FD_ZERO(&fd);

        FD_SET(sd, &fd);

        struct timeval time;

        time.tv_sec = 3;

        time.tv_usec = 0;

        int out = select( sd+1 , &fd, NULL, NULL, &time);

        if(out > 0) {

            // printf("out is %d\n",out);
            UDP_Read(sd, &addrRcv2, (char *)&m_rcv, sizeof(message_t));
            
            return m_rcv;

        }
    }

    return m_rcv;
}


int MFS_Init(char *hostname, int port) {

    
    sd = UDP_Open(0);

    rc = UDP_FillSockAddr(&addrSnd, hostname, port);

    return 0;
}

int MFS_Lookup(int pinum, char *name) {

    if(strlen(name) > 28) {

        return -1;
    }

    m_snd.mtype = 2;

    m_snd.inum = pinum;

    strcpy((char*)m_snd.name, name);

    return communicate(m_snd, m_rcv).rc;

}

int MFS_Stat(int inum, MFS_Stat_t *m) {

    m_snd.mtype = 3;

    m_snd.inum = inum;

    m_rcv = communicate(m_snd, m_rcv);

    m->type = m_rcv.type;

    m->size = m_rcv.nbytes;

    return m_rcv.rc;

}

int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    if(offset/4096 >= 30){

        return -1;
    }
    
    if ( nbytes < 0 || nbytes > 4096) {
        return -1;
    }

    m_snd.mtype = 4;

    m_snd.inum = inum;

    memcpy((char *)m_snd.buffer, buffer, 4096);

    m_snd.nbytes = nbytes;

    m_snd.offset = offset;

    return communicate(m_snd, m_rcv).rc;

}

int MFS_Read(int inum, char *buffer, int offset, int nbytes) {

    m_snd.mtype = 5;

    m_snd.inum = inum;

    m_snd.nbytes = nbytes;

    m_snd.offset = offset;



    m_rcv = communicate(m_snd, m_rcv);

    memcpy(buffer, m_rcv.buffer, MFS_BLOCK_SIZE);

    return m_rcv.rc;

}


int MFS_Creat(int pinum, int type, char *name) {

    m_snd.mtype = 6;

    m_snd.inum = pinum;

    m_snd.type = type;

    strcpy((char*)m_snd.name, name);


    return communicate(m_snd, m_rcv).rc;

}


int MFS_Unlink(int pinum, char *name) {

    m_snd.mtype = 7;

    m_snd.inum = pinum;

    strcpy((char*)m_snd.name, name);
    
    return communicate(m_snd, m_rcv).rc;

}


int MFS_Shutdown() {


    m_snd.mtype = 8;


    return communicate(m_snd, m_rcv).rc;

}