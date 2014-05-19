/**
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * Queue for threads, implementation file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"


typedef struct _queue_elem_st{
	/* char * desc; */
	void * data;
	struct _queue_elem_st * next;
}_queue_elem;

typedef struct _queue_st{
	int elem_number; /* number of elements in the queue */
	_queue_elem * q_head;
	_queue_elem * q_tail;

}_queue;


queue create_queue(){
	_queue * q_t = NULL;

	q_t = (_queue*)malloc(sizeof(_queue));

	if(q_t==NULL){
#ifdef QUEUE_DEBUG
		printf("q.c memory allocation error\n");
#endif
		exit(1);
	}
	q_t->elem_number = 0;
	q_t->q_head = NULL;
	q_t->q_tail = NULL;

	return (queue)q_t;
}

void decompose_queue(queue q){

}

queue_elem create_q_elem(void * data){
	_queue_elem * qe_t = NULL;

	if(data==NULL){
#ifdef QUEUE_DEBUG
		printf("data is NULL\n");
#endif
		return NULL;
	}

	qe_t = (_queue_elem*)malloc( sizeof(_queue_elem) );
	if(qe_t==NULL){
#ifdef QUEUE_DEBUG
		printf("q.c memory allocation error\n");
#endif
		exit(1);
	}

	qe_t->data = data;
	qe_t->next = NULL;

	return (queue_elem)qe_t;
}

void decompose_q_elem(queue_elem qe){

}

int en_queue(queue_elem e_t, queue q_t){
	_queue * q = NULL;
	_queue_elem * qe = NULL;

	if(e_t==NULL || q_t==NULL){
#ifdef QUEUE_DEBUG
		printf("queue_elem or queue is NULL\n");
#endif
		return 1;
	}

    q = (_queue*)q_t;
    qe = (_queue_elem*)e_t;
    if(q->elem_number==0){ /* if queue empty */
    	q->q_head=q->q_tail=qe;
    }else{
    	q->q_tail->next =  qe;
    	q->q_tail = qe;

    	/*qe->next = q->q_head;
    	q->q_head = qe;*/
    }
    q->elem_number++;

    return 1;
}

queue_elem de_queue(queue q_t){
	_queue * q = NULL;
	_queue_elem * qe = NULL;

	if(q_t==NULL){
#ifdef QUEUE_DEBUG
		printf("queue is NULL\n");
#endif
		return NULL;
	}

	q = (_queue*)q_t;
	if(q->elem_number==0){
#ifdef QUEUE_DEBUG
		printf("queue.c, de_queue, no element in the queue\n");
#endif
		return NULL;
	}

	qe = (_queue_elem*)q->q_head;

	if(q->elem_number==1){   /* if only one element */
		q->q_head = q->q_tail = NULL;
	}else{
		q->q_head = q->q_head->next;
	}
	qe->next = NULL;  /* cut the elem's relation in the queue */
	q->elem_number--;

	return (queue_elem)qe;
}

void * get_queue_elem_content(queue_elem qe_t){
	_queue_elem * qe = NULL;

	if(qe_t==NULL){
#ifdef QUEUE_DEBUG
		printf("queue.c, get_queue_elem_content, queue_elem is NULL\n");
#endif
		return NULL;
	}
	qe=(_queue_elem*)qe_t;

	return qe->data;
}

int get_queue_size(queue q_t){
	_queue * q = NULL;

	if(q_t==NULL){
#ifdef QUEUE_DEBUG
		printf("queue is NULL\n");
#endif
		return 0;
	}

	q = (_queue*)q_t;

	return q->elem_number;
}




