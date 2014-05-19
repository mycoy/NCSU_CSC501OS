/**
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * List for threads, header file
 */

#ifndef LIST_H_

#ifndef LIST_DEBUG
#endif


typedef void * list;
typedef void * list_elem;

list create_list();

void decompose_list(list q);

list_elem create_l_elem(void * data);

void decompose_l_elem(list_elem le);



/**
 * add an element to the list (add to the end of the list);
 * return: 0 success, 1 error;
 */
int en_list(list_elem e, list l);


/**
 * get and delete, an element from the list (get at the front of the list);
 * return: NULL if no element or other errors;
 */
list_elem de_list(list l);

/* get value from the list_elem wrapper */
void * get_list_elem_content(list_elem le);

/**
 * query the number of elements in the list;
 */
int get_list_size(list l);

/*
 * return 0: exist;
 * 1: not exist
 *
 * check the data file of the list_elem
 */
int is_l_elem_exist(list_elem le_t, list l_t);

/**
 * check the data file of the list_elem
 */
list_elem del_l_elem(list_elem le_t, list l_t);


#endif

