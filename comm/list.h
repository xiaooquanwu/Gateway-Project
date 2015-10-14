#ifndef _LIST_H_H_
#define _LIST_H_H_

#include <stdlib.h>

//list elmt数据结构
typedef struct ListElmt_ 
{
	//数据域指针
	void               *data;
	struct ListElmt_   *next;
} ListElmt;

//链表数据结构
typedef struct List_ 
{
	int                size;
	void               (*destroy)(void *data);

	ListElmt           *head;
	ListElmt           *tail;
} List;

//操作链表的共用接口
void list_init(List *list, void (*destroy)(void *data));

void list_clean(List *list);

void list_destroy(List *list);

int list_ins_next(List *list, ListElmt *element, const void *data);

int list_rem_next(List *list, ListElmt *element, void **data);

int list_append(List *list, const void *data);

#define list_size(list) ((list)->size)

#define list_head(list) ((list)->head)

#define list_tail(list) ((list)->tail)

#define list_is_head(list, element) ((element) == (list)->head ? 1 : 0)

#define list_is_tail(element) ((element)->next == NULL ? 1 : 0)

#define list_data(element) ((element)->data)

#define list_next(element) ((element)->next)

#endif
