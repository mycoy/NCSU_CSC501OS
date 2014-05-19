/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P2
 *
 * The main func for ush
 */

/*
 * Problems list:
 * 1. cd, cannot cd to "~/", cannot cd to "/root";
 *    what's the printf if cd doesn't exit?
 *
 * 2. where's format
 *
 * 3.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include "parse.h"

#ifndef USH_DEBUG
/* #define USH_DEBUG 1 */
#endif

#define MAX_HOSTNAME_LEN 256
#define MAX_DIR_LEN 1024

extern char **environ;

typedef struct _ush_core_st{
	char hostname[MAX_HOSTNAME_LEN];
	char currentDir[MAX_DIR_LEN];
	char * homeDir;
}_ush_core;

static _ush_core * ush_engine;
static char * ushrc_name = ".ushrc";

/*
 * return 1 if the cmd is a simple cmd
 */
int isBuiltinCmd(char * cmd){
	int result=0;

	if(cmd==NULL)
		return result;

#ifdef USH_DEBUG
		/* printf("**** isSimpleCmd, CMD:%s\n", c->args[0]); */
#endif

	if (strcmp("cd", cmd) == 0 || strcmp("echo", cmd) == 0 || strcmp("logout", cmd) == 0 || strcmp("nice", cmd) == 0 ||
		strcmp("pwd", cmd) == 0 || strcmp("setenv", cmd) == 0 || strcmp("unsetenv", cmd) == 0 || strcmp("where", cmd) == 0 )
		result=1;

	return result;
}

/*
If the command-name contains a /, the shell takes it as a pathname and
searches for it. If a pathname begins with a /, then the path is absolute; otherwise, the path is relative to the
current working directory. If the command-name does not contain a /, the shell attempts to resolve it to a
pathname, searching each directory in the PATH variable for the command.

Called by parseCmd
 */
char * searchFileCMD(char * to_search){
	char * cmd_buf=NULL;
	char * t_env, * t_env_buf;
	char * t1, * t2;

	DIR *dir;
	struct dirent *dir_entr;
	struct stat file_stat;

	int found_flag=0; /* if set to 1, find file cmd with the specified name; the found cmd file is NOT a dir path */

	if(to_search==NULL)
		return NULL;

	if(to_search[0]=='/'){ /* path is absolute */
#ifdef USH_DEBUG
		/* printf("**** searchFileCMD, absolute path:%s\n", c->args[0]); */
#endif

		cmd_buf = (char*)malloc( sizeof(char)*(strlen(to_search) +1) );
		memset(cmd_buf, 0, sizeof(char)*(strlen(to_search) +1) );
		memcpy(cmd_buf, to_search, sizeof(char)*strlen(to_search) );
		return cmd_buf;
	}
	if( strstr(to_search,"/")!=NULL ){  /* if path is relative to current working dir */
		cmd_buf = (char*)malloc( sizeof(char)*( strlen(ush_engine->currentDir)+strlen(to_search)+2) );
		memset(cmd_buf, 0, sizeof(char)*( strlen(ush_engine->currentDir)+strlen(to_search)+2) );
		sprintf(cmd_buf, "%s/%s", ush_engine->currentDir, to_search);
		return cmd_buf;
	}

	/* else, then to search PATH */
	t1 = to_search;  /* t1 is the file cmd to search */
	t_env = getenv("PATH");
	t_env_buf = (char*)malloc( sizeof(char)*strlen(t_env)+1 );
	memset(t_env_buf, 0, sizeof(char)*strlen(t_env)+1);
	memcpy(t_env_buf, t_env, sizeof(char)*strlen(t_env));

	while ((t2 = strsep(&t_env_buf, ":")) != NULL) { /* t2 is one dir in the PATH env */
		dir = opendir(t2);
		if (dir) {
			while ((dir_entr = readdir(dir)) != NULL) { /* get one file-entry in the dir */
				/* if find a dir entry whose name is the same as where's argument */
				if (strcmp(dir_entr->d_name, t1) == 0) {
					/* to spell out the file path name for the found cmd, to get its info */
					cmd_buf = (char*) malloc(
							sizeof(char) * (strlen(t2) + strlen(t1) + 2) );
					memset(cmd_buf, 0,
							sizeof(char) * (strlen(t2) + strlen(t1) + 2) );
					sprintf(cmd_buf, "%s/%s", t2, t1);
					if (stat(cmd_buf, &file_stat) == 0) { /* retrieve the dir-entry's stat-info */
						if (file_stat.st_mode & S_IFREG) { /* if the dir-entry is a file, not dir */
							found_flag = 1;
							break;
						}
					}
				}
			} /* end of while, finish searching one dir */
			closedir(dir);
		} /* end if(dir) */
	} /* end of while, finish searching all dirs of PATH */
	free(t_env_buf);

	if(found_flag==0 && cmd_buf!=NULL){
		free(cmd_buf);
		cmd_buf=(char*)malloc( sizeof(char)*17 );
		memset(cmd_buf, 0, sizeof(char)*17 );
		sprintf(cmd_buf, "%s", "!ONLY DIR FOUND!");
	}

	return cmd_buf;
}

