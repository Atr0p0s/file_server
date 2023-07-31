#include <sys/socket.h>
#include "client_tools.h"

extern struct termios userTermios;

void displaySize(off_t size) {
    const char array[] = {'K', 'K', 'M', 'G', 'T'};
    uint8_t i = 0;
    if (size < 1000) {
        size = 1;
    } else {
        for (i = 0; size >= 1000; i++) {
            size /= 1000;
        }
    }
    printf(CSI"29G"CSI"K %3ld %cB\r\n", size, array[i]);
}

uint32_t crc32(unsigned char *data) {
   register uint32_t i;
   register uint8_t j;
   register uint32_t crc, byte, mask;

   i = 0;
   crc = 0xFFFFFFFF;
   while (data[i] != 0) {
      byte = data[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 0; j < 8; j++) {    // Do eight times.
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      i = i + 1;
   }
   return ~crc;
}

/* Displays % of downloaded file. */
static void progressBar(size_t downloaded, size_t size) {
    char* buf;
    printf(CSI"s");
    printf(CSI"38G[..........] 0%%");
    if (downloaded == size) {
        printf(CSI"38G"CSI"KDownloaded");
    } else {
        uint8_t progress = downloaded*100 / size;
        printf(CSI"39G");
        for (uint8_t i = 0; i < progress/10; i++) {
            printf("#");
        }
        printf(CSI"45G %d%%", progress);
    }
    printf(CSI"u");
}

ssize_t readn(int fd, void *buffer, size_t size) {
    ssize_t numRead;                /* Bytes that we read from the last call of read() */
    size_t totRead;                 /* Number of bytes that we have read at the moment */
    char *buf;
    buf = buffer;                   /* Avoid ariphmetic with "void *" */
    for (totRead = 0; totRead < size; ) {
        numRead = read(fd, buf, size - totRead);
        if (numRead > 0) {
            totRead += numRead;
            buf += numRead;
            progressBar(totRead, size);
        } else if (numRead == 0) {          /* End of file */
            return totRead;                 /* It could be 0, if it is the first call of read() */
        } else {                            /* Error */
            if (errno == EINTR) {
                continue;                   /* Interrupted --> recall read() */
            }
            else {
                printf(CSI"s");
                printf(CSI"38G"CSI"KDOWNLOAD ERROR");
                printf(CSI"u");
                return -1;                  /* Some other error */
            }
        }
    }
    return totRead;                 /* Должно равняться 'size' байтам, если сюда добрались */
}

int8_t sendstr(int sockfd, void* buffer, int flags) {
    char* buf = buffer;
    uint16_t bytes_to_transit;
    uint8_t size_of_bytes_to_transit = sizeof(bytes_to_transit);
    bytes_to_transit = strlen(buf)+1;

    if (send(sockfd, &bytes_to_transit, size_of_bytes_to_transit, MSG_MORE) == -1) {
        return -1;
    }
    if (send(sockfd, buf, bytes_to_transit, flags) == -1) {
        return -1;
    }
    return 0;
}

int8_t recvstr(int sockfd, void* buffer, int flags) {
    char* buf = buffer;
    uint16_t bytes_to_transit;
    uint8_t size_of_bytes_to_transit = sizeof(bytes_to_transit);

    if (recv(sockfd, &bytes_to_transit, size_of_bytes_to_transit, MSG_WAITALL) == -1) {
        return -1;
    }
    if (recv(sockfd, buf, bytes_to_transit, flags | MSG_WAITALL) == -1) {
        return -1;
    }
    return 0;
}

// ssize_t writen(int fd, const void *buffer, size_t n) {
//     ssize_t numWritten;             /*  Количество байтов, записанных предыдущей 
//                                         операцией write() */
//     size_t totWritten;              /* Общее количество байтов, записанных на данный момент */
//     const char *buf;
//     buf = buffer;                   /* Избегаем арифметики с указателями для "void *" */
//     for (totWritten = 0; totWritten < n; ) {
//         numWritten = write(fd, buf, n - totWritten);
//         if (numWritten <= 0) {
//             if (numWritten == -1 && errno == EINTR)
//                 continue;           /* Прервано --> перезапускаем write() */
//             else {
//                 return -1;          /* Какая-то другая ошибка */
//             }
//         }
//         totWritten += numWritten;
//         buf += numWritten;
//     }
//     return totWritten;              /* Должно равняться 'n' байтам, если сюда добрались */
// }

/* General handler: restore tty settings and exit */
static void handler(int sig) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &userTermios) == -1) {
        errExit("tcsetattr");
    }
    printf(CSI"?25h");                      /* Restore default cursor */
    printf(CSI"?1049l");                    /* Disable alternative screen buffer */
    _exit(EXIT_SUCCESS);
}

