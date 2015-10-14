#include <stdlib.h>
#include <string.h>

#include "list.h"

//初始化一个链表
void list_init(List *list, void (*destroy)(void *data)) 
{
	list->size = 0;
	list->destroy = destroy;
	list->head = NULL;
	list->tail = NULL;

	return;
}

//销毁一个链表
void list_destroy(List *list) 
{

	void *data;
	while (list_size(list) > 0) 
	{
		if (list_rem_next(list, NULL, (void **)&data) == 0 && list->destroy != NULL)
			list->destroy(data);
	}

	memset(list, 0, sizeof(List));

	return;
}

//清空一个链表
void list_clean(List *list)
{
	void *data;
	while (list_size(list) > 0) 
	{
		if (list_rem_next(list, NULL, (void **)&data) == 0 && list->destroy != NULL)
			list->destroy(data);
	}
	
	return;
}

//在链表中插入一个元素
int list_ins_next(List *list, ListElmt *element, const void *data)
{
	ListElmt           *new_element;

	//新建一个节点
	if ((new_element = (ListElmt *)malloc(sizeof(ListElmt))) == NULL)
		return -1;

	//复制数据
	new_element->data = (void *)data;

	if (element == NULL) 
	{
		//如果elem为空，则插入链表首部
		if (list_size(list) == 0)
			list->tail = new_element;

		new_element->next = list->head;
		list->head = new_element;

	}
	else 
	{
		if (element->next == NULL)
			list->tail = new_element;

		new_element->next = element->next;
		element->next = new_element;
	}
	//更新链表节点数量
	list->size++;

	return 0;
}

//在链表末尾添加一个元素
int list_append(List *list, const void *data)
{
	return list_ins_next(list, list_tail(list), data);
}

//在链表中移除一个节点
int list_rem_next(List *list, ListElmt *element, void **data) 
{
	ListElmt *old_element;

	//如果链表为空
	if (list_size(list) == 0)
		return -1;

	if (element == NULL) 
	{
		*data = list->head->data;
		old_element = list->head;
		list->head = list->head->next;

		if (list_size(list) == 1)
			list->tail = NULL;
	}
	else 
	{
		if (element->next == NULL)
			return -1;

		*data = element->next->data;
		old_element = element->next;
		element->next = element->next->next;

		if (element->next == NULL)
			list->tail = element;
	}
	
	//释放内存空间
	free(old_element);

	list->size--;

	return 0;
}
