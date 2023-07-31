#include "gen_hdr.h"
#include "ename.c.inc"

static void outputError(int err, boolean flushStdout, const char* msg) {
    #define BUF_SIZE 500
    char buf[BUF_SIZE];
    snprintf(buf, BUF_SIZE, "ERROR %s: [%s %s]\n", msg, ((err > 0 && err < MAX_ENAME) ? ename[err] : "?UNKNOWN?"), strerror(err));
    if (flushStdout)
    {
        fflush(stdout);
    }
    fputs(buf, stderr);
    fflush(stderr);
}

void errExit(char* msg) {
    outputError(errno, TRUE, msg);
    exit(EXIT_FAILURE);
}

void errExitEN(int errNum, char* msg) {
    outputError(errNum, TRUE, msg);
    exit(EXIT_FAILURE);
}