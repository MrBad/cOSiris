#include <sys/types.h>
#include <stdlib.h>
#include "list.h"
#include "console.h"

list_t *list_open(free_function_t free_function)
{
    list_t *list;
    list = (list_t *) malloc(sizeof(list_t));
    if (!list) {
        panic("malloc");
        return NULL;
    }
    list->head = list->tail = NULL;
    list->num_items = 0;
    list->free_function = free_function;

    return list;
}

static node_t *new_node(void *data)
{
    node_t *node;
    node = (node_t *) malloc(sizeof(node_t));
    node->next = node->prev = NULL;
    node->data = data;

    return node;
}

static void list_add_node(list_t *list, node_t *node)
{
    if (!list->head) {
        list->head = list->tail = node;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
    list->num_items++;
}


node_t *list_add(list_t *list, void *data)
{
    node_t *node;
    node = new_node(data);
    list_add_node(list, node);
    return node;
}

node_t *list_insert_before(list_t *list, node_t *node, void *data)
{
    node_t *n;

    if (!(n = new_node(data)))
        return NULL;
    if (!node) {
        list_add_node(list, n);
        return n;
    }
    if (node->prev)
        node->prev->next = n;
    else
        list->head = n;
    n->prev = node->prev;
    n->next = node;
    node->prev = n;
    list->num_items++;

    return n;
}

void list_del(list_t *list, node_t *node)
{
    node_t *prev, *next;
    prev = node->prev;
    next = node->next;
    if (prev) {
        prev->next = next;
    } else {
        list->head = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        list->tail = prev;
    }
    if (list->free_function) {
        list->free_function(node->data);
    }
    free(node);
    list->num_items--;
}

void list_close(list_t *list)
{
    node_t *node;

    while (list->head) {
        node = list->head;
        if (list->free_function) {
            list->free_function(node->data);
        }
        free(node);
        list->head = node->next;
    }
    free(list);
}
#define foreach(list, item) \
    for(item = list->head; item; item = item->next)

node_t *list_for_each(list_t *list, iterator_function_t iterator)
{
    node_t *last_node = NULL;
    node_t *node;
    int found;
    node = list->head;

    while (node) {
        found = iterator(node->data);
        if (found) {
            last_node = node;
            break;
        }
        node = node->next;
    }
    return last_node;
}

node_t *list_find(list_t *list, compare_function_t compare, void *what) {
    node_t *last_node = NULL;
    node_t *node;

    int found;
    node = list->head;
    while (node) {
        found = compare(node->data, what);
        if(found) {
            last_node = node;
        }
        node = node->next;
    }

    return last_node;
}
