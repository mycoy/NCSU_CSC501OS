/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P4.
 *
 * Functions for list of ramdisk.
 * Specially created for OSP4 ramdisk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ramdisk.h"

#ifndef LIST_DEBUG
#define LIST_DEBUG 0
#endif

_list * create_list(){
	_list * l_t = NULL;

	l_t = (_list*)malloc(sizeof(_list));

	if(l_t==NULL){
#if LIST_DEBUG
		printf("create_list, memory allocation error\n");
#endif
		exit(1);
	}
	l_t->elem_number = 0;
	l_t->l_head = NULL;
	l_t->l_tail = NULL;

	return l_t;
}

_list_elem * create_l_elem(_ramdisk_file * data){
	_list_elem * le_t = NULL;

	if(data==NULL){
#if LIST_DEBUG
		printf("data is NULL\n");
#endif
		return NULL;
	}

	le_t = (_list_elem*)malloc( sizeof(_list_elem) );
	if(le_t==NULL){
#if LIST_DEBUG
		printf("create_l_elem, memory allocation error\n");
#endif
		exit(1);
	}

	le_t->data = data;
	le_t->next = NULL;

	return le_t;
}

/*void decompose_list(_list * q){
}

void decompose_l_elem(_list_elem * le){
}*/

int en_list(_list_elem * e_t, _list * l_t){
	_list * l = NULL;
	_list_elem * le = NULL;

	if(e_t==NULL || l_t==NULL){
#if LIST_DEBUG
		printf("en_list, list_elem or list is NULL\n");
#endif
		return 1;
	}

    l = l_t;
    le = e_t;
    if(l->elem_number==0){ /* if list empty */
    	l->l_head = l->l_tail = le;
    }else{
    	l->l_tail->next = le;
    	l->l_tail = le;
    }
    l->elem_number++;

    return 0;
}

_ramdisk_file * is_file_exist(char * name, _list * l_t){
	_list * l = NULL;
	_list_elem * lp=NULL;
	_list_elem * found=NULL;
	int i, size;

	if(name==NULL ){
#if LIST_DEBUG
		printf("is_file_exist, name is NULL\n");
#endif
		return NULL;
	}
	if(l_t==NULL ){
#if LIST_DEBUG
		printf("is_file_exist, list is NULL\n");
#endif
		return NULL;
	}
	if(l_t->elem_number==0 ){
#if LIST_DEBUG
		printf("is_file_exist, list-size is zero\n");
#endif
		return NULL;
	}

    l = l_t;

    size = l->elem_number;
    lp = l->l_head;
    for(i=0; i< size; i++){
    	if( strcmp(name, ((_ramdisk_file*)lp->data)->name) == 0 ){ /* if found */
    		found=lp;
    		return (_ramdisk_file*)found->data;
    	}
    	lp = lp->next;
    }

    return NULL;
}

int del_l_elem(_ramdisk_file * le_t, _list * l_t){
	_list * l = NULL;
	_list_elem * lp1=NULL, * lp2=NULL, * to_del=NULL;
	int i, size;
	_ramdisk_file * le = NULL;

	if(le_t==NULL || l_t==NULL){
#if LIST_DEBUG
		printf("del_l_elem, list_elem or list is NULL\n");
#endif
		return 1;
	}

    l = l_t;
    le = le_t;

    size = l->elem_number;
    lp1 = l->l_head;
    for(i=0; i< size; i++){
    	if( lp1->data == le ){ /* if found */
    		to_del=lp1;
    		if(lp1==l->l_head){ /* if to_del is the head */
    			if(l->elem_number==1){ /* if only one elem in the list */
    				l->l_head = l->l_tail = NULL;
    			}else{
    				l->l_head = l->l_head->next;
    			}
    			break;
    		} else if(lp1==l->l_tail){
    			l->l_tail = lp2;
    			lp2->next = NULL; /* added at 20:20PM Dec6 2013 */
    			break;
    		}else{
    			lp2->next = lp1->next;
    			break;
    		}
    	}
    	lp2=lp1;
    	lp1 = lp1->next;
    } /* end for */

    if(to_del!=NULL){
    	to_del->next = NULL; /* cut relation with the list */
    	l->elem_number--;
    }

    return to_del!=NULL ? 0 : 1;
}

