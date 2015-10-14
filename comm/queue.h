#ifndef _QUEUE_H_H_
#define _QUEUE_H_H_

#include <stdlib.h>

#include "list.h"

typedef List Queue;

//公共接口
#define queue_init list_init

#define queue_destroy list_destroy

int queue_enqueue(Queue *queue, const void *data);

int queue_dequeue(Queue *queue, void **data);

#define queue_peek(queue) ((queue)->head == NULL ? NULL : (queue)->head->data)

#define queue_size list_size

#endif
