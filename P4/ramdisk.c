/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P4.
 *
 * The main func for ramdisk.
 */

/*
  cd /home/wenzhao/data2/zwz/working/eclipse-c-workspace/OS_P4/src
  gcc -Wall ramdisk.c ramdisk_log.h ramdisk_log.c `pkg-config fuse --cflags --libs` -o ramdisk
  tar -cvf osp4_wzhang27.tar Makefile ramdisk.c ramdisk.h ramdisk_list.c ramdisk_log.c ramdisk_log.h
 */
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "ramdisk_log.h"
#include "ramdisk.h"

#define DEBUG_BUF_SIZE 512
/* dir/file permissions */
#define PERM_DIR 0755
#define PERM_FILE 0766

#define DIR_SIZE 4096


typedef struct _ramdisk_core_st {
	long max_size;  /* max size of this ramdisk, passed from cmd-line, in bytes */
	long current_size;  /* current total file size in the ramdisk, in bytes */

	_list * root_files; /* a list of files under the root-dir */

	logger lgr;

	time_t time;  /* used for file attributes */
	uid_t uid;
	gid_t gid;

} _ramdisk_core;

static _ramdisk_core * ramdisk_engine;

/*
 * check if OK to write more;
 * ramdisk_engine->current_size+size ? ramdisk_engine->max_size;
 * return: 0, OK to write; 1: will overflow is write more;
 */
static int check_ramdisk_capacity(int size){
	return (ramdisk_engine->current_size+size) <= ramdisk_engine->max_size ? 0 : 1;
}


static _ramdisk_file * create_ramdisk_dir(char * name, char * p_name){
	_ramdisk_file * t;

	if(name==NULL || p_name==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "create_ramdisk_dir, name or p_name is NULL\n", LEVEL_ERROR);
#endif
		return NULL;
	}

	t = (_ramdisk_file *)malloc(sizeof(_ramdisk_file));
	if(t==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "create_ramdisk_dir, memory error1\n", LEVEL_ERROR);
#endif
		return NULL;
	}

	t->type = FILE_DIR;
	t->sub_files = create_list();
	t->name = (char*)malloc(sizeof(char)*strlen(name)+1);
	t->p_name = (char*)malloc(sizeof(char)*strlen(p_name)+1);
	if(t->sub_files==NULL || t->name==NULL || t->p_name==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "create_ramdisk_dir, memory error2\n", LEVEL_ERROR);
#endif
		free(t->sub_files);
		free(t->name);
		free(t->p_name);
		free(t);
		return NULL;
	}
	memset(t->name, 0, sizeof(char)*strlen(name)+1);
	memset(t->p_name, 0, sizeof(char)*strlen(p_name)+1);
	memcpy(t->name, name, sizeof(char)*strlen(name));
	memcpy(t->p_name, p_name, sizeof(char)*strlen(p_name));
	t->block_number=0;
	t->blocks=NULL;
	t->size=0;
	t->openned=0;

	return t;
}

static _ramdisk_file * create_ramdisk_rf(char * name, char * p_name){
	_ramdisk_file * t;
	int i;

	if(name==NULL || p_name==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "create_ramdisk_rf, name or p_name is NULL\n", LEVEL_ERROR);
#endif
		return NULL;
	}

	t = (_ramdisk_file *)malloc(sizeof(_ramdisk_file));
	if(t==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "create_ramdisk_rf, memory error1\n", LEVEL_ERROR);
#endif
		return NULL;
	}

	t->type = FILE_REG;
	t->name = (char*)malloc(sizeof(char)*strlen(name)+1);
	t->p_name = (char*)malloc(sizeof(char)*strlen(p_name)+1);
	t->blocks = (char**)malloc( sizeof(char*)*INIT_BLOCK_NUM + 1);
	if( t->name==NULL || t->p_name==NULL || t->blocks==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "create_ramdisk_rf, memory error2\n", LEVEL_ERROR);
#endif
		free(t->name);
		free(t->p_name);
		free(t->blocks);
		free(t);
		return NULL;
	}
	memset(t->name, 0, sizeof(char)*strlen(name)+1);
	memset(t->p_name, 0, sizeof(char)*strlen(p_name)+1);
	memcpy(t->name, name, sizeof(char)*strlen(name));
	memcpy(t->p_name, p_name, sizeof(char)*strlen(p_name));
	memset(t->blocks, 0, sizeof(char*)*INIT_BLOCK_NUM + 1 );
	t->sub_files = NULL;
	t->block_number=INIT_BLOCK_NUM;
	t->size=0;
	t->openned=0;
	t->parent=NULL;

	return t;
}

/*
 * given "/home/wenzhao/data2/zwz/working/eclipse-c-workspace/OS_P4_FUSE/src/test/home/wenzhao/t1",
 * returns 12
 */
static int count_path_levels(const char * path){
	int len, count, i;

	if(path==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "count_path_levels, path is NULL\n", LEVEL_ERROR);
#endif
		return 0;
	}

	len = strlen(path);
	count = 0;
	for (i = 0; i < len; i++) {
		if( (*(path + i)) == '/' )
			count++;
	}

	return count;
}

/*
 * breaks every dir/file element in a path to a string array.
 * caller is responsible for freeing the returned string array.
 *
 * ret_count will be, the number of "/" -1. For example,
 * path="/home/wenzhao/data2", count=2;
 * path="/home", count=0;
 * path="/", count=0;
 */
static char ** break_paths(const char * path, int * ret_count){
	int path_len, i, count, flag, start, end;
	char ** file_names;

	if(path==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "break_paths, path is NULL\n", LEVEL_ERROR);
#endif
		return NULL;
	}

	path_len = strlen(path);
	count = count_path_levels(path);
	file_names = (char**) malloc(sizeof(char*) * count+1);
	memset(file_names, 0, sizeof(char*) * count+1);
	if( file_names==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "break_paths, file_names creation memory error\n", LEVEL_ERROR);
#endif
		free(file_names);
		return NULL;
	}
	for(i=0;i<count;i++){
		file_names[i]=NULL;
	}

	count = -1;  /* when the for-loop ends, count will be, the number of "/" -1 */
	start = end = 0;
	for (i = 0; i < path_len; i++) {
		flag = 0;
		if ((*(path + i)) == '/') {
			end = i;
			count++;
			flag = 1;
		}
		if (!flag)
			continue;
		if (count == 0) {
			start = end;
			continue;
		}
		file_names[count - 1] = (char*) malloc(sizeof(char) * (end - start));
		if(file_names[count - 1]==NULL){
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr, "break_paths, file_names creation memory error\n", LEVEL_ERROR);
#endif
			return NULL;
		}
		memset(file_names[count - 1], 0, sizeof(char) * (end - start));
		memcpy(file_names[count - 1], path + start + 1, sizeof(char) * (end - start - 1));

		start = end;
	}
	file_names[count] = (char*) malloc(sizeof(char) * (path_len - start));
	memset(file_names[count], 0, sizeof(char) * (path_len - start));
	memcpy(file_names[count], path + start + 1, sizeof(char) * (path_len - start - 1));

	*ret_count = count;
	return file_names;
}

/*
 * to check if a given file/dir path is valid.
 * if valid, return its _ramdisk_file pointer,
 * if not, return NULL.
 */
static _ramdisk_file * is_path_valid(const char *path, int file_type){
	char s[DEBUG_BUF_SIZE];
	char ** file_names;
	int i, count, flag;
	_ramdisk_file * r_file=NULL, * r_file_p=NULL, * r_file_c=NULL;
	int ret=0; /* return value */

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "is_path_valid called, path:%s, type:%d\n", path, file_type);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if(path==NULL){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "is_path_valid, path is NULL\n", LEVEL_ERROR);
#endif
		return NULL;
	}


	file_names = break_paths(path, &count);
	if( file_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "is_path_valid, cannot create file_names\n", LEVEL_ERROR);