/* called by executeCmd and nice in processBuiltin */
void processFileCmd(Cmd c, int index){
	pid_t pid;
	char * file_cmd=NULL;
	/* int permission_ret; */
	int exec_ret;


	file_cmd = searchFileCMD(c->args[0+index]);

	if (file_cmd == NULL) {
		printf("command not found\n");
		return;
	}
	if (strcmp(file_cmd, "!ONLY DIR FOUND!") == 0) {
		/* if the pathname matches a directory, a "permission denied" message is displayed. */
		printf("permission denied\n");
		return;
	}
	/*permission_ret = access (file_cmd, X_OK);
	 if( permission_ret!=0 ){
	 printf("permission denied\n");
	 return;
	 }*/

	pid = fork();
	if (pid == 0) { /* fork new process to execute */
#ifdef USH_DEBUG
		/* printf("**** processFileCmd, child process to execute single file command, %s\n",	file_cmd);  */
#endif

		dup2(STDOUT_FILENO, STDERR_FILENO);  /* redirect stderr to stdout, to avoid out/err overlap */
		exec_ret = execv(file_cmd, c->args+index);
		if (exec_ret != 0)
			printf("permission denied\n"); /* */
		exit(0);
	}/* end of child */
	else { /* parent wait for the child */
		waitpid(pid, NULL, 0);
		free(file_cmd);
		return;
	}
}


