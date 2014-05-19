/**
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * mythread.h's implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "mythread.h"
#include "queue.h"
#include "list.h"

#define THREAD_STACK_SIZE 8192  /* Allocate at least 8KB for each thread stack. */

#ifndef THREAD_DEBUG
#endif

/* used to swapcontext */
static ucontext_t ctx_buf;

/* pointer of this will be casted to MyThread */
typedef struct _thread_st{
	ucontext_t ctx;

	struct _thread_st * parent;

	struct _thread_st* joinedKid;

	list kids; /* all immediate kids */

	int is_joined; /* 0: not joined by one of its kids; parent perspective */
	int is_all_joined; /* 0: not joined by calling join-all; parent perspective */

	int joined_parent; /* 0: if its parent has not joined this thread; kid perspective */

}_thread;


/**
 * contains core global structures for MyThread library
 */
typedef struct _thread_core_st{
	_thread * running; /* currently running thread; this is non-preemptive lib, only one thread running at a time */
	_thread * root;    /* the oldest ancestor of all MyThreads */
	/* _thread * t_exit_handler; if the thread fall out of the start-fun(it doesn't call the exit), go to this handler*/

	queue ready_q;    /* ready queue */
	/*list t_exit_list;  if thread doesn't call exit, and fall out of their start-fun, hang them here */

	list joined_block_list;  /* if threads call join/join-all, hang them here */

	/*list sem_block_list;  this list contains semaphores, on which some threads might be blocked */

}_thread_core;


typedef struct _semaphore_st{
	int value;

	list blocked_list; /* put blocked thread on the list */
}_semaphore;


static _thread_core * thread_engine; /* core global variable */

/* check if the thread_engine has been initialized(if the init is called); if not, print an error msg and exit */
void _check_thread_engine(){
	if(thread_engine==NULL){
		printf("Please call MyThreadInit first!\n");
		exit(1);
	}
}

int _get_readyq_size(){ //return get_queue_size(
	 return get_queue_size( thread_engine->ready_q );
}


void _swap_run(){
	_thread * to_run, * running;

	running = thread_engine->running; /* get currently runnin */

	/* get the front thread from the ready q */
	to_run = (_thread*) get_queue_elem_content( de_queue(thread_engine->ready_q) );

	/* put the currently running to the front of the ready q*/
	en_queue(create_q_elem(running), thread_engine->ready_q);

	thread_engine->running = to_run; /* set the running pointer */

	swapcontext(&running->ctx, &to_run->ctx);
}



/* doesn't pre-empt */
MyThread MyThreadCreate(void(*start_funct)(void *), void *args){
	_thread * t, * running;

	_check_thread_engine();

	if(start_funct==NULL){
		printf("start_funct is NULL\n");
		exit(1);
	}

	t = (_thread*) malloc( sizeof(_thread) );

	if( t==NULL ){
#ifdef THREAD_DEBUG
		printf("thread,create, memory allocation error\n");
#endif
	}

	running = thread_engine->running;

	/* init the thread context */
    getcontext(& t->ctx);
    t->ctx.uc_stack.ss_sp = malloc( sizeof(char) * THREAD_STACK_SIZE );
    t->ctx.uc_stack.ss_size = sizeof(char) * THREAD_STACK_SIZE;
    t->parent = running;  /* set parent */
    t->joinedKid = NULL;
    t->kids = NULL;
    t->is_all_joined = 0;
    t->is_joined = 0;
    t->joined_parent = 0;
    memset( t->ctx.uc_stack.ss_sp, 0, t->ctx.uc_stack.ss_size );
    makecontext(& t->ctx, start_funct, 1, args);

    en_queue( create_q_elem(t), thread_engine->ready_q );  /* do not pre-empt the calling thread, only put the new thread to the ready q */

    if(running->kids==NULL) /* create kids-list when necessary */
    	running->kids = create_list();
    en_list(create_l_elem(t), running->kids);

    return (MyThread)t;
}

/*  */
void MyThreadYield(void){

	_check_thread_engine();

	if( _get_readyq_size()==0 ){ /* if the calling thread is the only thread, return */
		return;
	}else{
		_swap_run();
	}
}

