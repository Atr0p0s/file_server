#include <fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <libgen.h>
#include <sys/mman.h>
#include "inet_sockets.h"
#include "client_tools.h"
#include "file_client.h"
#include <ctype.h>
#define SERVER "localhost"
#define PORT_NUM "55550"

/* Commands that the server will handle */
#define GET_FILE_LIST 1
#define GET_FILE 2
#define GET_ROOT 3
#define GET_CHECKSUM 4

/* Keys that we will handle*/
#define KEY_Q 113
#define KEY_C 99
#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_ENTER 13
#define KEY_ESC 27

struct termios userTermios;                 /* To save terminal settings */

static const char* downloadsFolder;         /* Directory where we will save files */
static char directory[DIRECTORY_PATH_MAX];  /* Contains current server's directory */
static fileStat_t* files = NULL;            /* Contains server's directory members */
static uint8_t numberofFiles;               /* Contains number of server's directory members */

void compareChecksum(uint8_t fileIndex) {
    int socket;
    uint8_t command = GET_CHECKSUM;
    char serverFilePath[FILE_PATH_MAX];
    int fd;
    struct stat statBuf;
    uint32_t serverChecksum;
    uint32_t localChecksum;
    char *addr;
    char localFilePath[FILE_PATH_MAX];

    if (0 == files[fileIndex].size) {               /* It's a directory - skip it*/
        printf(CSI"s");
        printf(CSI"50GIt's a folder...");
        printf(CSI"u");
        return;
    }

    sprintf(localFilePath, "%s/%s", downloadsFolder, files[fileIndex].name);
    fd = open(localFilePath, O_RDONLY, 0);
    if (-1 == fd) { 
        if (errno == ENOENT) {                          /* If files doesn't exist -> download it */
            getFile(fileIndex);
            fd = open(localFilePath, O_RDONLY, 0);
            if (-1 == fd) { 
                errExit("open");
            }
        } else {
            errExit("open");
        }
    }

    /* Build a path to the server's file */
    sprintf(serverFilePath, "%s%s", directory, files[fileIndex].name);

    /* Connect with the server*/
    socket = inetConnect(SERVER, PORT_NUM, SOCK_STREAM);
    if (-1 == socket) {
        errExit("inetConnect");
    }

    /* Tell the server, that we want to get a file checksum */
    if (send(socket, &command, sizeof(command), MSG_MORE) == -1) {  
        errExit("send");
    }

    /* Tell the server the file name*/
    if (sendstr(socket, serverFilePath, 0) == -1) {
        errExit("sendstr");
    }
    /* Server calculate checksum and give us a result */
    if (recv(socket, &serverChecksum, sizeof(serverChecksum), 0) == -1) {
        errExit("send");
    }

    /* Close the connection */
    if (close(socket) == -1) {
        errExit("close");
    }

    /* Get the file's size */
    if (stat(localFilePath, &statBuf) == -1) {
        errExit("statBuf");
    }

    /* Create a mapping of the file (to calculate its checksum)*/
    addr = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED)
        errExit("mmap");

    localChecksum = crc32(addr);

    /* Remove a mapping from the calling process’s virtual address space */
    if (munmap(addr, statBuf.st_size) == -1)
        errExit("munmap");

    /* Close the file */
    if (close(fd) == -1) {
        errExit("close");
    }

    printf(CSI"s");
    if (localChecksum == serverChecksum) {
        printf(CSI"50GChecksum is OK");
    } else {
        printf(CSI"50GСhecksum does not match");
    }
    printf(CSI"u");
}

void getRoot() {
    int socket;
    uint8_t command = GET_ROOT;

    /* Connect with the server*/
    socket = inetConnect(SERVER, PORT_NUM, SOCK_STREAM);
    if (-1 == socket) {
        errExit("inetConnect");
    }

    /* Tell the server, that we want to know server's root */
    if (send(socket, &command, sizeof(command), 0) == -1) {
        errExit("send");
    }

    /* Send the directory whose contents we want to get */
    if (recvstr(socket, directory, 0) == -1) {
        errExit("sendstr");
    }

    getFileList(socket, TRUE);
}

