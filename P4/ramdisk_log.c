/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P4.
 *
 * Functions for logger of ramdisk.
 */

#include "ramdisk_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_BUF_SIZE 512

typedef struct _logger_st {
	int log_level;
	FILE * to_file;
} _logger;

logger create_logger(const int level, char * path) {
	_logger * lger;
	char * file_path=NULL;

	if (level < LEVEL_INFO || level > LEVEL_ERROR) {
		return NULL;
	}

	lger = (_logger*) malloc(sizeof(_logger));
	if (lger == NULL) {
		return NULL;
	}
	lger->log_level = level;

	if(path!=NULL){
		file_path = path;
	}else{
		file_path = "./ramdisk_debug_log";
	}
	lger->to_file = fopen(file_path, "wb+");
	if (lger->to_file == NULL) {
			free(lger);
			return NULL;
	}

	return (logger) lger;
}

int chat_log(const logger lger, const char * info) {
	if (lger == NULL) {
		return -1;
	}

	return chat_log_level(lger, info, ( (_logger*)lger )->log_level );
}

int chat_log_level(const logger lger, const char * info, const int level) {
	_logger * logger = (_logger*) lger;
	char s[LOG_BUF_SIZE];
	memset(s, 0, sizeof(char) * LOG_BUF_SIZE);

	if (logger == NULL) {
		return -1;
	}

	if (level < LEVEL_INFO || level > LEVEL_ERROR) {
		/* printf("in chat_log_level: log_level(%d) error, to log info :%s\n", level, info); */
		return -1;
	}


	if (level == LEVEL_INFO)
		strcat(s, "LOG_INFO, ");
	if (level == LEVEL_DEBUG)
		strcat(s, "LOG_DEBUG, ");
	if (level == LEVEL_ERROR)
		strcat(s, "LOG_ERROE, ");

	strcat(s, info);
	strcat(s, "\n");
	fputs(s, logger->to_file);
	fflush( logger->to_file );

	return 0;
}

int chat_log_chg_level(const logger lger, const int level){
	_logger * logger = (_logger*) lger;
	if (logger == NULL) {
		/* printf("chat_log: the logger is NULL\n"); */
		return -1;
	}
	if (level < LEVEL_INFO || level > LEVEL_ERROR) {
		/* printf("create_logger: log level error\n"); */
		return -1;
	}
	logger->log_level = level;
	return 0;
}

void destroy_logger(logger lger) {
	_logger * logger = (_logger*) lger;
	if (lger == NULL)
		return;

	if (logger->to_file != NULL)
		fclose(logger->to_file);

	free(logger);
}

