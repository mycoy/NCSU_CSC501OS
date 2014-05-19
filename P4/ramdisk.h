/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P4.
 *
 * The header for ramdisk.
 */

/* #define MAX_FS_SIZE 524288000  500MB, 500*1024*1024 bytes */
#ifndef OSP4_DEBUG
#define OSP4_DEBUG 0
#endif

#define FILE_REG 1
#define FILE_DIR 2
#define FILE_IGN 3 /* don't care the file's type */
#define BLOCK_SIZE 2048 /* in bytes */
#define INIT_BLOCK_NUM 4 /* when a file is created, how many initial blocks should it have */
#define BLOCK_INCR_FACTOR 2 /* define how blocks number increases, original-value*factor */

typedef struct _list_elem_st{
	void * data;  /* data will be ramdisk file */
	struct _list_elem_st * next;
}_list_elem;

typedef struct _list_st{
	int elem_number; /* number of elements in the list */
	_list_elem * l_head;
	_list_elem * l_tail;

}_list;

typedef struct _ramdisk_file_st{
	int type;  /* dir or regular file */

	/*
	 * under FUSE, a file, "/t1/t2/hello",
	 * name will be "hello"
	 * full path name is "/t1/t2/hello"
	 */
	char * name;
	char * p_name; /* parent dir name */
	struct _ramdisk_file_st * parent;  /* parent pointer, for root-parent, this field is NULL, should check p_name */

	int size; /* if regular file, total size of the file, in bytes */
	int block_number;  /* if regular file, how many blocks */
	char ** blocks;   /* if regular file, store content of a regular file; every element is a char-pointer to a block */
	int openned;  /* if regular file, indicates if the file is openned, 1: open, 0: not open */

	_list * sub_files;/* if dir, a list of sub-files */

}_ramdisk_file;


_list * create_list();

_list_elem * create_l_elem(_ramdisk_file * data);

/*void decompose_list(_list * q);

void decompose_l_elem(_list_elem * le);*/

/**
 * add an element to the list (add to the end of the list);
 * return: 0 success, 1 error;
 */
int en_list(_list_elem * e, _list * l);

/*
 * return:
 * non-NULL: the pointer to the list-elem(file-structure) with the specified name;
 * NULL: not exist
 *
 * check if the specified _ramdisk_file is in the files-list;
 * compare by "name" field in _ramdisk_file
 */
_ramdisk_file * is_file_exist(char * name, _list * l_t);

/**
 * delete the file from the file-list.
 * users should use "is_file_exit" to get the element pointer first, and call this function to delete.
 * return: 0 success, 1 error;
 *
 * if element doesn't exit, return fail;
 *
 * this function will not free the to-be-deleted element.
 */
int del_l_elem(_ramdisk_file * le_t, _list * l_t);
