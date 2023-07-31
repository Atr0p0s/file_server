/**
 * @brief This library of functions to perform tasks commonly required for Internet domain sockets.
 * Since these functions employ the protocol-independent getaddrinfo() and getnameinfo()
 * functions, they can be used with both IPv4 and IPv6.
 */

#ifndef INET_SOCKETS_H
#define INET_SOCKETS_H

#include <sys/socket.h>
#include <netdb.h>
#include "gen_hdr.h"

/**
 * @brief Suggested length for string buffer that caller should pass to inetAddressStr(). 
 * Must be greater than (NI_MAXHOST + NI_MAXSERV + 4).
 */
//#define ADDRSTRLEN (NI_MAXHOST + NI_MAXSERV + 10)
#define IS_ADDR_STR_LEN 4096


/**
 * @brief Many of the functions in this library have similar arguments:
 * @param host argument is a string containing either a hostname or a numeric address (in IPv4 
 * dotted-decimal, or IPv6 hex-string notation). Alternatively, host can be specified as a NULL
 * pointer to indicate that the loopback IP address is to be used.
 * @param service argument is either a service name or a port number specified as a decimal string.
 * @param type argument is a socket type, specified as either SOCK_STREAM or SOCK_DGRAM.
 */

/**
 * @brief Func inetConnect() creats socket with type <type> and connect it to the address
 * specified by <host> and <service>. The function is designed for TCP or UDP clients. that
 * need to connect their socket to a server socket.
 * @returns a file descriptor on success, or -1 on error.
 */
int inetConnect(const char* host, const char* service, int type);

/**
 * @brief The inetBind() function creates a socket of the given type, bound to the wildcard
 * IP address on the port specified by service and type. (The socket type indicates whether 
 * this is a TCP or UDP service.) This function is designed (primarily) for UDP servers and 
 * clients to create a socket bound to a specific address.
 * @param addrlen If addrlen is specified as a non-NULL pointer, then the location it points to
 * is used to return the size of the socket address structure corresponding to the returned file 
 * descriptor. This value allows us to allocate a socket address buffer of the appropriate size
 * to be passed to a later accept() call if we want to obtain the address of a connecting client.
 * @returns a file descriptor on success, or -1 on error.
 */
int inetBind(const char* service, int type, socklen_t* addrlen);

/**
 * @brief The inetListen() function creates a listening stream (SOCK_STREAM) socket bound
 * to the wildcard IP address on the TCP port specified by <service>. This function is designed 
 * for use by TCP servers.
 * @param backlog argument specifies the permitted backlog of pending connections (as for listen()).
 * @returns a file descriptor on success, or -1 on error.
 */
int inetListen(const char* service, int backlog, socklen_t* addrlen);

/**
 * @brief As with inetListen(), inetBind() returns the length of the associated socket address 
 * structure for this socket in the location pointed to by addrlen. This is useful if we want 
 * to allocate a buffer to pass to recvfrom() in order to obtain the address of the socket 
 * sending a datagram.
 */


/**
 * @brief The inetAddressStr() function converts an Internet socket address to printable form.
 * @param addr is a socket address structure.
 * @param addrlen the length of socket address structure of <addr>.
 * @param addrStr is a buffer where string will returned.
 * @param addrStrLen the size of <addrStr> buffer. If the returned string would exceed 
 * (addrStrLen – 1) bytes, it is truncated. The constant IS_ADDR_STR_LEN defines a suggested size
 * for the <addrStr> buffer that should be large enough to handle all possible return strings.
 * @returns pointer to <addrStr>, a string containing host and service name as: (hostname, port-number).
 */
char* inetAddressStr(const struct sockaddr* addr, socklen_t addrlen, char* addrStr, int addrStrLen);

#endif /* INET_SOCKETS_H */