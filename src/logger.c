#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "logger.h"

static FILE *fp;

void init_logger(const char *logfile)
{
	shutdown_logger();

	if(!(fp = fopen(logfile, "w"))) {
		fprintf(stderr, "failed to open logfile: %s: %s\n", logfile, strerror(errno));
		fprintf(stderr, "redirecting logging to standard error stream\n");
		fp = stderr;
	}
	setvbuf(fp, 0, _IONBF, 0);
}

void shutdown_logger(void)
{
	if(fp && fp != stdout && fp != stderr) {
		fclose(fp);
	}
}

void logmsg(const char *fmt, ...)
{
	va_list ap;

	if(!fp) fp = stderr;

	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
}