void processBuiltinCmd(Cmd c){
	int i;
	char ** t_envs; /* env stringlist */
	char * t_env; /* one env variable */
	char * t_env_buf;
	char * name_buf;
	char * t1, *t2;
	char * file_cmd;

	DIR *dir;
	struct dirent *dir_entr;
	struct stat file_stat;

	int nice_adjustment=4;  /* default, set to 4 */
	int pid;
	int exec_ret;

	int nice_withcmd_withoutprio = 0;
	char * nice_t_arg=NULL;

	if (strcmp("cd", c->args[0]) == 0) { /* cd */
		if(c->args[1]==NULL){ /* Without an argument, it changes the working directory to the original (home) directory. */

			if( chdir( ush_engine->homeDir )==0 ){
				memset(ush_engine->currentDir, 0, sizeof(char)*MAX_DIR_LEN);
				memcpy(ush_engine->currentDir, ush_engine->homeDir, strlen(ush_engine->homeDir) );
			}
		}
		else{
			if( chdir( c->args[1] )==0 ){
				memset(ush_engine->currentDir, 0, sizeof(char)*MAX_DIR_LEN);
				memcpy(ush_engine->currentDir, c->args[1], strlen(c->args[1]) );
			}else{
				printf("permission denied\n");
			}
		}

#ifdef USH_DEBUG
		printf("**** processBuiltinCmd, cd, currentDir:%s\n", ush_engine->currentDir);
#endif
	} else if (strcmp("echo", c->args[0]) == 0) { /* echo */
		for ( i = 1; c->args[i] != NULL; i++ )
			printf("%s ", c->args[i]);

		printf("\n");
	} else if (strcmp("logout", c->args[0]) == 0) { /* logout */
		exit(0);
	} else if (strcmp("nice", c->args[0]) == 0) { /* -----> nice */
		/*
		 * nice [[+/-]number] [command]
		 * Sets the scheduling priority for the shell to number, or, without number, to 4. With command,
		 * runs command at the appropriate priority. The greater the number, the less cpu the process gets.
		 * If no sign before the number, assume it is positive.
		 */

		if( c->args[1]==NULL){ /* if only given "nice" */
			nice_adjustment=4;
			setpriority( PRIO_PROCESS, 0, nice_adjustment);

#ifdef USH_DEBUG
			printf("**** processBuiltinCmd, only nice provided, requested privilege:%d\n",nice_adjustment );
			system("nice");
#endif
			return;
		}

		/* in the following, c->args[1] is guaranteed not NULL */
		/* parse out the privilege number */
#ifdef USH_DEBUG
		printf("**** processBuiltinCmd, nice, provided privilege: %s\n", c->args[1] );
#endif
		if (c->args[1][0] == '-') {
			nice_adjustment = atoi(c->args[1]+1);
			nice_adjustment *= -1;
		} else if (c->args[1][0] == '+') {
			nice_adjustment = atoi(c->args[1]+1);
		}else if( c->args[1][0]>='0' && c->args[1][0]<='9' ){
			nice_adjustment = atoi(c->args[1]);
		}else{
#ifdef USH_DEBUG
			printf("**** processBuiltinCmd, nice, only cmd(%s) provided, priority default to 4\n", c->args[1] );
#endif
			nice_withcmd_withoutprio=1;
			nice_adjustment=4;
		}


		if( c->args[1]!=NULL && c->args[2]==NULL && nice_withcmd_withoutprio==0 ){   /* if, nice [[+/-]number], set ush privilege */
			setpriority( PRIO_PROCESS, 0, nice_adjustment);
#ifdef USH_DEBUG
			printf("**** processBuiltinCmd, nice&privilege provided, requested privilege:%d\n",nice_adjustment );
			system("nice");
#endif
			return;
		}else{  /* if, nice [[+/-]number] [command]; nice cannot use processFileCmd, because of privilege setting requirement */
			if( nice_withcmd_withoutprio==1 ){
				nice_t_arg = c->args[1];
			}else{
				nice_t_arg = c->args[2];
			}

			if ( isBuiltinCmd(nice_t_arg)!= 1 ) { /* if not built-in command; search it in PATH, set its privilege, run it */
				file_cmd = searchFileCMD( nice_t_arg );

				if (file_cmd == NULL) {
					printf("command not found\n");
					return;
				}
				if (strcmp(file_cmd, "!ONLY DIR FOUND!") == 0) {
					/* if the pathname matches a directory, a "permission denied" message is displayed. */
					printf("permission denied\n");
					return;
				}

				/*permission_ret = access (file_cmd, X_OK);
				if( permission_ret!=0 ){
					printf("permission denied\n");
					return;
				}*/

				pid = fork();
				if(pid==0){  /* fork new process to execute */
					setpriority( PRIO_PROCESS, 0, nice_adjustment);
#ifdef USH_DEBUG
					printf("**** processBuiltinCmd, nice&privilege&cmd provided, requested privilege:%d, "
							"child process to execute single file command, %s\n", nice_adjustment, file_cmd); /* */
					system("nice");
#endif
					dup2(STDOUT_FILENO, STDERR_FILENO );   /* redirect stderr to stdout, to avoid out/err overlap */

					if( nice_withcmd_withoutprio==1 ){
						exec_ret = execv(file_cmd , c->args+1 );
					}else{
						exec_ret = execv(file_cmd , c->args+2 );
					}


					if(exec_ret!=0)
						printf("permission denied\n");
					exit(0);
				}/* end of child */
				else{ /* parent wait for the child */
					waitpid(pid, NULL, 0);
					free(file_cmd);
					return;
				}

			} /* end if not built-in */   else{
				/* ???? what to do if cmd is built-in??? */
				if (strcmp("cd", c->args[1]) == 0) { /* cd */
					if (c->args[2] == NULL) /* Without an argument, it changes the working directory to the original (home) directory. */
						chdir(ush_engine->homeDir);
					else {
						if (chdir(c->args[2]) == 0) {
							memset(ush_engine->currentDir, 0,
									sizeof(char) * MAX_DIR_LEN);
							memcpy(ush_engine->currentDir, c->args[2],
									strlen(c->args[2]));
						} else {
							printf("permission denied\n");
						}
					}

#ifdef USH_DEBUG
					printf("**** processBuiltinCmd, cd, currentDir:%s\n", ush_engine->currentDir);
#endif
				} else if (strcmp("echo", c->args[1]) == 0) { /* echo */
					for (i = 2; c->args[i] != NULL; i++)
						printf("%s ", c->args[i]);

					printf("\n");
				} else if (strcmp("logout", c->args[1]) == 0) { /* logout */
					exit(0);
				} else if (strcmp("pwd", c->args[1]) == 0) { /* pwd */
					printf("%s\n", ush_engine->currentDir);
				} else if (strcmp("setenv", c->args[1]) == 0) { /* setenv */
					/*
					 * setenv [VAR [word]]
					 * Without arguments, prints the names and values of all environment variables. Given VAR, sets
					 * the environment variable VAR to word or, without word, to the null string.
					 */
					if (c->args[2] != NULL && c->args[3] != NULL)
						setenv(c->args[2], c->args[3], 1);
					else if (c->args[2] != NULL && c->args[3] == NULL)
						setenv(c->args[2], NULL, 1);
					else { /* call setenv, without any argument, then printf all env */
						t_envs = environ;
						for (; *t_envs; t_envs++) {
							printf("%s\n", *t_envs);
						}
					}

#ifdef USH_DEBUG
					if(c->args[2]!=NULL)
						printf("**** processBuiltinCmd, setenv %s: %s\n", c->args[2], getenv(c->args[2]) );
#endif
				} else if (strcmp("unsetenv", c->args[1]) == 0) { /* unsetenv */
					if (c->args[1] == NULL)
						return;
					unsetenv(c->args[2]);

#ifdef USH_DEBUG
					printf("**** processBuiltinCmd, unsetenv %s: %s\n", c->args[2], getenv(c->args[2]) );
#endif
				} else if (strcmp("where", c->args[1]) == 0) { /* where */
					/* first check if built-in cmd */
					t1 = c->args[2]; /* t1 is the file cmd to search */
					if (isBuiltinCmd(t1))
						printf("%s is a shell built-in\n", t1); /* if built-in, printf */

					/* then also search PATH env */
					t_env = getenv("PATH");

					t_env_buf = (char*) malloc(
							sizeof(char) * strlen(t_env) + 1);
					memset(t_env_buf, 0, sizeof(char) * strlen(t_env) + 1);
					memcpy(t_env_buf, t_env, sizeof(char) * strlen(t_env));

					while ((t2 = strsep(&t_env_buf, ":")) != NULL) { /* t2 is one dir in the PATH env */
						dir = opendir(t2);
						if (dir) {
							while ((dir_entr = readdir(dir)) != NULL) { /* get one file-entry in the dir */
								/* if find a dir entry whose name is the same as where's argument */
								if (strcmp(dir_entr->d_name, t1) == 0) {
									/* to spell out the file path name for the found cmd, to get its info */
									name_buf = (char*) malloc(
											sizeof(char)
													* (strlen(t2) + strlen(t1)
															+ 2));
									memset(name_buf, 0,
											sizeof(char)
													* (strlen(t2) + strlen(t1)
															+ 2));
									sprintf(name_buf, "%s/%s", t2, t1);
									if (stat(name_buf, &file_stat) == 0) { /* successfully, retrieve the dir-entry's stat-info */
										if (file_stat.st_mode & S_IFREG) { /* if the dir-entry is a file, not dir */
											printf("%s\n", name_buf);
											free(name_buf);
											continue;
										}
									}
								}
							} /* end of while, finish searching one dir */
							closedir(dir);
						} /* end if(dir) */
					} /* end of while, finish searching all dir of PATH */
					free(t_env_buf);
				} /* end of else-if(where) */

			} /* end of nice to process built-in */
		}  /* end of, if, nice [[+/-]number] [command] */

	} else if (strcmp("pwd", c->args[0]) == 0) { /* pwd */
		printf("%s\n", ush_engine->currentDir);
	} else if (strcmp("setenv", c->args[0]) == 0) { /* setenv */
		/*
		 * setenv [VAR [word]]
		 * Without arguments, prints the names and values of all environment variables. Given VAR, sets
		 * the environment variable VAR to word or, without word, to the null string.
		 */
		if(c->args[1]!=NULL && c->args[2]!=NULL)
			setenv(c->args[1], c->args[2], 1);
		else if(c->args[1]!=NULL && c->args[2]==NULL)
			setenv(c->args[1], NULL, 1);
		else{ /* call setenv, without any argument, then printf all env */
			t_envs = environ;
			for (; *t_envs; t_envs++) {
			    printf("%s\n", *t_envs);
			}
		}

#ifdef USH_DEBUG
		if(c->args[1]!=NULL)
			printf("**** processBuiltinCmd, setenv %s: %s\n", c->args[1], getenv(c->args[1]) );
#endif
	} else if (strcmp("unsetenv", c->args[0]) == 0) { /* unsetenv */
		if( c->args[1]==NULL)
			return;
		unsetenv( c->args[1] );

#ifdef USH_DEBUG
		printf("**** processBuiltinCmd, unsetenv %s: %s\n", c->args[1], getenv(c->args[1]) );
#endif
	} else if (strcmp("where", c->args[0]) == 0) { /* where */
		/* first check if built-in cmd */
		t1 = c->args[1]; /* t1 is the file cmd to search */
		if ( isBuiltinCmd(t1) )
			printf("%s is a shell built-in\n", t1); /* if built-in, printf */

		/* then also search PATH env */
		t_env = getenv("PATH");

		t_env_buf = (char*)malloc( sizeof(char)*strlen(t_env)+1 );
		memset(t_env_buf, 0, sizeof(char)*strlen(t_env)+1);
		memcpy(t_env_buf, t_env, sizeof(char)*strlen(t_env));

		while( (t2 = strsep(&t_env_buf, ":")) != NULL ) { /* t2 is one dir in the PATH env */
			dir = opendir(t2);
			if (dir) {
				while ((dir_entr = readdir(dir)) != NULL) { /* get one file-entry in the dir */
					/* if find a dir entry whose name is the same as where's argument */
					if( strcmp(dir_entr->d_name, t1)==0 ){
						/* to spell out the file path name for the found cmd, to get its info */
						name_buf = (char*)malloc( sizeof(char)*(strlen(t2)+strlen(t1)+2) );
						memset(name_buf, 0, sizeof(char)*(strlen(t2)+strlen(t1)+2));
						sprintf(name_buf, "%s/%s", t2, t1);
						if( stat(name_buf, &file_stat) == 0 ){  /* successfully, retrieve the dir-entry's stat-info */
							if( file_stat.st_mode & S_IFREG ){ /* if the dir-entry is a file, not dir */
								printf("%s\n", name_buf);
								free(name_buf);
								continue;
							}
						}
					}
				} /* end of while, finish searching one dir */
				closedir(dir);
			} /* end if(dir) */
		} /* end of while, finish searching all dir of PATH */
		free(t_env_buf);
	} /* end of else-if(where) */
}  /* end of processBuiltinCmd */


