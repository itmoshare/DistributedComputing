#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "ipc.h"

typedef struct queue_t {
	struct queue_t *head, *tail, *next;
	local_id pid;
	timestamp_t time;
} queue_t;

queue_t *g_queue = NULL;

void enq (local_id pid, timestamp_t time)
{
	if( g_queue == NULL ){
		g_queue = malloc(sizeof(queue_t));
		memset(g_queue, 0, sizeof(queue_t));
		
	}
	queue_t *node;
	
	node = malloc(sizeof(queue_t));
	memset(node, 0, sizeof(queue_t));

	node->pid = pid;
	node->time = time;
	node->next = NULL;

	if (g_queue->head == NULL && g_queue->tail == NULL)
	{
		g_queue->head = node;
		g_queue->tail = node;
		return;
	}

	queue_t *cur = g_queue->head;
	queue_t *prev = NULL;

	while (cur != NULL) 
	{
		if ((cur->time == time && pid < cur->pid) ||
			cur->time > time)
		{
			node->next = cur;
			if (prev != NULL) prev->next = node;
			if (cur == g_queue->head) g_queue->head = node;

			break;
		} 
		prev = cur;
		cur = cur->next;

		if (cur == NULL) 
		{
			g_queue->tail->next = node;
			g_queue->tail = node;
		}
	}
}
void deq ()
{
	if (g_queue == NULL) return;
	if (g_queue->head == NULL) return;
	queue_t *del = g_queue->head;
	g_queue->head = g_queue->head == g_queue->tail ? g_queue->tail = NULL : g_queue->head->next;
	free(del);
}

void clear ()
{
	if (g_queue == NULL) return;
	queue_t *node = g_queue->head;
	while (node != NULL) 
	{
		queue_t *next = node->next;
		free(node);
		node = next;
	}
	g_queue->head = g_queue->tail = NULL;
	free(g_queue);
}

#endif
