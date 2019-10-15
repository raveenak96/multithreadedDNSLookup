#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>

typedef struct q_node_struct {
	void* element;
} q_node;

typedef struct queue_struct {
	q_node* array;
	int front;
	int back;
	int size;
} queue;


void init_queue(queue* q, int size);
int empty(queue* q);
int full(queue* q);
int enqueue(queue* q,void* element);
void* dequeue(queue* q);
void clear_queue(queue* q);

#endif