void executeCmd(Cmd c){
#ifdef USH_DEBUG
		printf("**** executeCmd, %s\n", c->args[0] ); /* */
#endif
	if (isBuiltinCmd(c->args[0])) { /* if built-in command */
		processBuiltinCmd(c);
	}/* end of parent*/
	else { /* if file cmd, search and execute it */
		processFileCmd(c, 0);
	}/* end of else */
}

/*
 * exec/in/out type demo:
 * 0  Terror
 * 1  Tword
 * 2  Tamp
 * 3  Tpipe		--a|b, a's in Tnil, out Tpipe; b's in Tpipe, b's out Tnil
 * 4  Tsemi		--a, a's exec Tsemi
 * 5  Tin		--a<b, a's in Tin, a's out Tnil
 * 6  Tout		--a>b, a's in Tnil, a's out Tout
 * 7  Tapp		--a>>b, a's in Tnil, a's out Tapp
 * 8  TpipeErr	--a|&b, a's in Tnil, out TpipeErr; b's in TpipeErr, b's out Tnil
 * 9  ToutErr	--a>&b, a's in Tnil, a's out ToutErr
 * 10 TappErr	--a>>&b, a's in Tnil, a's out TappErr
 * 11 Tnl
 * 12 Tnil		--a, in/out both Tnil
 * 13 Tend		--end of file ??
 *
 * Some invalid input:
 * a<<b, a<&b,
 *
 * For the preceding cmd of a pipeline, it just prints out its stdout/stderr,
 * the following cmd must check its in-type to properly redirect stdout/stderr to its input.
 *
 */