int MyThreadJoin(MyThread thread){
	_thread * t=NULL, *running, * to_run;

	if( thread==NULL ){
#ifdef THREAD_DEBUG
		printf("thread,MyThreadJoin, thread is NULL\n");
		return -1;
#endif
	}

	t = (_thread*)thread;
	running = thread_engine->running;

	if( is_l_elem_exist( create_l_elem(t), running->kids)==0 ){
#ifdef THREAD_DEBUG
		printf("thread,MyThreadJoin, thread is not immediate kid of the calling thread\n");
#endif
		return -1;
	}

	t->joined_parent = 1;

	running->is_joined = 1; /* set parent's joined flag */
	running->joinedKid = t;

	en_list( create_l_elem(running), thread_engine->joined_block_list ); /* put the calling thread on joined list */

	to_run = (_thread*) get_queue_elem_content( de_queue(thread_engine->ready_q) );
	thread_engine->running = to_run;
	swapcontext( & running->ctx, & to_run->ctx);

	return 0;
}

void MyThreadJoinAll(void){
	_thread *running, * to_run;

	_check_thread_engine();

	running = thread_engine->running;

	if( running->kids==NULL || get_list_size(running->kids)==0 ){
#ifdef THREAD_DEBUG
		printf("thread,MyThreadJoinAll, thread has no active kid.\n");
		return -1;
#endif
		return;
	}

	running->is_all_joined=1;

	en_list( create_l_elem(running), thread_engine->joined_block_list ); /* put the calling thread on joined list */

	to_run = (_thread*) get_queue_elem_content( de_queue(thread_engine->ready_q) );
	thread_engine->running = to_run;
	swapcontext( & running->ctx, & to_run->ctx);

}

/* get the first from ready-queue, then switch context of the two threads, then terminate the calling thread */
void MyThreadExit(void){
	int readyq_size;
	_thread * to_exit, * to_run;

	_check_thread_engine();

	if( thread_engine->running==thread_engine->root ){
		while( _get_readyq_size()>0 ){  /* if other threads on ready q, root swap with them */
			_swap_run();
		}
		exit(0);
	}else{
		to_exit =thread_engine->running;
		del_l_elem( create_l_elem(to_exit) , to_exit->parent->kids); /* del the thread from its parent's kids-list*/

		if( to_exit->joined_parent ){ /* if caused parent joined, clear its parent's join-flag, and put the parent to ready-q */
			to_exit->parent->is_joined = 0; /* clear the parent's joined flag */
			to_exit->parent->joinedKid = NULL;

			del_l_elem( create_l_elem(to_exit->parent), thread_engine->joined_block_list );  /* remove the parent from the block list */
			en_queue( create_q_elem(to_exit->parent) , thread_engine->ready_q); /* joined kid to exit, so put the parent to the ready-q */
		}

		/* if the to_exit-thread's parent is blocked by join-all, AND this to_exit-thread is the only kid of its parent */
		if( to_exit->parent->is_all_joined==1 && get_list_size( to_exit->parent->kids )==0 ){
			to_exit->parent->is_all_joined=0; /* clear the flag */

			del_l_elem( create_l_elem(to_exit->parent), thread_engine->joined_block_list );  /* remove the parent from the block list */
			en_queue( create_q_elem(to_exit->parent) , thread_engine->ready_q); /* joined kid to exit, so put the parent to the ready-q */
		}

		free(to_exit); /* free this thread's context */

		to_run = (_thread*) get_queue_elem_content( de_queue(thread_engine->ready_q) );  /* get a thread from the ready-q */
		thread_engine->running = to_run;
		swapcontext( &ctx_buf, & to_run->ctx);
	}
}

MySemaphore MySemaphoreInit(int initialValue){
	_semaphore * s = NULL;

	_check_thread_engine();

	s = (_semaphore*)malloc( sizeof(_semaphore) );

	if(initialValue<0){
#ifdef THREAD_DEBUG
		printf("thread, MySemaphoreInit, initialValue must be non-negative\n");
#endif
		return NULL;
	}


	if( s==NULL){
#ifdef THREAD_DEBUG
		printf("thread, MySemaphoreInit, memory allocation error\n");
#endif
		return NULL;
	}
	s->value = initialValue;
	s->blocked_list = create_list();
	/* en_list( create_l_elem(s), thread_engine->sem_block_list ); */

	return (MySemaphore)s;
}

