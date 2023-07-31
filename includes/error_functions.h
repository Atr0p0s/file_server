#ifndef ERROR_FUNCTIONS_H
#define ERROR_FUNCTIONS_H

#ifdef __GNUC__
    #define NORETURN __attribute__ ((__noreturn__))
#else
    #define NORETURN
#endif

void errExit(char* msg) NORETURN;
void errExitEN(int errNum, char* msg) NORETURN;

#endif /* ERROR_FUNCTIONS_H */