void parseCmd(Cmd c) {
	int outFile, inFile ;
	int stdin_bak, stdout_bak, stderr_bak;

	if(c==NULL)
		return;

	/* if the output is a file*/
	if (c->out == Tout || c->out == Tapp || c->out == ToutErr
			|| c->out == TappErr) {
#ifdef USH_DEBUG
		printf("**** parseCmd, CMD's output file:%s\n", c->outfile);  /* */
#endif
		stdout_bak = dup(fileno(stdout));
		if( c->in==Tin ){
			stdin_bak = dup(fileno(stdin));inFile = open(c->infile, O_RDONLY);dup2(inFile, STDIN_FILENO);
		}
		switch(c->out) {
		case Tout:
			outFile = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

			if(outFile<0){
#ifdef USH_DEBUG
				printf("**** parseCmd, cannot open CMD's output file:%s\n", c->outfile);  /* */
#endif
				return;
			}
			dup2(outFile, STDOUT_FILENO);

			executeCmd(c);

			dup2(stdout_bak, STDOUT_FILENO);
			close(stdout_bak);
			break;
		case Tapp:
			outFile = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

			if(outFile<0){
#ifdef USH_DEBUG
				printf("**** parseCmd, cannot open CMD's output file:%s\n", c->outfile);  /* */
#endif
				return;
			}
			dup2(outFile, STDOUT_FILENO);

			executeCmd(c);

			dup2(stdout_bak, fileno(stdout));
			close(stdout_bak);
			break;
		case ToutErr:
			outFile = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

			if(outFile<0){
#ifdef USH_DEBUG
				printf("**** parseCmd, cannot open CMD's output file:%s\n", c->outfile);  /* */
#endif
				return;
			}

			stderr_bak = dup( STDERR_FILENO);
			dup2(outFile, STDOUT_FILENO);
			dup2(outFile, STDERR_FILENO);

			executeCmd(c);

			dup2(stdout_bak, STDOUT_FILENO);
			dup2(stderr_bak, STDERR_FILENO);
			close(stdout_bak);
			close(stderr_bak);
			break;
		case TappErr:
			outFile = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

			if(outFile<0){
#ifdef USH_DEBUG
				printf("**** parseCmd, cannot open CMD's output file:%s\n", c->outfile);  /* */
#endif
				return;
			}

			stderr_bak = dup( STDERR_FILENO);
			dup2(outFile, STDOUT_FILENO);
			dup2(outFile, STDERR_FILENO);

			executeCmd(c);

			dup2(stdout_bak, STDOUT_FILENO);
			dup2(stderr_bak, STDERR_FILENO);
			close(stdout_bak);
			close(stderr_bak);
			break;
		} /* end of switch */
		if( c->in==Tin ){
			dup2(stdin_bak, STDIN_FILENO);close(stdin_bak); close(inFile);
		}
		close(outFile);
		fflush(NULL);
		return;
	}else if( c->in==Tin ){
		stdin_bak = dup(fileno(stdin));inFile = open(c->infile, O_RDONLY);dup2(inFile, STDIN_FILENO);
		executeCmd(c);
		dup2(stdin_bak, STDIN_FILENO);close(stdin_bak); close(inFile);fflush(NULL);
	}else {

#ifdef USH_DEBUG
		printf("**** parseCmd, command(no output file), %s\n", c->args[0]);  /* */
#endif
		executeCmd(c);
	}

}

