#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>

#include "log.h"

#define TAG "MAIN"

int main (int argc, char *argv[])
{
	log_setlevel(5);

	loga(TAG, "This is the main file for the UU Hardware Prefetch and Control project\n");

	return 0;
}
