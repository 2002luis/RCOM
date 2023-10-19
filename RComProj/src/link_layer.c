// Link layer protocol implementation
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define MAX_SIZE 1024

int nS = 0, nR = 0, timeoutLim = 0, fd, role = 0;
struct termios oldtio;

void myStrCpy(unsigned char* dest, const unsigned char* orig){
    int i;
    for(i = 0; orig[i]!='\0';i++){
        dest[i]=orig[i];
    }
    dest[i]='\0';
}

int wFlag(int *fd, unsigned char A, unsigned char C){
    unsigned char buf[5];
    buf[0] = 0x7e;
    buf[1] = A;
    buf[2] = C;
    buf[3] = A^C;
    buf[4] = 0x7e;

    int res = write(*fd,buf,5);
    return res!=5;
}

int dataBcc(const unsigned char* buf, int bufsize){
    int out = buf[4]; //BCC
    for(int i = 5; i < bufsize - 2; i++){
        out = (out ^ buf[i]);
    }
    return out;
}

int stuff(unsigned char* buf, int length){
    for(int i = 4; i < length-2; i++){
        if(buf[i] == 0x7e || buf[i] == 0x7d){
            for(int j = length; j > i; j--){
                buf[j] = buf[j-1];
            }
            buf[i+1] = (buf[i]^0x20);
            buf[i] = 0x7d;

            length = length + 1;
            i++;
        }
    }
    return length;
}