/* sequence -> pipeline -> cmd */
/* get idea from, http://stackoverflow.com/questions/916900/having-trouble-with-fork-pipe-dup2-and-exec-in-c/ */
void parsePipe(Pipe p) {
	Cmd c;
	int new_fds[2];
	int old_fds[2];
	int old_flag=0;

	int pid;

	if (p == NULL ){
		return;
	}

/*
	stdin_bak = dup(STDIN_FILENO);
	stdout_bak = dup(STDOUT_FILENO);
	stderr_bak = dup(STDERR_FILENO);
*/

#ifdef USH_DEBUG
	printf("**** parsePipe, to parse pipe, type:%d, pipe->next:%s\n", p->type, p->next );
#endif


	for (c = p->head; c != NULL ; c = c->next) {
#ifdef USH_DEBUG
		printf("**** parsePipe, to parse cmd(%s, in:%d, out:%d, exec:%d) \n", c->args[0], c->in, c->out, c->exec); /* */
#endif


		if ( !strcmp(c->args[0], "end") ){
#ifdef USH_DEBUG
			printf("**** parsePipe, encounter the end of input, exit(0)\n");
#endif
		    exit(0);
		}

		/* if no pipe associated with this cmd*/
		if (c->in != Tpipe && c->in != TpipeErr && c->out!=Tpipe && c->out!=TpipeErr ){
#ifdef USH_DEBUG
			printf("**** parsePipe, to execute cmd(%s, in:%d, out:%d, exec:%d) which has no in/out pipe\n", c->args[0], c->in, c->out, c->exec);
#endif

			parseCmd(c);
			continue;
		}


		/* if there is a next cmd */
		if( c->out==Tpipe || c->out==TpipeErr ){
			pipe(new_fds);
		}

		pid = fork();
		if( pid==0 ){ /* if child */
			if(old_flag){  /* if there's a previous cmd */
				dup2(old_fds[0], STDIN_FILENO);
				close(old_fds[0]);
				close(old_fds[1]);
			}
			if( c->out==Tpipe || c->out==TpipeErr ){   /* if there's a next cmd */
				close(new_fds[0]);
				dup2(new_fds[1], STDOUT_FILENO);
				if(c->out==TpipeErr)
					dup2(new_fds[1], STDERR_FILENO);
				close(new_fds[1]);
			}
			parseCmd(c);
			exit(0);
		}else{  /* parent */
			if(old_flag){  /* if there's a previous cmd */
				close(old_fds[0]);
				close(old_fds[1]);
			}
			if( c->out==Tpipe || c->out==TpipeErr ){  /* if there's a next cmd */
				old_fds[0] = new_fds[0];
				old_fds[1] = new_fds[1];
				old_flag=1;
			}else{
				old_flag=0;
			}
			wait(NULL);
		} /*end of parent*/

	}  /* end of for(iterate cmds in a pipe) */


	parsePipe(p->next);
}

