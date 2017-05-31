#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "ipc.h"

typedef struct queue_t {
	struct queue_t *head, *tail, *next;
	local_id pid;
	timestamp_t time;
} queue_t;

void q_init (queue_t **queue)
{
	*queue = malloc(sizeof(queue_t));
	memset(*queue, 0, sizeof(queue_t));
}

void enq (queue_t *queue, local_id pid, timestamp_t time)
{
	queue_t *node;
	
	node = malloc(sizeof(queue_t));
	memset(node, 0, sizeof(queue_t));

	node->pid = pid;
	node->time = time;
	node->next = NULL;

	if (queue->head == NULL && queue->tail == NULL)
	{
		queue->head = node;
		queue->tail = node;
		return;
	}

	queue_t *cur = queue->head;
	queue_t *prev = NULL;

	while (cur != NULL) 
	{
		if ((cur->time == time && pid < cur->pid) ||
			cur->time > time)
		{
			node->next = cur;
			if (prev) prev->next = node;

			if (cur == queue->head) queue->head = node;

			node = NULL;
			break;
		} 
		prev = cur;
		cur = cur->next;
	}

	if (node != NULL) 
	{
		queue->tail->next = node;
		queue->tail = node;
		node = NULL;
	}
}
void deq (queue_t *queue)
{
	if (queue->head == NULL) return;
	queue_t *del = queue->head;
	queue->head = queue->head == queue->tail ? queue->tail = NULL : queue->head->next;
	free(del);
}

void clear (queue_t *queue)
{
	queue_t *node = queue->head;
	while (node != NULL) 
	{
		queue_t *next = node->next;
		free(node);
		node = next;
	}
	queue->head = queue->tail = NULL;
	free(queue);
}

#endif
