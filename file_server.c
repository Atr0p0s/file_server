#include <signal.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include "inet_sockets.h"
#include "file_server.h"

//#define AVOIDSENDFILE     /* Call sendfile() doesn't work in shared with windows folder */

#define PORT_NUM "55550"
#define BACKLOG 5                                   /* Max connections */
#define FILE_NAME_MAX 256                           /* Max file name */
#define FILE_PATH_MAX 4096                          /* Max file path */
#define DIRECTORY_PATH_MAX FILE_PATH_MAX - FILE_NAME_MAX - 1
#define MAX_FILES 256

/* Commands that the server can handle */
#define GET_FILE_LIST 1
#define GET_FILE 2
#define GET_ROOT 3
#define CALCULATE_CRC32 4

/* Contains server's directory members */
typedef struct fileStat {
    off_t size;
    char name[FILE_NAME_MAX];
} fileStat_t;

const char* root;               /* Will contain root directory of the shared files s*/

void silverBullet(int sig) {
    int savedErrno;                         /* if errno will change */
    savedErrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        continue;
    }
    errno = savedErrno;
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

void sendChecksum(int socket) {
    int fd;
    char filePath[FILE_PATH_MAX];
    struct stat statBuf;
    uint32_t checksum;
    char *addr;

    /* Get the file path */
    if (recvstr(socket, filePath, 0) == -1) {
        errExit("recvstr");
    }

    /* Check if directory has correct name */
    if (strncmp(root, filePath, strlen(root)) != 0) {
        if (close(socket) == -1) {
            errExit("close socket");
        }
    }

    /* Open the file */
    fd = open(filePath, O_RDONLY);
    if (-1 == fd) {
        _exit(EXIT_FAILURE);
    }

    /* Get the file's size */
    if (stat(filePath, &statBuf) == -1) {
        errExit("statBuf");
    }

    addr = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        errExit("mmap");
    }

    checksum = crc32(addr);
    
    /* Send checksum */
    if (send(socket, &checksum, sizeof(checksum), 0) == -1) {
        errExit("send");
    }

    /* Remove a mapping from the calling process’s virtual address space */
    if (munmap(addr, statBuf.st_size) == -1) {
        errExit("munmap");
    }

    /* Close the file */
    if (close(fd) == -1) {
        errExit("close");
    }
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

void inline sendRoot(int socket) {
    if (sendstr(socket, (void*)root, 0) == -1) {
        errExit("sendstr");
    }
}

void sendFileList(int socket) {
    DIR* dirp;
    struct dirent* dp;
    struct stat statBuf;
    char filePath[FILE_PATH_MAX];
    char directory[DIRECTORY_PATH_MAX];
    fileStat_t files[MAX_FILES];
    uint8_t numberofFiles = 0;

    /* Take directory name, what content we will send */
    if (recvstr(socket, directory, 0) == -1) {
        errExit("recvstr");
    }

    /* Check if directory has correct name */
    if (strncmp(root, directory, strlen(root)) != 0) {
        if (close(socket) == -1) {
            errExit("close socket");
        }
    }

    /* Start sorting through the content */
    dirp = opendir(directory);
    if (NULL == dirp) {
        errExit("opendir");
    }
    /* First fill the list with directories */
    for (;;) {
        errno = 0;
        dp = readdir(dirp);
        if (NULL == dp) {
            break;
        }
        sprintf(filePath, "%s/%s", directory, dp->d_name);

        if (stat(filePath, &statBuf) == -1) {
            errExit("statBuf");
        }
        if ((statBuf.st_mode & S_IFDIR)) {         /* A directory*/
            if ( strcmp(dp->d_name, ".") == 0 || ((strcmp(dp->d_name, "..") == 0) && \
                    (strcmp(root, directory) == 0)) ) {
                continue;
            }
            if (strcmp(dp->d_name, "..") != 0 || (numberofFiles == 0)) {
                sprintf(files[numberofFiles].name, "%s", dp->d_name);
            } else {
                sprintf(files[numberofFiles].name, "%s", files[0].name);
                sprintf(files[0].name, "%s", dp->d_name);
            }
            files[numberofFiles].size = 0;
            numberofFiles++;
        }
    }
    if(0 != errno) {
        errExit("readdir");
    }

    rewinddir(dirp);
    /* Then fill the list with regular files */
    for (;;) {
        errno = 0;
        dp = readdir(dirp);
        if (NULL == dp) {
            break;
        }
        sprintf(filePath, "%s/%s", directory, dp->d_name);
        if (stat(filePath, &statBuf) == -1) {
            errExit("statBuf");
        }
        if (!(statBuf.st_mode & S_IFREG)) {         /* Not a regular file*/
            continue;
        }
        sprintf(files[numberofFiles].name, "%s", dp->d_name);
        files[numberofFiles].size = statBuf.st_size;
        numberofFiles++;
    }
    if (0 != errno) {
        errExit("readdir");
    }

    if (closedir(dirp) == -1) {
        printf("error on closedir");
    }
    /* Pack the list into a vector to reduce the number of transfers */
    struct iovec iov[numberofFiles];
    for (uint8_t i = 0; i < numberofFiles; i++) {
        iov[i].iov_base = files + i;
        iov[i].iov_len = sizeof(fileStat_t);
    }
    /* Send size of vector */
    if (send(socket, &numberofFiles, sizeof(numberofFiles), 0) == -1) {
        errExit("send");
    }
    /* Send vector of files list */
    if (writev(socket, iov, numberofFiles) == -1) {
        errExit("writev");
    }
}

