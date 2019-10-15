#include <stdlib.h>
#include "queue.h"


void init_queue(queue* q, int size) {

	q->size=size;
	q->array = malloc(sizeof(q_node)*(size));

	for(int i=0;i<size;i++) {
		q->array[i].element = NULL;
	}

	q->front = 0;
	q->back = 0;

	return;
}

int empty(queue* q) {
    if((q->array[q->front].element==NULL) && (q->front==q->back)) {
        return 1;
    } else {
        return 0;
    }
}

int full(queue* q) {

    if((q->array[q->front].element!=NULL) && (q->front==q->back)) {
        return 1;
    } else {
        return 0;
    }

}

int enqueue(queue* q,void* element) {

	if(full(q)) {
		return -1;
	}
	q->array[q->back].element = element;
	q->back = ((q->back+1) % q->size);

	return 0;
}

void* dequeue(queue* q) {
	void* deq_element;

	if(empty(q)) {
		return NULL;
	}

	deq_element = q->array[q->front].element;
	q->array[q->front].element = NULL;
	q->front = ((q->front+1) % q->size);
	return deq_element;

}

void clear_queue(queue* q) {

	while(!empty(q)) {

		dequeue(q);
	}

	free(q->array);
}

