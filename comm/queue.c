#include <stdlib.h>

#include "list.h"
#include "queue.h"

//入队
int queue_enqueue(Queue *queue, const void *data) 
{
	//在链表为插入元素
	return list_ins_next(queue, list_tail(queue), data);
}


//出队
int queue_dequeue(Queue *queue, void **data) 
{
	return list_rem_next(queue, NULL, data);
}
