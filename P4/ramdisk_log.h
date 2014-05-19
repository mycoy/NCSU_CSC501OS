/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P4.
 *
 * Header for logger of ramdisk.
 */

#ifndef OSP4_LOG_DEBUG
#define OSP4_LOG_DEBUG 1
#endif

#define LEVEL_INFO 1
#define LEVEL_DEBUG 2
#define LEVEL_ERROR 3

typedef void * logger;

/* var-arg indicate the file to log if we want to log to a file */
logger create_logger(const int level, char * path);

int chat_log(const logger lger, const char * info);
int chat_log_level(const logger lger, const char * info, const int level);
int chat_log_chg_level(const logger lger, const int level);

void destroy_logger(logger lger);

