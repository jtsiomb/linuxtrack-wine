#ifndef LOGGER_H_
#define LOGGER_H_

void init_logger(const char *logfile);
void shutdown_logger(void);
void logmsg(const char *fmt, ...);

#endif	/* LOGGER_H_ */