#endif
		return NULL;
	}


	/* check parent dir exist, and get attributes */
	if(count!=-1){
		if(count!=0){ /* if more than one level dir */
			for(i=0;i<=count-1;i++){ /* this for only checks if parent dis exist, doesn't create dir */
				if(i==0){
					r_file_p = is_file_exist(file_names[i], ramdisk_engine->root_files);
					if(r_file_p==NULL || r_file_p->type==FILE_REG){
#if OSP4_DEBUG
						memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
						sprintf(s, "is_path_valid, dir:%s doesn't exist or it's REG-file, cannot continue, 1\n", file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret=-EPERM;
						break;
					}
					continue;
				}
				r_file_c = is_file_exist(file_names[i], r_file_p->sub_files);
				if(r_file_c==NULL || r_file_c->type==FILE_REG ){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "is_path_valid, dir:%s doesn't exist or it's REG-file, cannot continue, 2\n", file_names[i]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					ret=-EPERM;
					break;
				}

				r_file_p = r_file_c;
			} /* end of for */
			if(ret==0){ /* if parent-dir check pass */
				r_file = is_file_exist(file_names[count], r_file_p->sub_files);
				if (r_file == NULL) { /* if the specified file doesn't exist */
					ret = -EPERM;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "is_path_valid, %s doesn't exist under %s-dir\n", file_names[count], r_file_p->name);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}else if( file_type!=FILE_IGN/*don't ignore file type*/ && r_file->type!=file_type){
					ret = -EPERM;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "is_path_valid, %s's file type(%d) is not the specified type(%d)\n", file_names[count], r_file->type, file_type);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}
			}
		}else{/* count==0, something like, "/t1"; */
			r_file = is_file_exist(file_names[count], ramdisk_engine->root_files);
			if( r_file==NULL ){ /* if file already exists */
				ret = -EPERM;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "is_path_valid, %s doesn't exist under root-dir\n", file_names[count]);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}else if( file_type!=FILE_IGN/*don't ignore file type*/ && r_file->type!=file_type){
				ret = -EPERM;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "is_path_valid, %s's file type(%d) is not the specified type(%d)\n", file_names[count], r_file->type, file_type);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}
		}

	}else{
		/* some unknown mistakes */
		ret = -EPERM;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "is_path_valid, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
	}

	/* free file_names */
	for(i=0; i<=count; i++)
		free(file_names[i]);
	free(file_names);


	if(ret == 0)
		return r_file;
	else
		return NULL;
}


/** File open operation
 *
 * No creation (O_CREAT, O_EXCL) and by default also no
 * truncation (O_TRUNC) flags will be passed to open(). If an
 * application specifies O_TRUNC, fuse first calls truncate()
 * and then open(). Only if 'atomic_o_trunc' has been
 * specified and kernel version is 2.6.24 or later, O_TRUNC is
 * passed on to open.
 *
 * Unless the 'default_permissions' mount option is given,
 * open should check if the operation is permitted for the
 * given flags. Optionally open may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to all file operations.
 *
 * Changed in version 2.2
 */
