#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include "gen_hdr.h"

/* SIGCHLD handler, destroy child process */
void silverBullet(int sig);

/* Send server's root folder */
void sendRoot(int socket);

/* Build and send file list to the client*/
void sendFileList(int socket);

/* Send file to the client */
void sendFile(int socket);

/* Сhooses an operation depending on the recived code.*/
void connectionHandler(int socket);

/* Send string length and then send that number of the bytes from the string */
int8_t sendstr(int sockfd, void* buffer, int flags);

/* Receive string length and then receive that number of the bytes from the string */
int8_t recvstr(int sockfd, void* buffer, int flags);

/* Calculate crc32 checksum of the file */
uint32_t crc32(unsigned char *data);

/* Call crc32() and send result to the client */
void sendChecksum(int socket);

#endif /* FILE_SERVER_H */