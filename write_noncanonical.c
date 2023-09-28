// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

volatile int STOP = FALSE;

int alarmActive = FALSE;
int alarmCount = 0;

void alarmHandler(){
    alarmActive = FALSE;
    alarmCount++;

    printf("Alarm %d\n", alarmCount);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 1 chars received

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

    // Create string to send
    unsigned char wbuf[5] = {0}, buf[2] = {0};

   
   
    wbuf[0] = 0x7e;
    wbuf[1] = 0x03;
    wbuf[2] = 0x03;
    wbuf[3] = wbuf[1]^wbuf[2];
    wbuf[4] = 0x7e;
   
    /*for (int i = 5; i < BUF_SIZE; i++)
    {
        buf[i] = 'a' + i % 26;
    }*/
   
    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    (void)signal(SIGALRM, alarmHandler);


    


    int state = 0; // 0 = start, 1 = flag_RCV, 2 = a_RCV, 3 = c_RCV, 4 = BCC, 5 = stop

    int other_a, other_c;

    int bytes;


    while(state!=5 && alarmCount < 4){
        
        
        if(!alarmActive){
            alarmActive = TRUE;
            alarm(1);
            state = 0;
            bytes = write(fd, wbuf, 5);
            printf("%d bytes written\n", bytes);
        }


        
        // Read bytes sent by other computer as answer
        bytes = read(fd, buf, 1);
        /*
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
        */


    }

    printf("State %d\t alarmCount %d\n", state, alarmCount);

    alarm(0);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}