static int r_open(const char *path, struct fuse_file_info *fi){
	char s[DEBUG_BUF_SIZE];
	_ramdisk_file * r_file;
	int ret=0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_open called, path:%s, fi:%ld, O_RDONLY:%d, O_WRONLY:%d, O_RDWR:%d, O_APPEND:%d, O_TRUNC:%d\n",
			path, fi, fi->flags&O_RDONLY, fi->flags&O_WRONLY, fi->flags&O_RDWR, fi->flags&O_APPEND, fi->flags&O_TRUNC);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if(path==NULL || strcmp(path, "/")==0 || '/'== (* (path+strlen(path)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_open, path is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}

	r_file = is_path_valid(path, FILE_REG);

	if(r_file==NULL)
		ret = -ENOENT;

	return ret;
}

/**
 * FUSE DOC:
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */

/**
 * zwz:
 * If the provided path doesn't exist, this function won't create those non-exist dirs, it will return error;
 * this is the standard behavior of POSIX create.
 *
 * If parent-dir checking pass, just create the file. "getattr" should have guaranteed the file doesn't exist, unnecessary to check existence again.
 */
static int r_create(const char * path, mode_t mode, struct fuse_file_info * fi){
	char s[DEBUG_BUF_SIZE];
	char ** file_names;
	int i, count, flag;
	_ramdisk_file * r_file, * r_file_p, * r_file_c;
	int ret=0; /* return value */

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_create called, path:%s, fi:%ld\n", path, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if(path==NULL || strcmp(path, "/")==0/* if path="/" */ || '/'== (* (path+strlen(path)-1) )/* if path is like, "/t1/t2/t3/", invalid */ ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_create called, path is NULL, or only /, or ended with /\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	file_names = break_paths(path, &count);
	if( file_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_create called, cannot create file_names\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}


	/* check parent dir exist, and create file */
	if(count!=-1){
		if(count!=0){ /* if more than one level dir */
			for(i=0;i<=count-1;i++){ /* this for only checks if parent dis exist, doesn't create dir */
				if(i==0){
					r_file_p = is_file_exist(file_names[i], ramdisk_engine->root_files);
					if(r_file_p==NULL || r_file_p->type==FILE_REG){
#if OSP4_DEBUG
						memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
						sprintf(s, "r_create, dir:%s doesn't exist or it's REG-file, cannot create file, 1\n", file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret=-ENOENT;
						break;
					}
					continue;
				}
				r_file_c = is_file_exist(file_names[i], r_file_p->sub_files);
				if(r_file_c==NULL || r_file_c->type==FILE_REG){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_create, dir:%s doesn't exist or it's REG-file, cannot create file, 2\n", file_names[i]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					ret=-ENOENT;
					break;
				}

				r_file_p = r_file_c;
			} /* end of for */
			if(ret==0){ /* if parent-dir check pass, create file */
				r_file = create_ramdisk_rf(file_names[count], r_file_p->name );
				if(r_file!=NULL){
					r_file->parent = r_file_p; /* set parent pointer */
					en_list(create_l_elem(r_file), r_file_p->sub_files );
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_create, file:%s is created, and added to %s-dir\n", r_file->name, r_file_p->name);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}
				else
					ret = -EPERM;
			}
		}else{
			/* count==0, something like, "/t1"; just create new file under root dir. */
			r_file = create_ramdisk_rf(file_names[count], "/");
			if(r_file!=NULL){
				en_list(create_l_elem(r_file), ramdisk_engine->root_files );
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_create, file:%s is created, and added to root-dir\n", r_file->name);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}
			else
				ret = -EPERM;
		}
	}else{
		/* some unknown mistakes */
		ret = -ENOENT;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_create, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
	}

	/* free file_names */
	for(i=0; i<=count; i++)
		free(file_names[i]);
	free(file_names);

/*
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_create, final return value:%d\n", ret);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
*/

	return ret;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.	 An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */

/*
 * Argument	in/out	Description:
path	input	A path to the file from which function has to read
buf	output	buffer to which function has to write contents of the file
size	input	both the size of buf and amount of data to read
offset	input	offset from the beginning of file
fi	input	detailed information about read operation, see fuse_file_info for more information
return	output	amount of bytes read, or negated error number on error

http://sourceforge.net/apps/mediawiki/fuse/index.php?title=Read()
 */
static int r_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	char s[DEBUG_BUF_SIZE];
	_ramdisk_file * r_file=NULL;
	//int read_block_num=0; /* how many blocks to read */
	int current_offset=0; /* offset of the file during this reading, in byte; */
	int block_index_offset=0; /* the block index, from which this reading starts, starts from 0 */
	int block_byte_offset=0; /* the byte offset of block_index_offset, from which this reading starts */
	int r_size=0; /* real read size; the given size could be bigger than the file's actual size */
	int t_size=0; /* local copy of size, will be modified */
	int i=0;
	int buf_offset=0;
	int to_copy_block_num=0; /* number of blocks to copy, excluding current block */
	int current_block_remaining=0; /* current block's bytes after the offset */

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_read called, path:%s, size:%d, offset:%d, fi:%ld\n", path, size, offset, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if(path==NULL || strcmp(path, "/")==0 || '/'== (* (path+strlen(path)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_read, path is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}
	if(buf==NULL || size<0 || offset<0){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_read, path is NULL, or size<0, or offset<0\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	r_file = is_path_valid(path, FILE_REG);
	if(r_file==NULL)
		return -ENOENT;

	current_offset = offset;
	block_index_offset = current_offset / BLOCK_SIZE;
	block_byte_offset = (current_offset%BLOCK_SIZE);
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_read, current_offset:%d, block_index_offset:%d, block_byte_offset:%d\n",
			current_offset, block_index_offset, block_byte_offset);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	/* sometimes, size could very large, but the actual file's size is very small */
	if( (block_byte_offset+size/* 4095+1 */)<=BLOCK_SIZE ){ /* if only read from current block; */
		memcpy(buf, r_file->blocks[block_index_offset]+block_byte_offset, size);

	}else{ /* if need to read from the remaining blocks */

		current_block_remaining = BLOCK_SIZE - block_byte_offset ; /*4096-4095==1 bytes remained to be read;(index starts from 0!) */
		if( size > (r_file->size - current_offset) ){ /* if the given size is bigger than the file's max possible read size(total-offset) */
			r_size = r_file->size - current_offset;
		}else{
			r_size = size;
		}
		t_size = r_size;

		/* to_copy_block_num: excluding current block */
		if( ( (r_size- current_block_remaining ) % BLOCK_SIZE )!=0 ){
			if( (r_size- current_block_remaining )<BLOCK_SIZE )  /* if only read from current block */
				to_copy_block_num = 0;
			else
				to_copy_block_num = (r_size- current_block_remaining ) / BLOCK_SIZE + 1;
		}
		else
			to_copy_block_num = (r_size- current_block_remaining ) / BLOCK_SIZE;
#if OSP4_DEBUG
		memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
		sprintf(s, "r_read, r_size:%d, to_copy_block_num:%d\n", r_size, to_copy_block_num);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

		/* copy current block first; this copy is likely only part of the block */
		memcpy(buf, r_file->blocks[block_index_offset] + block_byte_offset, current_block_remaining);
		buf_offset += current_block_remaining;
		t_size -= current_block_remaining;

		/* copy remaining bytes */
		for (i = 1; i<=to_copy_block_num; i++) {
			/* if some unknown error */
			if( (block_index_offset+i)>=r_file->block_number || r_file->blocks[block_index_offset+i]==NULL ){
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_read, read error, (block_index_offset[%d]+i[%d])>=r_file->block_number[%d], "
						"or r_file->blocks[block_index_offset+i]==NULL.  t_size:%d\n", block_index_offset, i, r_file->block_number, t_size);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_ERROR);
#endif
				continue; /* try to skip this unknown error */
			}
			if( t_size>BLOCK_SIZE ){ /* copy whole block */
				memcpy(buf+buf_offset, r_file->blocks[block_index_offset+i], BLOCK_SIZE);
				buf_offset += BLOCK_SIZE;
				t_size-=BLOCK_SIZE;
			}else{
				memcpy(buf+buf_offset, r_file->blocks[block_index_offset+i], t_size);
			}
		}
	}  /* end of outter if-else */

	return r_size;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.	 An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int r_write(const char * path, const char * buf, size_t size, off_t offset, struct fuse_file_info * fi){
	char s[DEBUG_BUF_SIZE];
	_ramdisk_file * r_file=NULL;
	int i;
	int current_offset=0; /* offset of the file during this writing, in byte; if sequential writing, it should equal the file's size */
	int max_capacity=0; /* current total capacity, current-blocks-number*block-capacity, in byte */
	int free_space=0;  /* remaining free space after the offset, in byte */

	/* block index, the last used block's index, in which there might be some space(will write in it first);
	 *  start from 0 */
	int block_index_offset=0;
	int block_free_space=0; /* free space in the block specified by "block_index_offset" */
	int new_block_number=0;
	/* in the block specified by block_index_offset, the byte offset; free byte space start from the point. */
	int block_byte_offset=0;
	char ** t_blocks=NULL;
	int total_free_space=0; /* total free space of this file's all blocks, including both initialized and un-initialized blocks */

	int buf_offset=0;
	int to_inti_block_num=0;
	int t_size=0; /* local copy of size, will be modified */
	long old_size = 0; /* total size before write. */

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_write called, path:%s, size:%d, offset:%d, fi:%ld\n", path, size, offset, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	//return size;

	if(path==NULL || strcmp(path, "/")==0 || '/'== (* (path+strlen(path)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_write, path is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}
	if(buf==NULL || size<0 || offset<0){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_write, path is NULL, or size<0, or offset<0\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	r_file = is_path_valid(path, FILE_REG);
	if(r_file==NULL)
		return -ENOENT;

	if( check_ramdisk_capacity(size)!=0 ){
#if OSP4_DEBUG
		memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
		sprintf(s, "r_write, ramdisk(%ld) will be full(%ld), cannot write(%d) more\n",
				ramdisk_engine->current_size, ramdisk_engine->max_size, size);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
		return -EFBIG;
	}

	current_offset = offset;
	max_capacity = r_file->block_number * BLOCK_SIZE;
	free_space = max_capacity - current_offset;
	block_index_offset = current_offset / BLOCK_SIZE;
	block_free_space = (block_index_offset+1)*BLOCK_SIZE - current_offset;
	block_byte_offset = BLOCK_SIZE - block_free_space ; /* offset is, used bytes in the block (the starting point of free bytes in the block) */
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_write, size:%d, current_offset:%d, max_capacity:%d, block_index_offset:%d, block_free_space:%d, block_byte_offset:%d\n",
			size, current_offset, max_capacity, block_index_offset, block_free_space, block_byte_offset);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	if( block_index_offset<r_file->block_number/* if block_index_offset is valid */ && r_file->blocks[block_index_offset] == NULL){  /* initialize a block if needed */
		r_file->blocks[block_index_offset] = (char*)malloc( sizeof(char)*BLOCK_SIZE + 1);
		if( r_file->blocks[block_index_offset] ==NULL ){
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr, "r_write, memory error, r_file->blocks[block_index_offset]\n", LEVEL_ERROR);
#endif
			return -EPERM;
		}
		memset(r_file->blocks[block_index_offset], 0, sizeof(char)*BLOCK_SIZE + 1);
	}

	if( (current_offset+size)<= max_capacity /* don't need to add more blocks, don't need to increase block's number */
			&& block_free_space >= size/* if the current block has enough space to write in */) {
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr, "r_write, if-don't need to add more blocks; current block is enough\n", LEVEL_DEBUG);
#endif
			memcpy( r_file->blocks[block_index_offset]+block_byte_offset, buf, size);

	} //else if( (current_offset+size)<= max_capacity && block_free_space < size){ /* don't need to add more blocks, but current block's space is insufficient */
	//}
	else{
		/* ----------> calculate how many blocks to init(allocate block space); excluding current block */
		total_free_space = max_capacity - current_offset;
		to_inti_block_num = 0; /* don't be confused this with block_index_offset */

		if( (current_offset+size) > max_capacity ){

			if ( ( (size - total_free_space /*- block_free_space*/ ) % BLOCK_SIZE) != 0 ) {
				to_inti_block_num = ( size - total_free_space /*- block_free_space*/) / BLOCK_SIZE + 1;
			} else {
				to_inti_block_num = ( size - total_free_space /*- block_free_space*/) / BLOCK_SIZE;
			}

			/* corner case, total_free_space is 0, block_index_offset is the first un-allocated block;
			 * such as, write-size:4096, BLOCK:16, total_blocks:256, total-size:4096;
			 * because, block_index_offset refers to a block which hasn't been allocated yet,
			 * so, minus to_inti_block_num by 1, to avoid possible issues during initialization&writing */
			if( block_index_offset==r_file->block_number )
				to_inti_block_num--;

			/* condition like, write-size is bigger than max_capacity, such as size:256, block: 16;
			 * so, some of original blocks are also needed to be initialized */
			if ( block_index_offset < (r_file->block_number - 1) ) {
				to_inti_block_num += (r_file->block_number - block_index_offset - 1);
			}

		}else if( (current_offset+size) <= max_capacity && block_free_space < size){
			/* max_capacity is enough, but the current block doesn't have enough space to write in */

			if( ( (size-block_free_space) % BLOCK_SIZE) != 0) {
				to_inti_block_num = ( size - block_free_space) / BLOCK_SIZE + 1;
			} else {
				to_inti_block_num = ( size -block_free_space) / BLOCK_SIZE;
			}
		}else{
#if OSP4_DEBUG
			memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
			sprintf(s, "r_write, calculate to_inti_block_num, unknown error, total_free_space:%d, r_file->block_number:%d\n",
					total_free_space, r_file->block_number);
			chat_log_level(ramdisk_engine->lgr, s, LEVEL_ERROR);
#endif
		}


#if OSP4_DEBUG
		memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
		sprintf(s, "r_write, %d blocks to be initialized\n", to_inti_block_num);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


		/* ----------> must add more blocks, increase block's number;
		 * use while, because if size very big, once block-number increase might not enough; */
		while( (current_offset+size) > max_capacity ){
			new_block_number = r_file->block_number * BLOCK_INCR_FACTOR;
#if OSP4_DEBUG
			memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
			sprintf(s,	"r_write, must add more blocks, new blocks number:%d\n", new_block_number);
			chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			t_blocks = (char**) malloc(sizeof(char*) * new_block_number + 1);
			if (t_blocks == NULL) {
#if OSP4_DEBUG
				chat_log_level(ramdisk_engine->lgr, "r_write, memory error, t_blocks\n", LEVEL_ERROR);
#endif
				return -EPERM;
			}
			memset(t_blocks, 0, sizeof(char*) * new_block_number + 1);
			memcpy(t_blocks, r_file->blocks, sizeof(char*) * r_file->block_number);   /* only need to copy blocks address */
			free(r_file->blocks);
			r_file->blocks = t_blocks;
			r_file->block_number = new_block_number;

			max_capacity = r_file->block_number * BLOCK_SIZE; /* max_capacity is updated */
		} /* end of while */


		/* ----------> initialize more blocks if needed */
		for (i = 0; i <= to_inti_block_num; i++) {
			if( (block_index_offset + i) >= r_file->block_number ) {  /* if some unknown error */
#if OSP4_DEBUG
				memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
				sprintf(s, "r_write, block init error, (block_index_offset[%d]+i[%d])>=r_file->block_number[%d]\n",
						block_index_offset, i, r_file->block_number);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_ERROR);
				continue; /* try to skip */
#endif
			} /* end if unknown error */
			if (r_file->blocks[block_index_offset + i] == NULL) {
				r_file->blocks[block_index_offset + i] = (char*) malloc(sizeof(char) * BLOCK_SIZE + 1);
				if (r_file->blocks[block_index_offset + i] == NULL) {
#if OSP4_DEBUG
					chat_log_level(ramdisk_engine->lgr,	"r_write, memory error, r_file->blocks[block_index_offset+i]\n", LEVEL_ERROR);
#endif
					return -EPERM;
				}
				memset(r_file->blocks[block_index_offset + i], 0, sizeof(char) * BLOCK_SIZE + 1);
			}
		} /* end of for */
#if OSP4_DEBUG
		memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
		sprintf(s, "r_write, the last block's index during block init, block_index_offset[%d] + i[%d] = %d\n",
						block_index_offset, i - 1, block_index_offset + i - 1);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


		/* ----------> start actually writing */
		t_size = size;
		if( size<= block_free_space){  /* condition like, originally all blocks are full, add more blocks, only some space of the first new block is needed */
			memcpy(r_file->blocks[block_index_offset] + block_byte_offset, buf,	size);
		}else{
			memcpy(r_file->blocks[block_index_offset] + block_byte_offset, buf,	block_free_space); /* write to current block first */
			buf_offset = block_free_space;
			t_size -= block_free_space;
			for (i = 1; i <= to_inti_block_num; i++) {
				/* if some unknown error */
				if( (block_index_offset+i)>=r_file->block_number || r_file->blocks[block_index_offset+i]==NULL ){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
					sprintf(s, "r_write, write error, (block_index_offset[%d]+i[%d])>=r_file->block_number[%d], "
							"or r_file->blocks[block_index_offset+i]==NULL.  t_size:%d\n", block_index_offset, i, r_file->block_number, t_size);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_ERROR);
#endif
					continue; /* try to skip this unknown error */
				} /* end if unknown error */

				if (t_size > BLOCK_SIZE) { /* write to a whole block */
					memcpy(r_file->blocks[block_index_offset + i],	buf + buf_offset, BLOCK_SIZE);
					t_size -= BLOCK_SIZE;
					buf_offset += BLOCK_SIZE;
				} else { /* t_size <= BLOCK_SIZE */
					memcpy(r_file->blocks[block_index_offset + i],	buf + buf_offset, t_size);
				}
			} /* end of for */
#if OSP4_DEBUG
			memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
			sprintf(s, "r_write, the last block's index during block writing, block_index_offset[%d] + i[%d] = %d\n",
					block_index_offset, i-1, block_index_offset + i-1);
			chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
		}

	}  /* end of outter if-else */

	old_size = ramdisk_engine->current_size;
	/* update the size of r_file and ramdisk_engine */
	r_file->size += size;
	ramdisk_engine->current_size += size;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_write finish, old total size:%ld, new total size:%ld, r_file's size:%d, r_file's block number:%d\n",
			old_size, ramdisk_engine->current_size, r_file->size, r_file->block_number);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	return size;
}

