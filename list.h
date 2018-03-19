#ifndef _LIST_H_
#define _LIST_H_

// a node definition
typedef struct node_t {
    struct node_t *prev, *next;
    void *data;
} node_t;


// common function to free objects generic data
typedef void (*free_function_t) (void *data);

// common function to be called on each node in for_each
//typedef int (*iterator_function_t) (node_t *node, va_list *app);
typedef int (*iterator_function_t) (void *data);

// use it in find node
typedef int (*compare_function_t) (void *data, void *what);

// a list of nodes definition
typedef struct {
    node_t *head;
    node_t *tail;
    unsigned int num_items;
    free_function_t free_function;
} list_t;


// "opens" - aka initialize the list
list_t *list_open(free_function_t free_function);

// "close" - aka free the list and call free data function on each node
void list_close(list_t *list);

// add a node to the list - generic data, which can be anything
node_t *list_add(list_t *list, void *data);

// deletes the node from the list
void list_del(list_t *list, node_t *node);

// iterates the list and call iterator function on each node
node_t *list_for_each(list_t *list, iterator_function_t iterator);
// old func with variable params
//node_t *list_for_each(list_t *list, iterator_function_t iterator, ...)

// find a node in the list by calling compare function on each node
node_t *list_find(list_t *list, compare_function_t compare, void * what);

#define forEach(list, node, obj) \
    for(node = list->head; node && (obj = node->data); node = node->next)

#endif
