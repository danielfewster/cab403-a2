#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "p_queue.h"

struct Node* newNode(void *data){
    struct Node *node = (struct Node*)malloc(sizeof(struct Node));

    node->data = data;
    node->next = NULL;

    return node;
}

struct Queue* newQueue() {
    struct Queue *queue = (struct Queue*)malloc(sizeof(struct Queue));

    queue->head = NULL;
    queue->tail = NULL;

    queue->size = 0;

    return queue;
}

void enqueue(struct Queue *queue, void *data){
    struct Node *node = newNode(data);

    queue->size++;

    if (queue->head == NULL) {
        queue->head = node;
    } else {
        queue->tail->next = node;
    }
    queue->tail = node;
}

bool isEmpty(struct Queue *queue) {
    if (queue->head == NULL) {
        return true;
    }
    return false;
}

void *dequeue(struct Queue *queue) {
    if (isEmpty(queue)) {
        printf("Can't dequeue this queue\n");
        exit(EXIT_FAILURE);
    }

    struct Node *node = queue->head;

    queue->head = node->next;

    if (isEmpty(queue)) {
        queue->tail = NULL;
    }

    if (node != NULL) {
        queue->size--;
        void *data = node->data;
        free(node);
        return data;
    }

    exit(EXIT_FAILURE);
}

/*
#include "simulator.h"

int main(void) {
    struct Queue *q = newQueue();

    int cap = 10;

    for (int i = 0; i < cap; i++) {
        car_t *c = (car_t *)malloc(sizeof(car_t));
        c->level_parked = i;
        enqueue(q, (void *)c);
    }

    for (int i = 0; i < cap; i++) {
        car_t *c = (car_t *)dequeue(q);
        printf("Car parked at: %d\n", c->level_parked);
        free(c);
    }

    return 0;
}
*/