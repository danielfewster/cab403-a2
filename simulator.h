#ifndef SIMULATOR
#define SIMULATOR

typedef struct car {
    char license_plate[6];
    int level_parked;
    unsigned long int parking_exp;
} car_t;

#include "common.h"
#include "car_queue.h"
#include "car_vector.h"

typedef struct thread_args {
    queue_t *entrance_queues;
    cp_data_t *cp_data;
    car_vector_t *parked_cars;
} thread_args_t;

#endif