/* Handler for SIGTSTP */
static void tstpHandler(int sig) {
    struct termios ourTermios;              /* To save our tty settings */
    sigset_t tstpMask, prevMask;
    struct sigaction sa;
    int savedErrno;

    savedErrno = errno;                     /* We might change 'errno' here */
    /*  Save current terminal settings, restore terminal to state 
        at time of program startup */
    if (tcgetattr(STDIN_FILENO, &ourTermios) == -1) {
        errExit("tcgetattr");
    }
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &userTermios) == -1) {
        errExit("tcsetattr");
    }
    printf(CSI"?25h");                      /* Restore default cursor */
    printf(CSI"?1049l");                    /* Disable alternative screen buffer */

    /*  Set the disposition of SIGTSTP to the default, raise the signal
        once more, and then unblock it so that we actually stop */
    if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
        errExit("signal");
    }
    raise(SIGTSTP);
    sigemptyset(&tstpMask);
    sigaddset(&tstpMask, SIGTSTP);
    if (sigprocmask(SIG_UNBLOCK, &tstpMask, &prevMask) == -1) {
        errExit("sigprocmask");
    }

    /* Execution resumes here after SIGCONT */

    printf(CSI"?25l");                                          /* Disable cursor */
    printf(CSI"?1049h");                       /* Enable alternative screen buffer */
    if (sigprocmask(SIG_SETMASK, &prevMask, NULL) == -1) {      /* Reblock SIGTSTP */
        errExit("sigprocmask");
    } 
    sigemptyset(&sa.sa_mask);                                   /* Reestablish handler */
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = tstpHandler;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        errExit("sigaction");
    }
    /*  The user may have changed the terminal settings while we were
        stopped; save the settings so we can restore them later */
    if (tcgetattr(STDIN_FILENO, &userTermios) == -1) {
        errExit("tcgetattr");
    }
    /* Restore our terminal settings */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ourTermios) == -1) {
        errExit("tcsetattr");
    }
    errno = savedErrno;
}

int setSignalsHandlers(void) {
    struct sigaction sa, prev;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = handler;

    /* Set handler for SIGQUIT signal */
    if (sigaction(SIGQUIT, NULL, &prev) == -1) {
        return -1;
    }
    if (prev.sa_handler != SIG_IGN) {
        if (sigaction(SIGQUIT, &sa, NULL) == -1) {
            return -1;
        }
    }
    /* Set handler for SIGINT signal */
    if (sigaction(SIGINT, NULL, &prev) == -1) {
        return -1;
    }
    if (prev.sa_handler != SIG_IGN) {
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            return -1;
        }
    }
    /* Set handler for SIGTERM signal */
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        return -1;
    }
    /* Set handler for SIGTSTP signal */
    sa.sa_handler = tstpHandler;
    if (sigaction(SIGTSTP, NULL, &prev) == -1) {
        return -1;
    }
    if (prev.sa_handler != SIG_IGN) {
        if (sigaction(SIGTSTP, &sa, NULL) == -1) {
            return -1;
        }
    }

    return 0;
}

int ttySetRaw(int fd, struct termios* prevTermios) {
    struct termios t;

    if (tcgetattr(fd, &t) == -1) {
        return -1;
    }

    if (prevTermios != NULL) {
        *prevTermios = t;
    }

    t.c_lflag &= ~(ICANON | IEXTEN | ECHO);
                /*  Noncanonical mode, disable extended
                    input processing, and echoing */
    t.c_lflag |= ISIG;              /* enable signals */
    t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                    INPCK | ISTRIP | IXON | PARMRK);
                /*  Disable special handling of CR, NL, and BREAK.
                    No 8th-bit stripping or parity error handling.
                    Disable START/STOP output flow control. */
    t.c_oflag &= ~OPOST;            /* Disable all output processing */
    t.c_cc[VMIN] = 3;               /* Read up to 3 chars to handle such keys as KEY_UP */
    t.c_cc[VTIME] = 1;              /* Wait up to 0.1 sec for the next char */

    if (tcsetattr(fd, TCSAFLUSH, &t) == -1) {
        return -1;
    }

    return 0;
}