void check_ushrc(){
	char * ushrc = NULL;
	int ushrc_f;
	int stdin_bak;
	Pipe p;


#ifdef USH_DEBUG
		printf("**** check_ushrc,to check ~/.ushrc\n"); /* */
#endif

	/* spell out the full path of ~/.ushrc */
	ushrc = (char*)malloc( sizeof(char)*(strlen(ushrc_name)+strlen(ush_engine->homeDir)+2) );
	memset(ushrc, 0, sizeof(char)*(strlen(ushrc_name)+strlen(ush_engine->homeDir)+2) );
	sprintf(ushrc, "%s/%s", ush_engine->homeDir, ushrc_name);

	ushrc_f = open(ushrc, O_RDONLY );
	/* to check if ushrc readable */
	if(ushrc_f<0){
		free(ushrc);
		return;
	}

	stdin_bak = dup( fileno(stdin) );
	dup2( ushrc_f, STDIN_FILENO );

	while (1) {
		p = parse();

		if(p==NULL || p->head==NULL ){
			break;
		}
		if ( !strcmp(p->head->args[0], "end") ){
			break;
		}

		parsePipe(p);
		freePipe(p);
	}

	dup2(stdin_bak, STDIN_FILENO );

	close(ushrc_f);
	close(stdin_bak);

	free(ushrc);
}

