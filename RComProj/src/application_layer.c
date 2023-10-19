// Application layer protocol implementation
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "application_layer.h"
#include "link_layer.h"


#define MAX_SIZE 1024

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

    LinkLayer connectionParameters;
    connectionParameters.baudRate=baudRate;
    connectionParameters.nRetransmissions = nTries;
    sprintf(connectionParameters.serialPort,"%s",serialPort);
    connectionParameters.timeout=timeout;
    if(strcmp(role,"tx")==0){
        connectionParameters.role=LlTx;
        if(llopen(connectionParameters)==-1){
            exit(1);
        }
        int file = open(filename,O_RDONLY);
        if(file<0)exit(1);
        unsigned char buf[MAX_SIZE];
        int writeRes = 0, bytesRead = 1;
        while(bytesRead>0){
            bytesRead = read(file,buf+1,MAX_SIZE-1);
            if(bytesRead<0){
                printf("Error reading bytes\n");
                break;
            }
            else if(bytesRead==0){
                buf[0]=0;
                llwrite(buf,1);
                printf("Done reading\n");
                break;
            }
            else{
                buf[0]=1;
                writeRes = llwrite(buf,bytesRead+1);
                if(writeRes<0) {
                    printf("Send to link layer error\n");
                    exit(1);
                }
            }
            sleep(1);
        }
        llclose(0);
        close(file);
    }
    else{
        connectionParameters.role=LlRx;
        if(llopen(connectionParameters)==-1) exit(1);
        int file=open(filename,O_RDWR|O_CREAT,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
        if(file<0)exit(1);
        int bytesRead = 0, writeRes = 0, totalBytes = 0;
        unsigned char buf[MAX_SIZE];
        while(bytesRead>=0){
            printf("calling llread\n");
            bytesRead=llread(buf);
            printf("\nbytes read: %d\n",bytesRead);
            if(bytesRead<0){
                printf("\nReceiving from link layer epic fail\n");
                exit(1);
            }
            else if(bytesRead>0){
                printf("application layer epically received %d bytes\nbyte 0 is %d\n", bytesRead, buf[0]);
                if(buf[0]==1){
                    writeRes=write(file,buf+1,bytesRead-1);
                    printf("done writing %d bytes\n",writeRes);
                    if(writeRes<0){
                        printf("\nError writing to file\n");
                        exit(1);
                    }
                    totalBytes += writeRes;
                }
                else if(buf[0] == 0){
                    printf("done\n");
                    break;
                }
            }
            
        }
        printf("llclose in receiver called\n");
        llclose(0);
        close(file);   
    }
}
