/**
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * List for threads, implementation file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

typedef struct _list_elem_st{
	/* char * desc; */
	void * data;
	struct _list_elem_st * next;
}_list_elem;

typedef struct _list_st{
	int elem_number; /* number of elements in the list */
	_list_elem * l_head;
	_list_elem * l_tail;

}_list;

list create_list(){
	_list * l_t = NULL;

	l_t = (_list*)malloc(sizeof(_list));

	if(l_t==NULL){
#ifdef LIST_DEBUG
		printf("list.c memory allocation error\n");
#endif
		exit(1);
	}
	l_t->elem_number = 0;
	l_t->l_head = NULL;
	l_t->l_tail = NULL;

	return (list)l_t;
}

void decompose_list(list l){

}

list_elem create_l_elem(void * data){
	_list_elem * le_t = NULL;

	if(data==NULL){
#ifdef LIST_DEBUG
		printf("data is NULL\n");
#endif
		return NULL;
	}

	le_t = (_list_elem*)malloc( sizeof(_list_elem) );
	if(le_t==NULL){
#ifdef LIST_DEBUG
		printf("list.c memory allocation error\n");
#endif
		exit(1);
	}

	le_t->data = data;
	le_t->next = NULL;

	return (list_elem)le_t;
}

void decompose_l_elem(list_elem le){

}

int en_list(list_elem e_t, list l_t){
	_list * l = NULL;
	_list_elem * le = NULL;

	if(e_t==NULL || l_t==NULL){
#ifdef LIST_DEBUG
		printf("list_elem or list is NULL\n");
#endif
		return 1;
	}

    l = (_list*)l_t;
    le = (_list_elem*)e_t;
    if(l->elem_number==0){ /* if list empty */
    	l->l_head = l->l_tail = le;
    }else{
    	l->l_tail->next = le;
    	l->l_tail = le;
    }
    l->elem_number++;

    return 1;
}

list_elem de_list(list l_t){
	_list * l = NULL;
	_list_elem * le = NULL;

	if(l_t==NULL){
#ifdef LIST_DEBUG
		printf("list is NULL\n");
#endif
		return NULL;
	}

	l = (_list*)l_t;
	if(l->elem_number==0){
#ifdef LIST_DEBUG
		printf("no element in the list\n");
#endif
		return NULL;
	}

	le = (_list_elem*)l->l_head;

	if(l->elem_number==1){   /* if only one element */
		l->l_head = l->l_tail = NULL;
	}else{
		l->l_head = l->l_head->next;
	}
	le->next = NULL;  /* cut the elem's relation in the list */
	l->elem_number--;

	return (list_elem)le;
}

void * get_list_elem_content(list_elem le_t){
	_list_elem * le = NULL;

	if(le_t==NULL){
#ifdef LIST_DEBUG
		printf("list.c, get_list_elem_content, list_elem is NULL\n");
#endif
		return NULL;
	}
	le=(_list_elem*)le_t;

	return le->data;
}

int get_list_size(list l_t){
	_list * l = NULL;

	if(l_t==NULL){
#ifdef LIST_DEBUG
		printf("list is NULL\n");
#endif
		return 0;
	}

	l = (_list*)l_t;

	return l->elem_number;
}

int is_l_elem_exist(list_elem le_t, list l_t){
	_list * l = NULL;
	_list_elem * le = NULL, * lp=NULL;
	void * to_find_data=NULL;
	int i, size, found=0;

	if(le_t==NULL || l_t==NULL){
#ifdef LIST_DEBUG
		printf("list_elem or list is NULL\n");
#endif
		return 0;
	}

    l = (_list*)l_t;
    le = (_list_elem*)le_t;
    to_find_data = le->data;

    size = get_list_size(l_t);
    lp = l->l_head;
    for(i=0; i< size; i++){
    	if( lp->data==to_find_data ){ /* if found */
    		found=1;
    		break;
    	}
    	lp = lp->next;
    }
    return found;
}

list_elem del_l_elem(list_elem le_t, list l_t){
	_list * l = NULL;
	_list_elem * le = NULL, * lp1=NULL, * lp2=NULL, * to_del=NULL;
	void * to_find_data=NULL;
	int i, size, found=0;

	if(le_t==NULL || l_t==NULL){
#ifdef LIST_DEBUG
		printf("list_elem or list is NULL\n");
#endif
		return 0;
	}

    l = (_list*)l_t;
    le = (_list_elem*)le_t;
    to_find_data = le->data;

    size = get_list_size(l_t);
    lp1 = l->l_head;
    for(i=0; i< size; i++){
    	if(lp1->data == to_find_data){ /* if found */
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

    return (list_elem)to_del;
}


