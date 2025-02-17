// Read from serial port in non-canonical mode
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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

volatile int STOP = FALSE;

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

    // Open serial port device for reading and writing and not as controlling tty
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

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char

    int state = 0;
    int other_a, other_c;

    while(state != 5){
    
    // Read bytes sent by other computer as answer
        int bytes = read(fd, buf, 1);
        printf("%d\n", buf[0]);
        buf[1] = '\0';
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
    
    buf[0]=0x7e;
    buf[1]=0x01;
    buf[2]=0x07;
    buf[3]=0x01 ^ 0x07;
    buf[4]=0x7e;
   
    int bytes = write(fd, buf, BUF_SIZE);
    
    /*
    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
        if(buf[1]^buf[2] != buf[3]) printf("Bruh");
        printf("var = 0x%02X\n", buf[0]);
        printf("var = 0x%02X\n", buf[1]);
        printf("var = 0x%02X\n", buf[2]);
        printf("var = 0x%02X\n", buf[3]);
        printf("var = 0x%02X\n", buf[4]);
        
        printf(":%s:%d\n", buf, bytes);
        if (buf[0] == 'z')
            STOP = TRUE;
            
        buf[0]=0x7e;
        buf[1]=0x01;
        buf[2]=0x07;
        buf[3]=0x01 ^ 0x07;
        buf[4]=0x7e;
       
        bytes = write(fd, buf, BUF_SIZE);
        printf("%d bytes written\n", bytes);
    }
    */

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
