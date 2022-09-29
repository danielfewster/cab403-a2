#ifndef CAR_QUEUE
#define CAR_QUEUE

#include "simulator.h"

// A structure to represent a queue
typedef struct queue {
    int front;
    int rear; 
    int size;
    int capacity;
    car_t *cars;
} queue_t;

#endif