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


#define MAX_SIZE 512

int buildDataPacket(unsigned char* buf, unsigned char* data, int dataSize){
    buf[0] = 0x01;
    buf[1] = (dataSize / 256) * 256;
    buf[2] = dataSize % 256;
    for(int i = 0; i < dataSize; i++){
        buf[i+3] = data[i];
    }
    return dataSize + 3;
}

int readDataPacket(unsigned char* buf, int dataSize){
    for(int i = 3; i < (dataSize+3); i++){
        buf[i-3] = buf[i];
    }
    return dataSize-3;
}

int buildStartPacket(unsigned char* buf, long fileSize){
    buf[0] = 2;
    buf[1] = 0;
    char tmp[300] = {'\0'};
    sprintf(tmp,"%ld",fileSize);
    buf[2] = strlen(tmp);
    memcpy(buf+3,tmp,buf[2]);
    
    return buf[2]+3;
}

int readStartPacket(unsigned char* buf){
    return buf[2]+3;
}

int myStrnCmp(unsigned char* str1, unsigned char* str2, int n){
    for(int i = 0; i < n; i++){
        if(str1[i]!=str2[i]) return 1;
    }
    return 0;
}

void printBuf(unsigned char* buf, int size){
    for(int i = 0; i < size; i++) printf("%d",buf[i]);
    printf("\n");
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    //Time of Execution test
    clock_t start, end;
    double cpu_time_used;

    start = clock();
    
    LinkLayer connectionParameters;
    connectionParameters.baudRate=baudRate;
    connectionParameters.nRetransmissions = nTries;
    sprintf(connectionParameters.serialPort,"%s",serialPort);
    connectionParameters.timeout=timeout;
    long int totalBytes = 0;
    if(strcmp(role,"tx")==0){
        connectionParameters.role=LlTx;
        if(llopen(connectionParameters)==-1){
            exit(1);
        }
        int file = open(filename,O_RDONLY);
        if(file<0)exit(1);

        FILE *fp;
        fp = fopen(filename,"r");
        fseek(fp,0L,SEEK_END);
        long size = ftell(fp);
        //printf("tamanho epico %ld\n",size);

        unsigned char buf[MAX_SIZE], startPacket[MAX_SIZE], data[MAX_SIZE];
        size = buildStartPacket(startPacket,size);
        //printBuf(startPacket,size);
        //printf("startpacket[0] %d\n", startPacket[0]);

        llwrite(startPacket,size);

        startPacket[0] = 3;
        //printBuf(startPacket,size);
        //printf("startpacket[0] %d\n", startPacket[0]);
        //sleep(5);
        int writeRes = 0, bytesRead = 1;
        while(bytesRead>0){
            bytesRead = read(file,data,MAX_SIZE-3);
            if(bytesRead != 0) {
                totalBytes += bytesRead;
                bytesRead = buildDataPacket(buf,data,bytesRead);
                writeRes = llwrite(buf,bytesRead);
                if(writeRes<0) {
                    printf("Send to link layer error\n");
                    exit(1);
                }
            }
            else if (bytesRead < 0){
                printf("Error reading bytes\n");
                break;
            }
            else {
                llwrite(startPacket,size);
                printf("Done reading\n");
                break;
            }
            //for(int i = 1; i < bytesRead; i++) printf("%c",buf[i]);
            //printf("\n");
            //sleep(1);
        }
        llclose(0);
        close(file);
    }
    else{
        connectionParameters.role=LlRx;
        if(llopen(connectionParameters)==-1) exit(1);
        int file=open(filename,O_RDWR|O_CREAT,S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
        if(file<0)exit(1);
        int bytesRead = 0, writeRes = 0;
        unsigned char buf[MAX_SIZE*2], closer[MAX_SIZE*2] = {'\0'};

        //buildControlPacket(close,,2);
        
        int timesWritten = 0;

        int ctrlPacket = 0, ctrlSize = 0;
        if(llread(buf) == 0) bytesRead = -1;
        ctrlSize = readStartPacket(buf);
        ctrlPacket = 1;
        memcpy(closer,buf,ctrlSize);
        closer[0] = 3;
        
        while(ctrlPacket == 0){
            if(llread(buf) > 0){
                ctrlSize = readStartPacket(buf);
                //printBuf(buf, ctrlSize);
                if(buf[0]==2) {
                    ctrlPacket = 1;
                    memcpy(closer,buf,ctrlSize);
                    closer[0] = 3;
                    //printBuf(closer,ctrlSize);
                }
            }
            else sleep(1);
        }



        //sleep(5);

        while(bytesRead>=0){
            //printf("calling llread\n");
            bytesRead = llread(buf);
            //printBuf(buf,bytesRead);
            if(bytesRead == ctrlSize && myStrnCmp(closer,buf,ctrlSize) == 0){
                break;
            }
            bytesRead = readDataPacket(buf,bytesRead);
            //for(int i = 1; i < bytesRead; i++) printf("%c",buf[i]);
            //printf("\n");
            //printf("\nbytes read: %d\n",bytesRead);
            if(bytesRead<0){
                //printf("\nReceiving from link layer epic fail\n");
                exit(1);
            }
            else if(bytesRead>0){
            
                writeRes=write(file,buf,bytesRead);
                if(writeRes<0){
                    //printf("\nError writing to file\n");
                    exit(1);
                }
                timesWritten++;
                totalBytes += writeRes;
            
            }
        }
        printf("wrote %d times\nllclose in receiver called\n",timesWritten);
        llclose(0);
        close(file);   
    }

    //CLOCK END
    end = clock();
    cpu_time_used = ((double) (end - start) / CLOCKS_PER_SEC);
    printf("total bytes %ld\n",totalBytes);
    
}
