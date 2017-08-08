#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_SYSLOG
#ifdef USE_SYSLOG
#include <syslog.h>

#define logg(LEVEL, FMT, ARGS...)   do { syslog (LEVEL, FMT, ##ARGS); } while (0)
#define logg_err(FMT, ARGS...)      do { syslog (LOG_ERR, "%s (%d):%s: " FMT, __FILE__, __LINE__, __FUNCTION__, ##ARGS); } while (0)
#define log_open(NAME)              do { openlog(NAME, LOG_CONS | LOG_PID, LOG_USER); logg(LOG_INFO, "started"); } while (0)
#define log_close(x)                do { logg(LOG_INFO, "stopped"); closelog(); } while (0)

#else
#define LOG_DEBUG	1
#define LOG_INFO	2
#define LOG_ERROR	3
#define log_open(NAME)		    do { logg(LOG_INFO, "%s started", NAME } while(0);
#define log_close()		    do { logg(LOG_INFO, "stopp logging" } while(0);
#define logg(LEVEL, FMT, ARGS...)   do{ printf(FMT "\n", ##ARGS); } while(0);
#define logg_err(FMT, ARGS...)      do { printf("%s:%s (%d): " FMT "\n", __FILE__, __FUNCTION__, __LINE__, ##ARGS); } while (0)
#endif

