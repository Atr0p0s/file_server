#ifndef FILE_CLIENT_H
#define FILE_CLIENT_H

#include "gen_hdr.h"

/* codes of keys to decode read values. Uses by keyPressingHandler() function */
#define PRESSED 0x01
#define RELEASED 0x00
#define LONG_PRESS 0x02
#define TYPE 20
#define CODE 21
#define VALUE 22

#define FILE_NAME_MAX 256               /* Max file name */
#define FILE_PATH_MAX 4096              /* Max file path */
#define DIRECTORY_PATH_MAX FILE_PATH_MAX - FILE_NAME_MAX - 1
#define MAX_FILES 256

#define START_LINE 4

/* Structure to save file name and size*/
typedef struct fileStat {
    off_t size;
    char name[FILE_NAME_MAX];
} fileStat_t;

/* Get the server's root directory and then call getFileList() function */
void getRoot();

/*  Download file list and display it. If it is called not from getRoot() function - 
    arguments should be (FALSE, 0) */
void getFileList(int sockfd, boolean fromGetRoot);

/* Downloads a file from server */
void getFile(uint8_t fileIndex);

/* Handles button presses */
void keyPressingHandler();

/*  Depending on the member of the list (directory or file), /
    choose operation (receive file list or file) */
void operationSelector(uint8_t* fileIndex);

/* Get checksum(crc32) of the file from the server and check it with the checksum of local file */
void compareChecksum(uint8_t fileIndex);

#endif /* FILE_CLIENT_H */