void getFileList(int sockfd, boolean fromGetRoot) {
    int socket;
    char unnessessaryData[FILE_PATH_MAX];
    uint8_t command = GET_FILE_LIST;
    char* base;
    static char* directorytoPrint;

    /* If we already have fulled filed list - delete it */
    if (NULL != files) {
        free(files);
    }

    if (fromGetRoot) {          /* Calling only once - at start of program */
        socket = sockfd;
        /* Server's root folder will be displayed as "/" */
        directorytoPrint = directory + strlen(directory) - 1;
    } else {
        /* Connect with the server*/
        socket = inetConnect(SERVER, PORT_NUM, SOCK_STREAM);
        if (-1 == socket) {
            errExit("inetConnect");
        }

        /* Tell the server, that we want to get a (new) file list */
        if (send(socket, &command, sizeof(command), 0) == -1) {
            errExit("send");
        }
    }

    /* Send the directory whose contents we want to get */
    if (sendstr(socket, directory, 0) == -1) {
        errExit("sendstr");
    }

    /* Receive the number of the directory members */
    if (recv(socket, &numberofFiles, sizeof(numberofFiles), 0) == -1) {
        errExit("recv");
    }

    /* Create space for each member */
    files = malloc(sizeof(fileStat_t)*numberofFiles);
    if (NULL == files) {
        errExit("malloc");
    }

    printf(CSI"H"CSI"2J"CSI"3J");                   /* Clear the display */
    printf(CSI"3m  Directory: %s\r\n\n"CSI"0m", directorytoPrint);
    printf(CSI"1m  Name"CSI"30GSize"CSI"0m\r\n");   /* Print header */

    /* Receive the directory's members */
    struct iovec iov[numberofFiles];
    for (uint8_t i = 0; i < numberofFiles; i++) {
        iov[i].iov_base = files + i;
        iov[i].iov_len = sizeof(*files);
    }
    if (readv(socket, iov, numberofFiles) == -1) {
        errExit("readv");
    }
    for (uint8_t i = 0; i < numberofFiles; i++) {
        printf("  %s", files[i].name);
        if (0 == files[i].size) {
            if (strcmp(files[i].name, "..") != 0) {
                printf("/");
            }
            printf("\r\n");
        } else {
            displaySize(files[i].size);
        }
    }

    printf(CSI"4;1H>");         /* Set cursor position to the first member of the list */
    MOVE_LEFT

    /* Close the connection */
    if(-1 == close(socket)) {
        errExit ("close");
    }
}

void getFile(uint8_t fileIndex) {
    int socket;                         /* Socket fd */
    int fd, openFlags;
    mode_t filePerms;
    char* buf = NULL;                   /* Buffer to save file data*/
    uint8_t command = GET_FILE;         /* Command that will be send to the server */
    /* Buffer that will contain full path to the file for the client */
    char localFilePath[FILE_PATH_MAX];
    /* Buffer that will contain full path to the file for the server */
    char serverFilePath[FILE_PATH_MAX];
    /* Variables for changing a file name in case of duplicate*/
    uint16_t folderPathLength;
    uint8_t dotPosition;                /*  We'll save position of char '.' 
                                            if we have to change file name */
    char* dotAddr;                      /* Pointer to '.' address*/

    /* Buffer for saving file data from the server */
    buf = (char*)malloc(files[fileIndex].size);     
    if (NULL == buf) {
        errExit("malloc");
    }
    filePerms = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | 
                S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH; /* rwxrwxrwx */
    /* Make a dir if it isn't exist */
    if (mkdir(downloadsFolder, filePerms) == - 1) {
        if (errno != EEXIST) {
            errExit("mkdir");
        }
    }

    /* Create new file to save data */
    openFlags = O_CREAT | O_RDWR | O_EXCL;
    filePerms = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
                S_IROTH | S_IWOTH; /* rwxrw-rw- */
    sprintf(localFilePath, "%s/%s", downloadsFolder, files[fileIndex].name);
    fd = open(localFilePath, openFlags, filePerms);
    for (uint8_t i = 1; fd < 0; i++) {                          /* If we got error on opening */
        if (EEXIST == errno) {                                  /* File already exists */
            dotAddr = strchr(files[fileIndex].name, '.');       /* Find the first '.' in string */
            if (dotAddr != NULL) {
                folderPathLength = strlen(downloadsFolder);
                dotPosition = dotAddr - files[fileIndex].name;          /* Position of the '.' */
                sprintf(localFilePath, "%s", downloadsFolder);
                strncpy(localFilePath + folderPathLength, files[fileIndex].name, dotPosition);   
                sprintf(localFilePath + folderPathLength + dotPosition, "(%d)%s", i, dotAddr);
            } else {
                sprintf(localFilePath, "%s/%s(%d)", downloadsFolder, files[fileIndex].name, i);
            }
            errno = 0;
            fd = open(localFilePath, openFlags, filePerms);
        } else {                                                        /* Some other error*/
        //errExit("open");
        printf(CSI"s");
        printf(CSI"38GOPENING ERROR");
        printf(CSI"u");
        free(buf);
        return;
        }
    }

    /* Build a path to the server's file */
    sprintf(serverFilePath, "%s%s", directory, files[fileIndex].name);

    /* Connect with the server */
    socket = inetConnect(SERVER, PORT_NUM, SOCK_STREAM);
    if (-1 == socket) {
        errExit("inetConnect");
    }

    /* Tell the server, that we want to get a file */
    if (send(socket, &command, sizeof(command), MSG_MORE) == -1) {  
        errExit("send");
    }

    /* Tell the server what file we want to recieve */
    if (sendstr(socket, serverFilePath, 0) == -1) {
        errExit("sendstr");
    }

    /* Receive the file */
    if (readn(socket, buf, files[fileIndex].size) == -1) {
        errExit("readn");
    }

    /* Close connection */
    if (close(socket) == -1) {
        errExit("close");
    }

    /* Save the file from the buffer */
    if (write(fd, buf, files[fileIndex].size) == -1) {
        errExit("write");
    }

    free(buf);
    if (close(fd) == -1) {
        errExit("close");
    }
}