int destuff(unsigned char* buf, int length){
    for(int i = 4; i < (length-3); i++){
        if( buf[i] == 0x7d && ((buf[i+1] == (0x7e^0x20)) || (buf[i+1] == (0x7d^0x20))) ){
            if(buf[i+1] == (0x7e^0x20)){
                buf[i] = 0x7e;
            } else{
                buf[i] = 0x7d;
            }
            for(int j = i+1; j < length; j++){
                buf[j] = buf[j+1];
            }
            length--;
        }
    }
    return length;
}
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = connectionParameters.timeout * 10; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Don't block read

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char buf[5] = {0};

    int state = 0;
    int other_a, other_c;
    timeoutLim = connectionParameters.nRetransmissions;
    int tries = timeoutLim;

    switch(connectionParameters.role){
        case(LlTx):
        role = 1; //transmitter
        if(wFlag(&fd,0x03,0x03)) return -1;
        while(state!=5){
            int res = read(fd,buf,1);
            if(res==0){ //timeout
                tries--;
                state = 0;
                if(tries<=0) return -1;
                wFlag(&fd,0x03,0x03);
            }

            if(state != 4 && buf[0] == 0x7e) state = 1;
            else if(state == 4 && buf[0] == 0x7e) state = 5;
            else if(state == 1){
                if(buf[0]==0x01){
                    other_a = 0x01;
                    state = 2;
                }
                else state = 0;
            }
            else if(state == 2){
                if(buf[0]==0x07){
                    other_c = 0x07;
                    state = 3;
                }
                else state = 0;
            }
            else if (state == 3){
                if(buf[0] == (other_a^other_c)){
                    state = 4;
                }
                else state = 0;
            }
            
        }
        break;

        case(LlRx):
        role = -1; //receiver
        while(state!=5){
            int res = read(fd,buf,1);
            if(res==0){ //timeout
                state = 0;
                tries--;
                if(tries<=0) return -1;
            }
            if(state != 4 && buf[0] == 0x7e) state = 1;
            else if(state == 4 && buf[0] == 0x7e) state = 5;
            else if(state == 1){
            if(buf[0]==0x03){
                other_a = 0x03;
                state = 2;
            }
            else state = 0;
            }
            else if(state == 2){
                if(buf[0]==0x03){
                    other_c = 0x03;
                    state = 3;
                }
                else state = 0;
            }
            else if (state == 3){
                if(buf[0] == (other_a^other_c)){
                    state = 4;
                }
                else state = 0;
            }
        }
        wFlag(&fd,0x01,0x07);
        break;
    }


    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *inbuf, int bufSize)
{
    if(inbuf == NULL || bufSize == 0){
        return 0;
    }
    unsigned char buf[MAX_SIZE];
    unsigned char inBuf[MAX_SIZE];
    myStrCpy(inBuf,inbuf); //inbuf is const and can't be changed
    int frameLength = 6;

    buf[0] = 0x7e;
    buf[1] = 0x01;
    if(nS == 0) buf[2] = 0x00;
    else buf[2] = 0x02;
    buf[3]=buf[1]^buf[2];
    for(int i = 0; i < bufSize; i++){
        buf[4+i]=inBuf[i]; //append input buffer to buf
        frameLength++;
    }
    buf[frameLength-1]=0x7e;
    buf[frameLength-2]=dataBcc(buf,frameLength);

    frameLength = stuff(buf,frameLength);

    int state = 0, res = 0, done = FALSE;
    unsigned char rec[MAX_SIZE];
    int times = timeoutLim;

    while(!done){


        if(write(fd,buf,frameLength)!=frameLength) return -1;
        printf("\nsou mesmo epico e mandei estas coisas: %s\n",buf);
        state = 0;
        res = 0;

        while(state != 5){
            res = read(fd,buf,1);
            if(res==0){ //timeout
                state = 0;
                times--;
                if(times<=0){
                    perror("TIMEOUT LLWRITE");
                    return -1;
                }
                if(write(fd,buf,frameLength)!=frameLength) return -1;
                printf("\na outra funcao e um cringe e deu timeout portanto mandei outra vez\n");
            }


            if(state==0){
                if(buf[0]==0x7e){
                    state = 1;
                    rec[0]=buf[0];
                }
            }
            else if(state==1){
                if(buf[0]==0x01){
                    state = 2;
                    rec[1]=buf[0];
                }
                else if(buf[0] != 0x7e) state = 0;
            }
            else if(state==2){
                if(buf[0] == 0x05 || buf[0] == 0x25 || buf[0] == 0x01 || buf[0] == 0x21){
                    state = 3;
                    rec[2] = buf[0];
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state==3){
                if(buf[0] == (rec[1]^rec[2])){
                    state = 4;
                    rec[3] = buf[0];
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state==4){
                if(buf[0]==0x7e){
                    state = 5;
                    rec[4]=buf[0];
                }
                else state = 0;
            }
            else{
                perror("LLWRITE STATE MACHINE");
                state = 0;
            }
        }


        if(nS==0){
            if(rec[2]==0x21){
                done = TRUE;
                nS = 1;
            }
            else if(rec[2]==0x05){
                state = 0;
                times = timeoutLim;
            }
            else{
                state = 0;
                times--;
            }
        }
        else if(nS==1){
            if(rec[2]==0x01){
                done = TRUE;
                nS = 0;
            }
            else if(rec[2]==0x25){
                state = 0;
                times = timeoutLim;
            }
            else{
                state = 0;
                times--;
            }
        }
        else {
            perror("NUMSEND");
            return -1;
        }
    }

    return bufSize;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    if(packet==NULL) return -1;
    int frameLength = 4, n = 0, state = 0, res = 0, done = FALSE, times = timeoutLim;
    unsigned char rec[MAX_SIZE], buf[5];

    while(!done){
        state = 0;
        res = 0;
        printf("vou entrar na state machine de ler xis de\n");
        while(state != 6){
            res = read(fd,buf,1);
            if(res==0){
                state = 0;
                times--;
                if(times<=0){
                    perror("TIMEOUT LLREAD");
                    return -1;
                }
                printf("read time out\n");
            }
            //else printf("byte ");
            //ele está a receber os bytes mas n está a sair desta state machine acho eu

            if(state==0){
                if(buf[0]==0x7e){
                    state = 1;
                    rec[0]=buf[0];
                }
            }
            else if(state==1){
                if(buf[0]==0x01){
                    state = 2;
                    rec[1]=buf[0];
                }
                else if(buf[0] != 0x7e) state = 0;
            }
            else if(state==2){
                if(buf[0] == 0x00 || buf[0] == 0x01 || buf[0] == 0x03 || buf[0] ==0x07 || buf[0] ==0x0b){
                    state = 3;
                    rec[2] = buf[0];
                }
                else if(buf[0] == 0x7e){
                    state = 1;
                } else{
                    state = 0;
                }
            }
            else if(state==3){
                if(buf[0]==(rec[1]^rec[2])){
                    if(rec[2] == 0x03 || rec[2] == 0x07 || rec[2] == 0x0b){
                        state = 5; //BCC_OTHER
                    } else{
                        state = 4; //BCC1
                    }
                    frameLength = 4;
                    rec[3] = buf[0];
                }
                else if(buf[0] == 0x7e){
                    state = 1;
                } else{
                    state = 0;
                }
            }
            else if(state==4){
                rec[frameLength]=buf[0];
                frameLength++;
                if(buf[0]==0x7e){
                    state = 6;
                }
            }
            else if(state==5){
                if(buf[0]==0x7e){
                    state = 6;
                    rec[frameLength] = buf[0];
                    frameLength++;
                }
                else state = 0;
            }
            else{
                perror("LLREAD STATE MACHINE");
                state = 0;
            }
        }
        printf("\nsai da state machine de ler, a informacao apos o coiso e: %s\n",rec);

        if(rec[2]==0x03 || rec[2] == 0x07 || rec[2]==0x0b){

            if(rec[2]==0x03){
                if(wFlag(&fd,0x01,0x07)!=0) return -1;
            }
            if(rec[2]==0x0b){
                n = -1;
                printf("li o n=-1 wtf?\n");
            }
            done = TRUE;
            
        }
        else{
            frameLength = destuff(rec,frameLength);
            unsigned char bcc2 = dataBcc(rec,frameLength);

            printf("\n dei destuff? sou mesmo fixe\n");

            if(bcc2 == rec[frameLength-2]){
                printf("uau o bcc2 estava bem que epico\n");
                if(rec[2]==0x00 && nR == 0){
                    wFlag(&fd,0x01,0x21);
                    nR=1;
                    for(int i = 0; i < frameLength-6; i++) {packet[i]=rec[i+4];n++;}
                    done = TRUE;
                }
                else if(rec[2]==0x02 && nR==1){
                    wFlag(&fd,0x01,0x01);
                    nR=0;
                    for(int i = 0; i < frameLength-6; i++) {packet[i] = rec[i+4];n++;}
                    done = TRUE;
                }
                else if(rec[2] == 0x00 && nR == 1) wFlag(&fd,0x01,0x21);
                else if(rec[2] == 0x01 && nR == 0) wFlag(&fd,0x01,0x01);
                else {
                    perror("BCC2 ERROR LLREAD");
                    return -1;
                }
                printf("packet momento é %s\n",packet);
                n = frameLength-6;
            }
            else{
                printf("o bcc2 estava mal que cringe\n");
                if(rec[2]==0x00 && nR==0) wFlag(&fd,0x01,0x05);
                else if(rec[2]==0x02 && nR==1) wFlag(&fd,0x01,0x25);
                else if(rec[2]==0x00 && nR==1) wFlag(&fd,0x01,0x21);
                else if(rec[2]==0x02 && nR==0) wFlag(&fd,0x01,0x01);
                else return -1;
            }
        }

    }
    return n;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    int state = 0, res = 0, times = timeoutLim;
    unsigned char rec[MAX_SIZE], buf[5];
    printf("\nmfw entrei no llclose\n");
    if(role==1) {
        if(wFlag(&fd,0x01,0x0b)) return -1;
        while(state!=5){
            res = read(fd,buf,1);
            if(res==0){//timeout
                times--;
                state = 0;
                if(times<=0){
                    perror("TIMEOUT LLCLOSE");
                    return -1;
                }
                if(wFlag(&fd,0x01,0x0b) != 0) return -1;
            }

            if(state==0){
                if(buf[0]==0x7e){
                    state=1;
                    rec[0]=buf[0];
                }
            }
            else if(state==1){
                if(buf[0]==0x03){
                    state = 2;
                    rec[1]=buf[0];
                }
                else if(buf[0]!=0x7e) state = 0;
            }
            else if(state==2){
                if(buf[0]==0x0b){
                    state = 3;
                    rec[2]=buf[0];
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state==3){
                if(buf[0] == (rec[1]^rec[2])){
                    rec[3] = buf[0];
                    state = 4;
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state==4){
                if(buf[0]==0x7e){
                    state = 5;
                    rec[4] = buf[0];
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else {
                perror("State machine invalid state");
                state = 0;
            }
        }
        if(wFlag(&fd,0x03,0x07));
    }
    else if(role == -1){
        while(state != 5){
            res = read(fd,buf,1);
            if(res == 0){
                times--;
                if(times<=0){
                    perror("TIMEOUT");
                    return -1;
                }
            }


            if(state == 0){
                if(buf[0] == 0x7e) state = 1;
                rec[0] = buf[0];
            }
            else if(state == 1){
                if(buf[0] == 0x01){
                    state = 2;
                    rec[1] = buf[0];
                }
                else if(buf[0] != 0x7e) state = 0;
            }
            else if(state == 2){
                if(buf[0] == 0x0b){
                    rec[2] = buf[0];
                    state = 3;
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state == 3){
                if(buf[0] == (rec[1]^rec[2])){
                    rec[3] = buf[0];
                    state = 4;
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state == 4){
                if(buf[0] == 0x7e){
                    rec[4] = buf[0];
                    state = 5;
                }
                else state = 0;
            }
            else{
                perror("State machine 1 invalid state");
                state = 0;
            }
        }
        if(wFlag(&fd,0x03,0x0b)) return -1;
        state = 0;
        res = 0;
        times = timeoutLim;
        while(state != 6){
            res = read(fd,buf,1);
            if(res == 0){
                times--;
                if(times<=0){
                    perror("TIMEOUT");
                    return -1;
                }
                if(wFlag(&fd,0x03,0x0b) != 0) return -1;
            }

            if(state == 0){
                if(buf[0] == 0x7e) state = 1;
                rec[0] = 0x7e;
            }
            else if(state == 1){
                if(buf[0] == 0x03){
                    state = 2;
                    rec[1] = buf[0];
                }
                else if(buf[0] != 0x7e) state = 0;
            }
            else if(state == 2){
                if(buf[0] == 0x07 || buf[0] == 0x0b){
                    rec[2] = buf[0];
                    state = 3;
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state == 3){
                if(buf[0] == (rec[1]^rec[2])){
                    rec[3] = buf[0];
                    if(rec[2] == 0x0b) state = 5;
                    else state = 4;
                }
                else if(buf[0] == 0x7e) state = 1;
                else state = 0;
            }
            else if(state == 4){
                if(buf[0] == 0x7e){
                    rec[4] = buf[0];
                    state = 6;
                }
                else state = 0;
            }
            else if(state == 5){
                if(buf[0] == 0x7e){
                    rec[4] = buf[0];
                    state = 0;
                    if(wFlag(&fd,0x03,0x0b) != 0) return -1;
                }
                else state = 0;
            }
            else{
                perror("State machine 2 invalid state");
                state = 0;
            }
        }
    }

    sleep(1);

    if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("LLCLOSE TCSETATTR");
        exit(-1);
    }

    close(fd);

    return 0;
}
