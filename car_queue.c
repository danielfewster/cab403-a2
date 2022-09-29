// C program for array implementation of queue
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simulator.h"
#include "car_queue.h"
 
// function to create a queue
// of given capacity.
// It initializes size of queue as 0
queue_t *create_queue(unsigned capacity)
{
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
 
    // This is important, see the enqueue
    queue->rear = capacity - 1;
    queue->cars = (car_t *)malloc(queue->capacity*sizeof(car_t));
    return queue;
}
 
// Queue is full when size becomes
// equal to the capacity
int isFull(queue_t *queue)
{
    return (queue->size == queue->capacity);
}
 
// Queue is empty when size is 0
int isEmpty(queue_t *queue)
{
    return (queue->size == 0);
}
 
// Function to add an item to the queue.
// It changes rear and size
void enqueue(queue_t *queue, car_t *item)
{
    if (isFull(queue)) return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->cars[queue->rear] = *item;
    queue->size = queue->size + 1;
}
 
// Function to remove an item from queue.
// It changes front and size
car_t *dequeue(queue_t *queue)
{
    if (isEmpty(queue)) return NULL;
    car_t *item = &queue->cars[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}
 
// Function to get front of queue
car_t *front(queue_t *queue)
{
    if (isEmpty(queue)) return NULL;
    return &queue->cars[queue->front];
}
 
// Function to get rear of queue
car_t *rear(queue_t *queue)
{
    if (isEmpty(queue)) return NULL;
    return &queue->cars[queue->rear];
}
 
/*
// Driver program to test above functions./
int main()
{
    queue_t *queue = create_queue(1000);
 
    char plate[6] = {'1', '2', '3', 'A', 'B', 'C'};

    car_t c1;
    strcpy(c1.license_plate, plate);
    c1.test = 1;

    car_t c2;
    strcpy(c2.license_plate, plate);
    c2.test = 2;

    car_t c3;
    strcpy(c3.license_plate, plate);
    c3.test = 3;

    car_t c4;
    strcpy(c4.license_plate, plate);
    c4.test = 4;

    printf("car 1 plate: %s\n", c1.license_plate);
    printf("car 2 plate: %s\n", c2.license_plate);
    printf("car 3 plate: %s\n", c3.license_plate);
    printf("car 4 plate: %s\n", c4.license_plate);

    enqueue(queue, &c1);
    enqueue(queue, &c2);
    enqueue(queue, &c3);
    enqueue(queue, &c4);
 
    printf("%d dequeued from queue\n", dequeue(queue)->test);
    printf("Front item is %d\n", front(queue)->test);
    printf("Rear item is %d\n", rear(queue)->test);
 
    return 0;
}
*/