void operationSelector(uint8_t* fileIndex) {   
    if (0 == files[*fileIndex].size) {                   /* It's a directory -> get new file list*/
        if (strcmp(files[*fileIndex].name, "..") == 0) {
            sprintf(directory, "%s/", dirname(directory));
        } else {
            sprintf(directory, "%.*s%s/", DIRECTORY_PATH_MAX - FILE_NAME_MAX - 1, directory, files[*fileIndex].name);
        }
        *fileIndex = 0;
        getFileList(FALSE, 0);
    } else {                                            /* It's a file -> download it */
        getFile(*fileIndex);
    }
}

void keyPressingHandler() {
    char input[3];                                  /* Buffer for to save read a key code */
    ssize_t numRead;                                /* Number of read bytes */
    uint8_t fileIndex = 0;                          /* Number of selected file(line) */

    for (;;) {                                      /* Handle key pressing */
        input[2] = KEY_ESC;                         /* For correct ESC key handling*/
        numRead = read(STDIN_FILENO, &input, 3);
        if (numRead == -1) {
            printf("read error");
            break;
        }
        if (numRead == 0) {                         /* Can occur after terminal disconnect */
            break;
        }
        switch(input[0]) {
            case KEY_ESC:                           /* Some keys generate sequence of 3 chars, */
            switch(input[2]) {                      /* check the third to identify the key */
                case KEY_UP:
                if (fileIndex > 0) {
                    printf(" ");
                    MOVE_LEFT
                    MOVE_UP
                    printf(">");
                    MOVE_LEFT
                    fileIndex--;
                }
                break;

                case KEY_DOWN:
                if (fileIndex < (numberofFiles - 1)) {
                    printf(" ");
                    MOVE_LEFT
                    MOVE_DOWN
                    printf(">");
                    MOVE_LEFT
                    fileIndex++;
                }
                break;

                case KEY_ESC:
                return;
                break;
            }
            break;

            case KEY_ENTER:
            operationSelector(&fileIndex);
            break;

            case KEY_C:
            compareChecksum(fileIndex);
            break;

            case KEY_Q:
            return;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0) {
            printf("By default app stores donwloands in the \"./Downloads/\" folder. \
                To set another download directory, specify it as a program argument.\n");
            exit(EXIT_SUCCESS);
        } else {
            downloadsFolder = argv[1];
        }
    } else {
        downloadsFolder = "./Downloads/";
    }

    /* Set handlers for signals */
    if (setSignalsHandlers() == -1) {
        errExit("sigaction");
    }

    printf(CSI"?1049h");                        /* Enable alternative screen buffer */
    setbuf(stdout, NULL);                       /* Disable stdout buffering */
    printf(CSI"?25l");                          /* Disable cursor */

    /* Save terminal settings and switch it to raw mode */
    if (ttySetRaw(STDIN_FILENO, &userTermios) == -1) {
        errExit("ttySetRaw");
    }

    getRoot();                  /* Get the name of server's root folder and display its files */
    keyPressingHandler();

    printf(CSI"?25h");                                              /* Restore default cursor */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &userTermios) == -1) {   /* Restore terminal settings */
        errExit("tcsetattr");
    }
    printf(CSI"?1049l");                        /* Disable alternative screen buffer */

    exit(EXIT_SUCCESS);
}