/* doesn't pre-empt */
void MySemaphoreSignal(MySemaphore sem){
	_semaphore * s=NULL;
	_thread * to_run;

	_check_thread_engine();

	if( sem==NULL){
#ifdef THREAD_DEBUG
		printf("thread, MySemaphoreSignal, sem is NULL\n");
#endif
		return;
	}

	s = (_semaphore*)sem;
	s->value++;

	/* if some threads previously blocked on the semaphore */
	if( s->value<=0 ){
		to_run = (_thread*)get_list_elem_content( de_list(s->blocked_list) ); /* get one from the sem-block-list */
		en_queue( create_q_elem(to_run), thread_engine->ready_q ); /* put the thread on the ready-q, but doesn't pre-empt current */
	}

}

void MySemaphoreWait(MySemaphore sem){
	_semaphore * s=NULL;
	_thread * running, * to_run;

	_check_thread_engine();

	if( sem==NULL){
#ifdef THREAD_DEBUG
		printf("thread, MySemaphoreWait, sem is NULL\n");
#endif
		return;
	}

	s = (_semaphore*)sem;
	running = thread_engine->running;

	s->value--;
	if( s->value<0 ){
		en_list( create_l_elem(running), s->blocked_list); /* put this thread on sem-block-list */
		to_run = (_thread*) get_queue_elem_content( de_queue(thread_engine->ready_q) );  /* get a thread from the ready-q */

		thread_engine->running = to_run;
		swapcontext( & running->ctx, & to_run->ctx);
	}

}

int MySemaphoreDestroy(MySemaphore sem){
	_semaphore * s=NULL;

	_check_thread_engine();

	if( sem==NULL){
#ifdef THREAD_DEBUG
		printf("thread, MySemaphoreDestroy, sem is NULL\n");
#endif
		return -1;
	}

	s = (_semaphore*)sem;
	if( get_list_size( s->blocked_list )!=0 ){
#ifdef THREAD_DEBUG
		printf("thread, MySemaphoreDestroy, the sem-block-list is not empty, cannot destroy it.\n");
#endif
		return -1;
	}

	free(s->blocked_list);
	free(s);

	return 0;
}

/**
 * it will call exit(1) if start_funct is NULL
 */
void MyThreadInit(void(*start_funct)(void *), void *args){
	_thread_core * engine_t = NULL;
	_thread * root_t = NULL;
	_thread * t_exit_handler_t = NULL;

	if(thread_engine!=NULL){
		printf("MyThreadInit can only be invoked once\n");
		exit(1);
	}

	if(start_funct==NULL){
		printf("start_funct is NULL\n");
		exit(1);
	}

	engine_t = (_thread_core*) malloc( sizeof(_thread_core) );
	root_t = (_thread*) malloc( sizeof(_thread) );

	if( engine_t==NULL || root_t==NULL ){
#ifdef THREAD_DEBUG
		printf("thread, init, memory allocation error\n");
#endif
	}

	engine_t->ready_q = create_queue();
	engine_t->root = NULL;
	engine_t->running = NULL;
	engine_t->joined_block_list = create_list();

	/* init the root thread*/
    getcontext(& root_t->ctx);
    root_t->ctx.uc_stack.ss_sp = malloc( sizeof(char) * THREAD_STACK_SIZE );
    root_t->ctx.uc_stack.ss_size = sizeof(char) * THREAD_STACK_SIZE;
    root_t->parent=NULL; /* root has no parent */
    root_t->joinedKid = NULL; /* root doesn't need these two fields, it just monitor the ready-q */
    root_t->kids=NULL;
    root_t->joined_parent = 0;
    root_t->is_all_joined = 0;
    root_t->is_joined = 0;
    memset( root_t->ctx.uc_stack.ss_sp, 0, root_t->ctx.uc_stack.ss_size );
    makecontext(& root_t->ctx, start_funct, 1, args);

    /* set engine's fields */
	engine_t->root = engine_t->running = root_t;

	thread_engine = engine_t;

    setcontext( &root_t->ctx ); /* execute root */

}
