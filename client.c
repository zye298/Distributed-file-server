#include <stdio.h>
#include "udp.h"

#include "message.h"

#include "mfs.h"

// client code
int main(int argc, char *argv[]) {

    // extern int x;

    MFS_Init("localhost", atoi(argv[1]) );
    
    // int sd = UDP_Open(20000);
    // int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    extern struct sockaddr_in addrSnd, addrRcv;

    extern int sd;
    extern int rc;

    message_t m;

    m.mtype = MFS_CRET;
    m.inum = 0;
    m.type = 1;
    strcpy(m.name,"witcher");
    printf("client:: send message %d\n", m.mtype);
    rc = UDP_Write(sd, &addrSnd, (char *) &m, sizeof(message_t));
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, (char *) &m, sizeof(message_t));
    printf("client:: got reply [size:%d rc:%d type:%d]\n", rc, m.rc, m.mtype);

    //write test
    m.mtype = MFS_WRITE;
    m.inum = 1;
    memcpy(m.buffer , "START BLOCK 1\x00\x00\x00\x00\x00\x00\x00\x00\x00 End BLOCK", 4096);
    m.offset = 0;
    m.nbytes = 4096;

    printf("client:: send message %d\n", m.mtype);
    rc = UDP_Write(sd, &addrSnd, (char *) &m, sizeof(message_t));
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, (char *) &m, sizeof(message_t));
    printf("client:: got reply [size:%d rc:%d type:%d]\n", rc, m.rc, m.mtype);  

    //Read test
    m.mtype = MFS_READ;
    m.inum = 1;
    m.offset = 0;
    m.nbytes = 4096;

    printf("client:: send message %d\n", m.mtype);
    rc = UDP_Write(sd, &addrSnd, (char *) &m, sizeof(message_t));
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    message_t testm;
    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, (char *) &testm, sizeof(message_t));
    printf("client:: got reply [size:%d rc:%d type:%d], The read message is --%s--\n", rc, testm.rc, testm.mtype, testm.buffer);


    //Read test
    m.mtype = MFS_STAT;
    m.inum = 1;
    
    printf("client:: send message %d\n", m.mtype);
    rc = UDP_Write(sd, &addrSnd, (char *) &m, sizeof(message_t));
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    message_t teststate;
    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, (char *) &teststate, sizeof(message_t));
    printf("client:: got reply [size:%d rc:%d type:%d], The file type is --%d--\n", rc, teststate.rc, teststate.mtype,teststate.type );

    return 0;
}

