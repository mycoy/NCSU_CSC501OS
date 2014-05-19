/**
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * Queue for threads, header file
 */

#ifndef QUEUE_H_

#ifndef QUEUE_DEBUG
#endif


typedef void * queue;
typedef void * queue_elem;

queue create_queue();

void decompose_queue(queue q);

queue_elem create_q_elem(void * data);

void decompose_q_elem(queue_elem qe);



/**
 * add an element to the quque (add to the end of the queue);
 * return: 0 success, 1 error;
 */
int en_queue(queue_elem e, queue q);


/**
 * get and delete, an element from the queue (get at the front of the queue);
 * return: NULL if no element or other errors;
 */
queue_elem de_queue(queue q);

/* get value from the queue_elem wrapper */
void * get_queue_elem_content(queue_elem qe);

/**
 * query the number of elements in the queue;
 */
int get_queue_size(queue q);


#endif