/* setup ush_engine and signal-handling */
void ush_init(){
	struct passwd *pw;
	ush_engine=(_ush_core*)malloc(sizeof(_ush_core));

	if(ush_engine==NULL){
#ifdef USH_DEBUG
		printf("**** ush_init, memory error1.\n");
#endif
		exit(1);
	}

	memset(ush_engine->hostname, 0, sizeof(char)*MAX_HOSTNAME_LEN);
	gethostname(ush_engine->hostname, sizeof(char)*MAX_HOSTNAME_LEN);

	memset(ush_engine->currentDir, 0, sizeof(char)*MAX_DIR_LEN);
	getcwd(ush_engine->currentDir, sizeof(char)*MAX_DIR_LEN);

	pw = getpwuid(getuid());
	ush_engine->homeDir = (char*)malloc( sizeof(char)*strlen(pw->pw_dir)+1 );
	memset(ush_engine->homeDir, 0,  sizeof(char)*strlen(pw->pw_dir)+1 );
	memcpy(ush_engine->homeDir, pw->pw_dir, sizeof(char)*strlen(pw->pw_dir));

	if( ush_engine->homeDir==NULL ){
#ifdef USH_DEBUG
		printf("**** ush_init, memory error2.\n");
#endif
		exit(1);
	}

#ifdef USH_DEBUG
	printf("**** ush_init, hostname:%s, currentDir:%s, homeDir:%s\n", ush_engine->hostname, ush_engine->currentDir,ush_engine->homeDir);
#endif

	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM , SIG_IGN);
}

void printPrompt(){
	printf("%s%%", ush_engine->hostname);
}

int main(int argc, char *argv[]) {
	Pipe p;

	/* int stdin_bak, stdout_bak, stderr_bak; */
	int ushrc_pid, pid;

	ush_init();

/*	ushrc_pid = fork();
	if(ushrc_pid==0){
		check_ushrc();
		exit(0);
	}
	else{
		wait(NULL);
	}*/
	check_ushrc();

	while (1) {

		fflush(NULL);
		if( isatty(STDIN_FILENO) )
			printf("%s%%", ush_engine->hostname);
		fflush(NULL);

/*		if(flag){
			flag=0;
			printPrompt();
			continue;
		}*/
		p = parse();


/*		if( p!=NULL && p->head!=NULL && (p->head->out==Tpipe||p->head->out==TpipeErr) ){
			printf("**** set flag to 1\n");
			flag=1;
		}*/

/*		if( p!=NULL && p->head!=NULL && (p->head->out==Tpipe||p->head->out==TpipeErr) ){
			pid = fork();
			if(pid==0){
				parsePipe(p);
				exit(0);
			}else{
				wait(pid, NULL, 0);
			}
		}else{*/
			parsePipe(p);
		/*}*/


		freePipe(p);
	}

}