void sendFile(int socket) {
    int fd;
    //char directory[DIRECTORY_PATH_MAX];
    char filePath[FILE_PATH_MAX];
    struct stat statBuf;
    /* Get the file path */
    if (recvstr(socket, filePath, 0) == -1) {
        errExit("recvstr");
    }
    /* Check if path has correct name */
    if (strncmp(root, filePath, strlen(root)) != 0) {
        if (close(socket) == -1) {
            errExit("close socket");
        }
    }
    /* Open the file */
    fd = open(filePath, O_RDONLY);
    if (-1 == fd) {
        _exit(EXIT_FAILURE);
    }
    /* Get the file's size */
    if (stat(filePath, &statBuf) == -1) {
        errExit("statBuf");
    }
    /* Send the file */
#ifndef AVOIDSENDFILE   /* sendfile might not work in shared library */
    off_t offset = 0;
    ssize_t sent = 0;
    size_t size_to_send = statBuf.st_size;
    for (; size_to_send > 0; ) {
        sent = sendfile(socket, fd, &offset, size_to_send);
        if (sent <= 0)                  // Error or end of file
        {
            if (sent != 0) {
                errExit("sendfile");    // Was an error, report it
            }
            break;                      // End of file
        }
        size_to_send -= sent;           // Decrease the send length by the amount actually sent
    }
#else
    char* fileDataBuf;
    fileDataBuf = (char*)malloc(statBuf.st_size);
    if (NULL == fileDataBuf) {
        errExit("malloc");
    }
    if (read(fd, fileDataBuf, statBuf.st_size) == -1) {
        errExit("read");
    }
    if (send(socket, fileDataBuf, statBuf.st_size, 0) == -1) {
        errExit("send");
    }
    free(fileDataBuf);
#endif
    /* Close the file */
    if (close(fd) == -1) {
        errExit("close");
    }
}

void connectionHandler(int socket) {
    uint8_t command;
    /* Checking the command to decide what to do next */
    if (recv(socket, &command, sizeof(command), 0) == -1) {
        errExit("recv");
    }
    switch(command) {
        case GET_ROOT:
            sendRoot(socket);      /* After sending root directory we will send root's file list */
        case GET_FILE_LIST:
            sendFileList(socket);
            break;
        
        case GET_FILE:
            sendFile(socket);
            break;
        
        case CALCULATE_CRC32:
            sendChecksum(socket);
            break;

        default:
            break;
    }
    /* Close the connection after operation */
    if (close(socket) == -1) {
        errExit("close");
    }
}

int main(int argc, char* argv[]) {
    int lfd, cfd;                       /* listening and connected sockets*/
    struct sigaction sa;
    socklen_t len;
    struct sockaddr_storage claddr;
    char address[IS_ADDR_STR_LEN];

    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0) {
            printf("By default, the server checks the files in the \"./server_files/\" folder. \
                To set another root directory, specify it as a program argument.\n");
            exit(EXIT_SUCCESS);
        } else {
            root = argv[1];
            printf("len: %ld. String: %s\n", strlen(root), root);
        }
    } else {
        root = "server_files/";
    }

    /* Ignore SIGPIPE signal if we write() to the closed pipe.
     * In this case write() ends with EPIPE error. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        errExit("signal");
    }

    /* If signal SIGCHLD from ended child proccess interrupt system call - we just restart it. */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = silverBullet;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        errExit("sigaction");
    }

    lfd = inetListen(PORT_NUM, BACKLOG, NULL);
    if (-1 == lfd) {
        errExit("inetListen");
    }
    printf("Waiting for connection...\n");
    for(;;) {
        len = sizeof(struct sockaddr_storage);
        //cfd = accept(lfd, NULL, NULL);
        cfd = accept(lfd, (struct sockaddr*)&claddr, &len);
        if (-1 == cfd) {
            errExit("accept");
        }
        inetAddressStr((struct sockaddr*)&claddr, len, address, IS_ADDR_STR_LEN);
        printf("Connection from %s\n", address);
        switch(fork()) {
            case 0:                     /* Children */
                close(lfd);
                connectionHandler(cfd);
                _exit(EXIT_SUCCESS);
            
            case -1:                    /* In case of error */
                printf("Can't create child\n");
                close(cfd);
                break;
            
            default:                    /* Parent */
                close(cfd);
                break;
        }
    }
    exit(EXIT_SUCCESS);
}