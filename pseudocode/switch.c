/* Step 1 : Will wait until somebody connects */
/* Step 2 :  Hand shake  */
/* Step 3 :  Will send advertisement of new block  */
/* Step 4 :  Will wait until somebody requests SEG  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "port.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define COMMAND_INV 0x1
#define COMMAND_GETBLOCK 0x2
#define COMMAND_BLOCK 0x3

#define BUFSIZE 2048

struct testRINV {
    char command[3];
    char hash[32];
    uint16_t segCount;
} __attribute__((__packed__)) RINV_MESSAGE = {{'I', 'N', 'V'}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x5e, 0x70, 0xc2, 0xab, 0xea, 0xbd, 0x9b, 0x38, 0x0a, 0x0a, 0x54, 0xa9, 0x15, 0x58, 0x09, 0x41, 0x11, 0xf3, 0xf4, 0x75, 0x8b, 0xb1}, 452u};

struct RBLOCKMessage {
    uint8_t command;
    char hash[32];
    uint16_t segNo;
    char payload[100];
} __attribute__((__packed__));



int main(int argc, char **argv)
{
    struct sockaddr_in myaddr;	/* our address */
    struct sockaddr_in remaddr;	/* remote address */
    socklen_t addrlen = sizeof(remaddr);		/* length of addresses */
    int recvlen;			/* # bytes received */
    int fd;				/* our socket */
    int msgcnt = 0;			/* count # of messages we received */
    char buf[BUFSIZE];	/* receive buffer */


    /* create a UDP socket */

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return 0;
    }

    /* bind the socket to any valid IP address and a specific port */

    int fragmentRetrans = 0;

    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(SERVICE_PORT);

    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }

    struct sockaddr_in pyaddr;	/* remote address */
    memset((char *)&pyaddr, 0, sizeof(pyaddr));
    pyaddr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &pyaddr.sin_addr);
    pyaddr.sin_port = htons(8081);

    printf("I am pretending to be a P4 switch and waiting for clients to connect in %d\n", SERVICE_PORT);

    pid_t pid = 0;

    /* now loop, receiving data and printing what we received */
    for (;;) {
        recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
        printf("received msg from: %u\n", ntohs(remaddr.sin_port));
        buf[recvlen] = 0;
        int secret = 3;
        if (recvlen > 0) {
            char command[20];
            sprintf(command, "%.*s", 6, buf );
            if (strstr(command, "REY") != NULL && buf[3] == 0x10) {
                printf("received SYN: \"%s\" (%d bytes)\n", buf, recvlen);
                /*SHOULD SEND SYNACK*/
                buf[3] = 0x21;
                buf[6] = 0xff;
                printf("sending response \"%s\"\n", buf);
                if (sendto(fd, buf, 6, 0, (struct sockaddr *)&remaddr, addrlen) < 0)
                    perror("sendto");}
            else if (strstr(command, "REY") != NULL && (buf[3]&0xf0) == 0x30){
                printf("received MYACK: \"%s\" (%d bytes)\n", buf, recvlen);
                char string[5];
                sprintf(string,"%.*s", 1, buf + 5);
                printf("MYACK SECRET %s\n", string);

                if(pid==0){
                    /* start python stuff */
                    pid = fork();
                    if (pid == 0)
                    {
                        close(fd);
                        sprintf(buf, "./translate.py %u", ntohs(remaddr.sin_port));

                        system (buf);
                        return 1;
                    }
                } else {
                    //signal port change
                    //int port = ntohs(remaddr.sin_port);
                    fragmentRetrans = 0;
                    //sendto(fd, &port, 4, 0, (struct sockaddr *)&pyaddr, addrlen);
                }

                sleep(4);

                // handshake done. advertice new block
                printf("advertise block \n");
                sendto(fd, &RINV_MESSAGE, sizeof(RINV_MESSAGE), 0, (struct sockaddr *)&remaddr, addrlen);

            }else if (strstr(command, "SEG") != NULL){
                printf("received GETSEG: \"%s\" (%d bytes)\n", buf, recvlen);

                struct RBLOCKMessage msg;
                msg.command = COMMAND_BLOCK;
                memcpy(&msg.hash, buf+3, 32);
                memcpy(&msg.segNo, buf+35, 2);

                // drop the 100th fragment at the first try
                if(fragmentRetrans<2 && msg.segNo == 30){
                    printf("drop packet 30\n");
                    fragmentRetrans += 1;
                } else {
                    sendto(fd, &msg.segNo, 2, 0, (struct sockaddr *)&pyaddr, addrlen);
                }

                //printf("send fragment %d of requested block\n", msg.segNo);

                //sendto(fd, &msg, sizeof(msg), 0, (struct sockaddr *)&remaddr, addrlen);
                //TODO: test out of order
            }else if (strstr(command, "ADV") != NULL || buf[0] == COMMAND_INV){
                printf("would receive new block\n");
            }else {
                printf("UNKNOWN \"%s\" (%d bytes)\n", buf, recvlen);
            }
        }
        else
            printf("uh oh - something went wrong!\n");

    }
}
