#ifndef CLIENT_TOOLS_H
#define CLIENT_TOOLS_H

#include <signal.h>
#include <termios.h>
#include "gen_hdr.h"

/* Control Sequence Introducer sequences (to move cursor in terminal) */
#define CSI "\e["                           
#define MOVE_UP printf(CSI"A");
#define MOVE_DOWN printf(CSI"B");
#define MOVE_LEFT printf(CSI"D");
//#define MOVE_RIGHT printf(CSI"C");

/*  The function installs the same handler for SIGQUIT, SIGINT and SIGTERM signals. The SIGTSTP 
    signal requires special treatment, so a different handler is installed for that signal. */
int setSignalsHandlers(void);

/*  Place terminal referred to by 'fd' in raw mode (noncanonical mode
    with all input and output processing disabled). Return 0 on success,
    or -1 on error. If 'prevTermios' is non-NULL, then use the buffer to
    which it points to return the previous terminal settings. */
 int ttySetRaw(int fd, struct termios* prevTermios);

/* Resumes system read() call if an interrupt occurs*/
ssize_t readn(int fd, void *buffer, size_t size);

/* Send string length and then send that number of the bytes from the string */
int8_t sendstr(int sockfd, void* buffer, int flags);

/* Receive string length and then receive that number of the bytes from the string */
int8_t recvstr(int sockfd, void* buffer, int flags);

/* Calculate crc32 checksum of the file */
uint32_t crc32(unsigned char *data);

/* Display size of the file */
void displaySize(off_t size);

#endif /* CLIENT_TOOLS_H */