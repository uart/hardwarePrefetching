#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "log.h"

static int runtime_loglevel = 5;

int log_setlevel(int level)
{
	if(level > 5 || level < 1)return 0;
	runtime_loglevel = level;

	return level;
}

char * mergetags(char *t, char *f, int l)
{
	static char taggbuff[180];

	sprintf(taggbuff, "%s %s+%d", t, f, l);

	return taggbuff;
}

int loglevel(int level, char *tag, const char * format, ...)
{
	if(level > runtime_loglevel)return 0;

	time_t now = time(NULL);
	char * timestr = ctime(&now);
	timestr[strlen(timestr)-1] = '\0'; //remove newline

	printf("%d %s %s|", level, timestr, tag);
	va_list args;
	va_start (args, format);
	int r = vprintf (format, args);
	va_end (args);

	return r;
}

/*
#define TAG "TEST"

int main()
{
//	loglevel(1, TAG, "your usual message\n");
//	loglevel(2, TAG, "Testing numbers %d %f\n", 1,0.2);
	loga(TAG, "Testing numbers %d %f\n", 1,0.2);
	logd(TAG, "Debugtest %d\n", 1);

   return 0;
}*/
