#include <netinet/in.h>
#include <arpa/inet.h>
#include "inet_sockets.h"

int inetConnect(const char* host, const char* service, int type) {
    int sfd;                            /* Socket fd */
    int s;                              /* Save returned functions' value */
    struct addrinfo hints;
    struct addrinfo *result = NULL;

    /* Call getaddrinfo(), to obtain list of addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
    hints.ai_socktype = type;
    s = getaddrinfo(host, service, &hints, &result);
    if (s != 0) {
        errno = ENOSYS;
        return -1;
    }

    /* Find address in the list, that we can successfully use */
    for (; result != NULL; result = result->ai_next) {
        sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (-1 == sfd) {
            continue;                   /* On error, try next address */
        }
        if (connect(sfd, result->ai_addr, result->ai_addrlen) != -1) {
            break;                      /* Success */
        }
        /* Connect failed: close this socket and try next address */
        close(sfd);
    }

    freeaddrinfo(result);
    return (NULL == result) ? -1 : sfd;
}

static int inetPassiveSocket(const char* service, int type, socklen_t* addrlen, boolean doListen, int backlog) {
    int sfd;                            /* Socket fd */
    int s;                              /* Save returned functions' value */
    int optval;                         /* Set SO_REUSEADDR for socket */
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    
    /* Call getaddrinfo(), to obtain list of addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
    hints.ai_socktype = type;
    hints.ai_flags = AI_PASSIVE;        /* Universal IP */
    s = getaddrinfo(NULL, service, &hints, &result);
    if (s != 0) {
        errno = ENOSYS;
        return -1;
    }

    /* Find address in the list, that we can successfully use */
    optval = 1;
    for (; result != NULL; result = result->ai_next) {
        sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (-1 == sfd) {
            continue;                   /* On error, try next address */
        }

        if (doListen) {
            if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
                close(sfd);
                freeaddrinfo(result);
                return -1;
            }
        }

        if (bind(sfd, result->ai_addr, result->ai_addrlen) == 0) {
            break;                      /* Success */
        }
        
        close(sfd);                     /* bind() failed; close this socket and try next address */
    }

    if (doListen && NULL != result) {
        if (listen(sfd, backlog) == -1) {
            freeaddrinfo(result);
            return -1;
        }
    }

    /* Return size of address struct */
    if (NULL != result && NULL != addrlen) {
        *addrlen = result->ai_addrlen;
    }

    freeaddrinfo(result);
    return (NULL == result) ? -1 : sfd;
}

int inetBind(const char* service, int type, socklen_t* addrlen) {
    return inetPassiveSocket(service, type, addrlen, FALSE, 0);
}

int inetListen(const char* service, int backlog, socklen_t* addrlen) {
    return inetPassiveSocket(service, SOCK_STREAM, addrlen, TRUE, backlog);
}

char* inetAddressStr(const struct sockaddr* addr, socklen_t addrLen, char* addrStr, int addrStrLen) {
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    if (getnameinfo(addr, addrLen, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV) == 0) {
        snprintf(addrStr, addrStrLen, "(%s, %s)", host, service);
    }
    else {
        snprintf(addrStr, addrStrLen, "(?UNKNOWN?)");
    }
    addrStr[addrStrLen-1] = '\0';
    return addrStr;
}