/** Remove a file */
int r_unlink(const char * path){
	char s[DEBUG_BUF_SIZE];
	char ** file_names;
	int i, count, flag;
	_ramdisk_file * r_file, * r_file_p, * r_file_c;
	int ret = 0;
	long old_size=0;  /* total size before remove, for debug */

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_unlink called, path:%s\n", path);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	if( path==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_unlink, path is NULL\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}
	if (strcmp(path, "/") == 0) {
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_unlink, path is /, invalid\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	file_names = break_paths(path, &count);
	if( file_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_unlink, cannot create file_names\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}


	/* check parent dir exist, and get attributes */
	if(count!=-1){
		if(count!=0){ /* if more than one level dir */
			for(i=0;i<=count-1;i++){ /* this for only checks if parent dis exist, doesn't create dir */
				if(i==0){
					r_file_p = is_file_exist(file_names[i], ramdisk_engine->root_files);
					if(r_file_p==NULL || r_file_p->type==FILE_REG){
#if OSP4_DEBUG
						memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
						sprintf(s, "r_unlink, dir:%s doesn't exist or it's REG-file, cannot continue, 1\n", file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret=-EPERM;
						break;
					}
					continue;
				}
				r_file_c = is_file_exist(file_names[i], r_file_p->sub_files);
				if(r_file_c==NULL || r_file_c->type==FILE_REG ){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_unlink, dir:%s doesn't exist or it's REG-file, cannot continue, 2\n", file_names[i]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					ret=-EPERM;
					break;
				}

				r_file_p = r_file_c;
			} /* end of for */
			if(ret==0){ /* if parent-dir check pass */
				r_file = is_file_exist(file_names[count], r_file_p->sub_files);
				if (r_file == NULL || r_file->type==FILE_DIR) { /* if the specified file doesn't exist or it's DIR file */
					ret = -EPERM;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_unlink, %s doesn't exist under %s-dir or it's a DIR, cannot delete file\n", file_names[count], r_file_p->name);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}else{
					del_l_elem( r_file , r_file_p->sub_files);
				}
			}
		}else{/* count==0, something like, "/t1"; */
			r_file = is_file_exist(file_names[count], ramdisk_engine->root_files);
			if( r_file==NULL || r_file->type==FILE_DIR){ /* if the specified file doesn't exist or it's DIR file */
				ret = -EPERM;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_unlink, %s doesn't under root-dir or it's REG file, cannot rmdir\n", file_names[count]);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}else{
				del_l_elem( r_file, ramdisk_engine->root_files);
			}
		}

	}else{
		/* some unknown mistakes */
		ret = -EPERM;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_unlink, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
	}

	for(i=0; i<=count; i++)
		free(file_names[i]);
	free(file_names);

	if(ret==0){
		old_size = ramdisk_engine->current_size;
		ramdisk_engine->current_size -= r_file->size;
#if OSP4_DEBUG
		memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
		sprintf(s, "r_unlink, old total size:%ld, new total size:%ld\n",
				old_size, ramdisk_engine->current_size);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

		/* free file, file_names */
		for (i = 0; i < r_file->block_number; i++)
			free(r_file->blocks[i]);
		free(r_file->blocks);
		free(r_file->name);
		free(r_file->p_name);
		free(r_file);
	}

	return ret;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().	This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.	It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int r_flush (const char * path, struct fuse_file_info * fi){
	char s[DEBUG_BUF_SIZE];
	_ramdisk_file * r_file;
	int ret = 0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_flush called, path:%s, fi:%ld\n", path, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif



	if(path==NULL || strcmp(path, "/")==0 || '/'== (* (path+strlen(path)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_flush, path is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}



	r_file = is_path_valid(path, FILE_REG);

	if(r_file==NULL)
		ret = -ENOENT;

	return ret;
}

/** Open directory
 *
 * Unless the 'default_permissions' mount option is given,
 * this method should check if opendir is permitted for this
 * directory. Optionally opendir may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to readdir, closedir and fsyncdir.
 *
 * Introduced in version 2.3
 */
int r_opendir (const char * path, struct fuse_file_info * fi){
	char s[DEBUG_BUF_SIZE];
	_ramdisk_file * r_file;
	int ret = 0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_opendir called, path:%s, fi:%ld\n", path, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	if(path==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_open, path is NULL\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}
	if( strcmp(path, "/")==0 ){
		return ret;
	}
	if( '/'== (* (path+strlen(path)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_open, path ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}

	r_file = is_path_valid(path, FILE_DIR);

	if(r_file==NULL)
		ret = -ENOENT;

	return ret;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

static int r_readdir(const char *path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	char s[DEBUG_BUF_SIZE];
	char ** file_names;
	int count=0, i;
	int ret = 0;
	_list_elem * lp;
	struct stat st;
	_ramdisk_file * r_file=NULL, * r_file_c=NULL, * r_file_p=NULL;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_readdir called, path:%s, offset:%d, fi:%ld\n", path, offset, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if( path==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_readdir, path is NULL\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	if (strcmp(path, "/") == 0){  /* special case, if to read root-dir */
		lp = ramdisk_engine->root_files->l_head;
		while(lp!=NULL){
			memset(&st, 0, sizeof(struct stat));
			r_file = (_ramdisk_file*)lp->data;

			st.st_uid = ramdisk_engine->uid;
			st.st_gid = ramdisk_engine->gid;
			st.st_atime = ramdisk_engine->time;
			st.st_mtime = ramdisk_engine->time;
			st.st_ctime = ramdisk_engine->time;

			if (r_file->type == FILE_REG) {
				st.st_mode = S_IFREG | PERM_FILE;
				st.st_nlink = 1;
				st.st_size = r_file->size;
				/* st.st_blksize = BLOCK_SIZE;
				st.st_blocks = r_file->block_number; */
			} else {
				st.st_mode = S_IFDIR | PERM_DIR;
				st.st_nlink = 2;
				st.st_size = DIR_SIZE;
			}
			filler(buf, r_file->name, &st, 0);

			lp=lp->next;
		}  /* end of while */
	} else { /* if more than one level dir; check parent existance first, and get files' attributes */

		file_names = break_paths(path, &count);
		if (file_names == NULL) {
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr,
					"r_readdir, cannot create file_names\n", LEVEL_ERROR);
#endif
			return -EPERM;
		}

		/* check parent dir exist, and get files' attributes */
		if (count != -1) {
			if (count != 0) { /* if count!=0, something like, /home/wenzhao */
				for (i = 0; i <= count - 1; i++) { /* this for only checks if parent dis exist, doesn't create dir */
					if (i == 0) {
						r_file_p = is_file_exist(file_names[i],
								ramdisk_engine->root_files);
						if (r_file_p == NULL || r_file_p->type == FILE_REG) {
#if OSP4_DEBUG
							memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
							sprintf(s, "r_readdir, dir:%s doesn't exist or it's REG-file, cannot continue, 1\n",
									file_names[i]);
							chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
							ret = -ENOENT;
							break;
						}
						continue;
					}
					r_file_c = is_file_exist(file_names[i],
							r_file_p->sub_files);
					if (r_file_c == NULL || r_file_c->type == FILE_REG) {
#if OSP4_DEBUG
						memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
						sprintf(s, "r_readdir, dir:%s doesn't exist or it's REG-file, cannot continue, 2\n",
								file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret = -ENOENT;
						break;
					}

					r_file_p = r_file_c;
				} /* end of for */
				if (ret == 0) { /* if parent-dir check pass, check the last dir  */
					r_file = is_file_exist(file_names[count],
							r_file_p->sub_files);
					if (r_file == NULL || r_file->type == FILE_REG) { /* if file exists, and it's not a dir */
						ret = -ENOENT;
#if OSP4_DEBUG
						memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
						sprintf(s, "r_readdir, dir:%s doesn't exist under %s-dir or it's REG-file, cannot continue, 3\n",
								file_names[count], r_file_p->name);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					}
				} /* end if(ret==0) */
			} else { /* count==0, something like, /home */
				r_file = is_file_exist(file_names[count],
						ramdisk_engine->root_files);
				if (r_file == NULL || r_file->type == FILE_REG) { /* if file exists, and it's not a dir */
					ret = -ENOENT;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
					sprintf(s, "r_readdir, dir:%s doesn't exist under root-dir or it's REG-file, cannot continue, 4\n",
							file_names[count]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}
			}

			if (ret == 0) {  /* r_file is not NULL */
				lp = r_file->sub_files->l_head;
				while (lp != NULL) {
					memset(&st, 0, sizeof(stat));
					r_file = (_ramdisk_file*) lp->data;

					st.st_uid = ramdisk_engine->uid;
					st.st_gid = ramdisk_engine->gid;
					st.st_atime = ramdisk_engine->time;
					st.st_mtime = ramdisk_engine->time;
					st.st_ctime = ramdisk_engine->time;

					if (r_file->type == FILE_REG) {
						st.st_mode = S_IFREG | PERM_FILE;
						st.st_nlink = 1;
						st.st_size = r_file->size;
						/* st.st_blksize = BLOCK_SIZE;
						st.st_blocks = r_file->block_number; */
					} else {
						st.st_mode = S_IFDIR | PERM_DIR;
						st.st_nlink = 2;
						st.st_size = DIR_SIZE;
					}
					filler(buf, r_file->name, &st, 0);

					lp = lp->next;
				} /* while (lp != NULL) */
			}

		} else {
			/* some unknown mistakes */
			ret = -ENOENT;
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr,
					"r_readdir, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
		}

		/* free file_names */
		for(i=0; i<=count; i++)
			free(file_names[i]);
		free(file_names);
	} /* end outside if-else*/

	return ret;
}

/** Create a directory
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 * */
int r_mkdir (const char * path, mode_t mode){
	char s[DEBUG_BUF_SIZE];
	char ** file_names=NULL;
	int i, count, flag;
	_ramdisk_file * r_file, * r_file_p, * r_file_c;
	int ret = 0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_mkdir called, path:%s\n", path);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if( path==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_mkdir, path is NULL\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}
	if (strcmp(path, "/") == 0) {
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_mkdir, path is /, invalid\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	if( check_ramdisk_capacity(DIR_SIZE)!=0 ){
#if OSP4_DEBUG
		memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
		sprintf(s, "r_mkdir, ramdisk(%ld) will be full(%ld), cannot write(%d) more\n",
				ramdisk_engine->current_size, ramdisk_engine->max_size, DIR_SIZE);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
		return -EFBIG;
	}

	file_names = break_paths(path, &count);
	if( file_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_mkdir, cannot create file_names\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	/* check parent dir exist, and get attributes */
	if(count!=-1){
		if(count!=0){ /* if more than one level dir */
			for(i=0;i<=count-1;i++){ /* this for only checks if parent dis exist, doesn't create dir */
				if(i==0){
					r_file_p = is_file_exist(file_names[i], ramdisk_engine->root_files);
					if(r_file_p==NULL || r_file_p->type==FILE_REG){
#if OSP4_DEBUG
						memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
						sprintf(s, "r_mkdir, dir:%s doesn't exist or it's REG-file, cannot continue, 1\n", file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret=-EPERM;
						break;
					}
					continue;
				}
				r_file_c = is_file_exist(file_names[i], r_file_p->sub_files);
				if(r_file_c==NULL || r_file_c->type==FILE_REG ){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_mkdir, dir:%s doesn't exist or it's REG-file, cannot continue, 2\n", file_names[i]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					ret=-EPERM;
					break;
				}

				r_file_p = r_file_c;
			} /* end of for */
			if(ret==0){ /* if parent-dir check pass */
				r_file = is_file_exist(file_names[count], r_file_p->sub_files);
				if (r_file != NULL) { /* if the specified file already exists */
					ret = -EPERM;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_mkdir, %s already exists under %s-dir, cannot mkdir again\n", file_names[count], r_file_p->name);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}else{
					en_list( create_l_elem( create_ramdisk_dir(file_names[count], r_file_p->name) ),r_file_p->sub_files);
				}
			}
		}else{/* count==0, something like, "/t1"; */
			r_file = is_file_exist(file_names[count], ramdisk_engine->root_files);
			if( r_file!=NULL ){ /* if file already exists */
				ret = -EPERM;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_mkdir, %s already exists under root-dir, cannot mkdir\n", file_names[count]);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}else{
				en_list(  create_l_elem( create_ramdisk_dir(file_names[count], "/") ), ramdisk_engine->root_files);
			}
		}

	}else{
		/* some unknown mistakes */
		ret = -EPERM;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_mkdir, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
	}

	/* free file_names */
	for(i=0; i<=count; i++)
		free(file_names[i]);
	free(file_names);

	if(ret==0)
		ramdisk_engine->current_size+=DIR_SIZE; /* add dir size */

	return ret;
}

/** Remove a directory */
int r_rmdir(const char * path){
	char s[DEBUG_BUF_SIZE];
	char ** file_names=NULL;
	int i, count, flag;
	_ramdisk_file * r_file=NULL, * r_file_p=NULL, * r_file_c=NULL;
	int ret = 0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_rmdir called, path:%s\n", path);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if( path==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rmdir, path is NULL\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}
	if (strcmp(path, "/") == 0) {
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rmdir, path is /, invalid\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	file_names = break_paths(path, &count);
	if( file_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rmdir, cannot create file_names\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}


	/* check parent dir exist, and get attributes */
	if(count!=-1){
		if(count!=0){ /* if more than one level dir */
			for(i=0;i<=count-1;i++){ /* this for only checks if parent dis exist, doesn't create dir */
				if(i==0){
					r_file_p = is_file_exist(file_names[i], ramdisk_engine->root_files);
					if(r_file_p==NULL || r_file_p->type==FILE_REG){
#if OSP4_DEBUG
						memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
						sprintf(s, "r_rmdir, dir:%s doesn't exist or it's REG-file, cannot continue, 1\n", file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret=-EPERM;
						break;
					}
					continue;
				}
				r_file_c = is_file_exist(file_names[i], r_file_p->sub_files);
				if(r_file_c==NULL || r_file_c->type==FILE_REG ){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_rmdir, dir:%s doesn't exist or it's REG-file, cannot continue, 2\n", file_names[i]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					ret=-EPERM;
					break;
				}

				r_file_p = r_file_c;
			} /* end of for */
			if(ret==0){ /* if parent-dir check pass */
				r_file = is_file_exist(file_names[count], r_file_p->sub_files);
				if (r_file == NULL || r_file->type==FILE_REG) { /* if file doesn't exist or it's REG file */
					ret = -EPERM;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_rmdir, %s doesn't exist under %s-dir or it's REG-file, cannot rmdir\n", file_names[count], r_file_p->name);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}else if( r_file->sub_files->elem_number!=0 ){  /* if dir not empty */
					ret = -EPERM;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_rmdir, %s is not empty, cannot rmdir\n", file_names[count] );
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}else{
					del_l_elem( r_file , r_file_p->sub_files);
				}
			}
		}else{/* count==0, something like, "/t1"; */
			r_file = is_file_exist(file_names[count], ramdisk_engine->root_files);
			if( r_file==NULL || r_file->type==FILE_REG){ /* if file doesn't exist or it's REG file */
				ret = -EPERM;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_rmdir, %s doesn't under root-dir or it's REG file, cannot rmdir\n", file_names[count]);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}else if( r_file->sub_files->elem_number!=0 ){ /* if dir not empty */
				ret = -EPERM;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_rmdir, %s is not empty, cannot rmdir\n", file_names[count] );
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}else{
				del_l_elem( r_file, ramdisk_engine->root_files);
			}
		}

	}else{
		/* some unknown mistakes */
		ret = -EPERM;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rmdir, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
	}

	/* free file_names */

	for(i=0; i<=count; i++)
		free(file_names[i]);
	free(file_names);

	if(ret==0){
		free(r_file->name);
		free(r_file->p_name);
		free(r_file);
		ramdisk_engine->current_size-=DIR_SIZE; /* delete dir size */
	}

	return ret;
}


static int r_getattr(const char *path, struct stat *stbuf){
	char s[DEBUG_BUF_SIZE];
	char ** file_names;
	int i, count, flag;
	_ramdisk_file * r_file, * r_file_p, * r_file_c;
	int ret = 0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_getattr called, path:%s\n", path);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if( path==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_getattr called, path is NULL\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | PERM_DIR;
		stbuf->st_nlink = 2;
		stbuf->st_uid = ramdisk_engine->uid;
		stbuf->st_gid = ramdisk_engine->gid;
		stbuf->st_atime = ramdisk_engine->time;
		stbuf->st_mtime = ramdisk_engine->time;
		stbuf->st_ctime = ramdisk_engine->time;
		stbuf->st_size = DIR_SIZE;

#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_getattr, got / attributes\n", LEVEL_DEBUG);
#endif

		return ret;
	}

	if('/'== (* (path+strlen(path)-1) )/* if path is like, "/t1/t2/t3/", invalid */ ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_getattr, path ended with /\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	file_names = break_paths(path, &count);
	if( file_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_getattr, cannot create file_names\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	/* check parent dir exist, and get attributes */
	if(count!=-1){
		if(count!=0){ /* if more than one level dir */
			for(i=0;i<=count-1;i++){ /* this for only checks if parent dis exist, doesn't create dir */
				if(i==0){
					r_file_p = is_file_exist(file_names[i], ramdisk_engine->root_files);
					if(r_file_p==NULL || r_file_p->type==FILE_REG){
#if OSP4_DEBUG
						memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
						sprintf(s, "r_getattr, dir:%s doesn't exist or it's REG-file, cannot continue, 1\n", file_names[i]);
						chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
						ret=-ENOENT;
						break;
					}
					continue;
				}
				r_file_c = is_file_exist(file_names[i], r_file_p->sub_files);
				if(r_file_c==NULL || r_file_c->type==FILE_REG ){
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_getattr, dir:%s doesn't exist or it's REG-file, cannot continue, 2\n", file_names[i]);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
					ret=-ENOENT;
					break;
				}

				r_file_p = r_file_c;
			} /* end of for */
			if(ret==0){ /* if parent-dir check pass */
				r_file = is_file_exist(file_names[count], r_file_p->sub_files);
				if (r_file == NULL) { /* if file exists */
					ret = -ENOENT;
#if OSP4_DEBUG
					memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
					sprintf(s, "r_getattr, %s doesn't exists under %s-dir\n", file_names[count], r_file_p->name);
					chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				}
			}
		}else{/* count==0, something like, "/t1"; */
			r_file = is_file_exist(file_names[count], ramdisk_engine->root_files);
			if( r_file==NULL ){ /* if file exists */
				ret = -ENOENT;
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_getattr, %s doesn't exists under root-dir\n", file_names[count]);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			}
		}
		if(ret==0){  /* r_file is not NULL */
#if OSP4_DEBUG
			memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
			sprintf(s, "r_getattr, to get attributes of %s\n", r_file->name);
			chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			stbuf->st_uid = ramdisk_engine->uid;
			stbuf->st_gid = ramdisk_engine->gid;
			stbuf->st_atime = ramdisk_engine->time;
			stbuf->st_mtime = ramdisk_engine->time;
			stbuf->st_ctime = ramdisk_engine->time;

			if (r_file->type == FILE_REG) {
				stbuf->st_mode = S_IFREG | PERM_FILE;
				stbuf->st_nlink = 1;
				stbuf->st_size = r_file->size;
				/*stbuf->st_blksize = BLOCK_SIZE;
				stbuf->st_blocks = r_file->block_number;*/
			} else {
				stbuf->st_mode = S_IFDIR | PERM_DIR;
				stbuf->st_nlink = 2;
				stbuf->st_size = DIR_SIZE;
			}
		}
	}else{
		/* some unknown mistakes */
		ret = -ENOENT;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_getattr, unknown mistakes, count is -1\n", LEVEL_ERROR);
#endif
	}

	/* free file_names */
	for(i=0; i<=count; i++)
		free(file_names[i]);
	free(file_names);


	return ret;
}


/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
static int r_getattr_wrapper(const char *path, struct stat *stbuf){
	char s[DEBUG_BUF_SIZE];
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_getattr_wrapper called, path:%s\n", path);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	return r_getattr(path, stbuf);
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
static int r_fgetattr_wrapper (const char * path, struct stat * buf, struct fuse_file_info * fi){
	char s[DEBUG_BUF_SIZE];
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_fgetattr_rapper called, path:%s, fi:%ld \n", path, fi);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	return r_getattr(path, buf);
}


int r_truncate(const char * path, off_t offset){
	char s[DEBUG_BUF_SIZE];
	_ramdisk_file * r_file;
	int ret=0;
	long old_size=0;
	int i=0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_truncate called, path:%s, offset:%d \n", path, offset);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	if(path==NULL || strcmp(path, "/")==0 || '/'== (* (path+strlen(path)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_open, path is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}

	r_file = is_path_valid(path, FILE_REG);
	if(r_file==NULL)
		return -ENOENT;

	if( offset==0 ){ /* clear the entire content of the file */
		old_size = ramdisk_engine->current_size;
		ramdisk_engine->current_size -= r_file->size;
#if OSP4_DEBUG
		memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
		sprintf(s, "r_truncate, old total size:%ld, new total size:%ld\n", old_size, ramdisk_engine->current_size);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

		/* free blocks, and recreate */
		for (i = 0; i < r_file->block_number; i++)
			free(r_file->blocks[i]);
		free(r_file->blocks);
		r_file->blocks = (char**) malloc(sizeof(char*) * INIT_BLOCK_NUM + 1);
		if (r_file->blocks == NULL) {
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr,
					"r_truncate, memory error1\n", LEVEL_ERROR);
#endif
			return -EPERM;
		}
		memset(r_file->blocks, 0, sizeof(char*) * INIT_BLOCK_NUM + 1);
		r_file->block_number=INIT_BLOCK_NUM;
		r_file->size=0;

	}else{
		if(offset==r_file->size)
			return 0; /* success */

#if OSP4_DEBUG
		memset(s, 0, sizeof(char) * DEBUG_BUF_SIZE);
		sprintf(s, "r_truncate, unimplemented branch, r_file->size:%d, offset:%d\n", r_file->size, offset);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_ERROR);
#endif
		return -EPERM;
		/*else if( offset<r_file->size){

		}else{   //offset > file's size,

		}*/
	}

	return ret;
}

/** Change the size of a file */
int r_truncate_wrapper(const char * path, off_t offset){
	char s[DEBUG_BUF_SIZE];
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_truncate_wrapper called, path:%s, offset:%d \n", path, offset);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	return r_truncate(path, offset);
}


/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int r_ftruncate_wrapper(const char * path, off_t offset, struct fuse_file_info * fi){
	char s[DEBUG_BUF_SIZE];
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_ftruncate_wrapper called, path:%s, offset:%d\n", path, offset);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif

	return r_truncate(path, offset);
}

static int r_rename(const char *from, const char *to){
	char s[DEBUG_BUF_SIZE];
	int ret=0;
	_ramdisk_file * r_from=NULL, * r_to_parent=NULL;
	char ** to_names=NULL;
	int to_count;
	int i=0;
	/*_list * to_parent_sub_files=NULL;  to's parent's sub_files; this is the list to which the from-file will be added */
	char * to_parent=NULL; /* to's parent's name */
	int t_index=0;

#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_rename called, from:%s, to:%s \n", from, to);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	if(from==NULL || strcmp(from, "/")==0 || '/'== (* (from+strlen(from)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rename, from is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}
	if(to==NULL || strcmp(to, "/")==0 || '/'== (* (to+strlen(to)-1)) ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rename, to is NULL, or just /, or ended with /\n", LEVEL_ERROR);
#endif
		return -ENOENT;
	}

	r_from = is_path_valid(from, FILE_IGN);  /* from must exist */
	if(r_from==NULL){
#if OSP4_DEBUG
		memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
		sprintf(s, "r_rename, from(%s) doesn't exist\n", from);
		chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
		return -EPERM;
	}

	to_names = break_paths(to, &to_count);
	if( to_names==NULL ){
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rename, cannot create to_names\n", LEVEL_ERROR);
#endif
		return -EPERM;
	}

	/* extract to's parent */
	if (to_count != -1) {
		if (to_count >= 1) { /* if more than one level dir */
			to_parent = (char*)malloc( sizeof(char)*(strlen(to)+to_count+1)+1 );
			if(to_parent==NULL){
#if OSP4_DEBUG
				chat_log_level(ramdisk_engine->lgr, "r_rename, cannot create to_parent, memory error\n", LEVEL_ERROR);
#endif
				return -EPERM;
			}
			memset(to_parent, 0, sizeof(char)*(strlen(to)+to_count+1)+1 );
			for (i = 0; i <= to_count - 1; i++) { /* this for only checks if parent dis exist, doesn't create dir */
				strcat(to_parent, "/");
				strcat(to_parent, to_names[i] );
			} /* end of for; end of constructing to's parent's full-path */

#if OSP4_DEBUG
			memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
			sprintf(s, "r_rename, to's parent's full path-name:%s\n", to_parent);
			chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
			r_to_parent = is_path_valid(to_parent, FILE_DIR);
			if(r_to_parent==NULL){
#if OSP4_DEBUG
				memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
				sprintf(s, "r_rename, to's parent's(%s) doesn't exist or it's not a dir\n", to_parent);
				chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif
				return -ENOENT;
			}
			/* to_parent_sub_files = r_to_parent->sub_files; */
		} else {/* count==0, something like, "/t1"; */
#if OSP4_DEBUG
			chat_log_level(ramdisk_engine->lgr, "r_rename, to's parent is root\n", LEVEL_DEBUG);
#endif
			/* to_parent_sub_files = ramdisk_engine->root_files; */
		}

		/* delete the from-file from its parent's list */
		if( strcmp(r_from->p_name, "/")==0 )
			del_l_elem(r_from, ramdisk_engine->root_files);
		else
			del_l_elem(r_from, r_from->parent->sub_files);

		/* update the from-file's name, change it to to's name */
		free( r_from->name );
		r_from->name = (char*)malloc( sizeof(char)*strlen(to_names[to_count]) + 1 );
		if(r_from->name==NULL){
			return -EPERM;
		}
		memset(r_from->name, 0, sizeof(char)*strlen(to_names[to_count]) + 1);
		memcpy(r_from->name, to_names[to_count], sizeof(char)*strlen(to_names[to_count]) );

		/* update the from-file's parent's info, p_name/parent; and add it to new dir */
		free( r_from->p_name );
		if( to_count == 0 ){ /* if the to-file will be under the root */
			r_from->p_name = (char*)malloc(sizeof(char)*2);
			if(r_from->p_name==NULL){
				return -EPERM;
			}
			memset( r_from->p_name, 0, sizeof(char)*2 );
			r_from->p_name[0]='/';

			r_from->parent = NULL;
			en_list( create_l_elem(r_from), ramdisk_engine->root_files);
		}else{
			r_from->p_name = (char*)malloc( sizeof(char)*strlen(to_names[to_count-1]) + 1 );
			if(r_from->p_name==NULL){
				return -EPERM;
			}
			memset( r_from->p_name, 0, sizeof(char)*strlen(to_names[to_count-1]) + 1 );
			memcpy( r_from->p_name, to_names[to_count-1], sizeof(char)*strlen(to_names[to_count-1]) );

			r_from->parent = r_to_parent;
			en_list( create_l_elem(r_from), r_to_parent->sub_files);
		}

	} else {
		/* some unknown mistakes */
		ret = -EPERM;
#if OSP4_DEBUG
		chat_log_level(ramdisk_engine->lgr, "r_rename, unknown mistakes, to_count is -1\n", LEVEL_ERROR);
#endif
	}


	/* free to_names */
	for(i=0; i<=to_count; i++)
		free(to_names[i]);
	free(to_names);


	return ret;
}

static int r_access(const char *path, int mask){
	char s[DEBUG_BUF_SIZE];
	int ret=0;
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_access called, path:%s, mask:%d\n", path, mask);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	return ret;
}

static int r_chmod(const char *path, mode_t mode){
	char s[DEBUG_BUF_SIZE];
	int ret=0;
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_chmod called, path:%s, mode:%d\n", path, mode);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	return ret;
}

static int r_chown(const char *path, uid_t uid, gid_t gid){
	char s[DEBUG_BUF_SIZE];
	int ret=0;
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_chmod called, path:%s, uid:%d, gid:%d\n", path, uid, gid);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	return ret;
}

static int r_utimens(const char * path, const struct timespec tv[2]){
	char s[DEBUG_BUF_SIZE];
	int ret=0;
#if OSP4_DEBUG
	memset(s, 0, sizeof(char)*DEBUG_BUF_SIZE);
	sprintf(s, "r_utimens called, path:%s\n", path);
	chat_log_level(ramdisk_engine->lgr, s, LEVEL_DEBUG);
#endif


	return ret;
}

static struct fuse_operations ramdisk_oper = {

	.open		= r_open,
	.create		= r_create,
	.read		= r_read,
	.write		= r_write,		//==> ssize_t write(int filedes, const void * buf , size_t nbytes ) in POSIX
	.unlink		= r_unlink,
	.flush		= r_flush,		//==> close() in POSIX
	.opendir	= r_opendir,
	.readdir	= r_readdir,
	.mkdir		= r_mkdir,
	.rmdir		= r_rmdir,
	.getattr	= r_getattr_wrapper,
	.fgetattr	= r_fgetattr_wrapper,	//==> int fstat(int pathname , struct stat * buf ) in POSIX
	.truncate	= r_truncate_wrapper,
	.ftruncate	= r_ftruncate_wrapper,

	.rename		= r_rename,
	.access		= r_access,
	.chmod		= r_chmod,
	.chown		= r_chown,
	.utimens	= r_utimens,
};


/*
 * ramdisk /path/to/dir 512
 */
int main(int argc, char *argv[]) {
	int size=0; /* argument size, MB */
	char * t_argv[2];
	if(argc!=3){
#if OSP4_DEBUG
		printf("USAGE: ./ramdisk <mount point dir> <size of ramdisk(MB)>\n");
#endif
		exit(1);
	}
	size = atoi(argv[2]);
	if(size<0){
#if OSP4_DEBUG
		printf("size parameter cannot be less than 0\n");
#endif
		exit(1);
	}

	t_argv[0]=argv[0];
	t_argv[1]=argv[1];

	ramdisk_engine = (_ramdisk_core*) malloc(sizeof(_ramdisk_core));
	ramdisk_engine->current_size = 0;
	ramdisk_engine->root_files = create_list();
	ramdisk_engine->time = time(NULL);
	ramdisk_engine->uid = getuid();
	ramdisk_engine->gid = getgid();
	ramdisk_engine->max_size = size*1024*1024 ; /* byte */
#if OSP4_DEBUG
	ramdisk_engine->lgr = create_logger(LEVEL_DEBUG, NULL);
#endif

	return fuse_main(argc-1, t_argv, &ramdisk_oper, NULL);

 	/*char * path="/";
 	char s[512];
	struct stat st;
	memset(&st, 1, sizeof(struct stat));
	int t_offset=0;
	int w_size = 256;
	int r_size = 8192;
	char read_buf[8192];
	//r_getattr(path, NULL);
	//r_readdir(path, s, NULL, 0, NULL);

	r_create("/hello", 0, NULL);
	while(t_offset<=5148){
		printf("%d\n", r_write("/hello", s, w_size, t_offset, NULL) );
		t_offset+=w_size;
	}

	t_offset=0;
	memset(read_buf, 0, sizeof(char)*8192 );
	while(t_offset<=8192){
		printf("%d\n", r_read("/hello", read_buf, r_size, t_offset, NULL) );
		t_offset+=r_size;
	}*/

}

