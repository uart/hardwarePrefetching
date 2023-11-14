#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int log_setlevel(int level);
char * mergetags(char *t, char *f, int l);
int loglevel(int level, char *tag, const char * format, ...);

//ALERT 1
//ERROR 2
//INFO  3
//VERBO 4
//DEBUG 5

#define loga(T, ...) loglevel(1,T, __VA_ARGS__)
#define loge(T, ...) loglevel(2,T, __VA_ARGS__)
#define logi(T, ...) loglevel(3,T, __VA_ARGS__)
#define logv(T, ...) loglevel(4,T, __VA_ARGS__)
#define logd(T, ...) loglevel(5,mergetags(T,__FILE__,__LINE__),__VA_ARGS__)

#endif
