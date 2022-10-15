#ifndef SIMULATOR
#define SIMULATOR

typedef struct car {
    char license_plate[7];
    int level_parked;
    unsigned long int parking_exp;
} car_t;

#include "common.h"
#include "p_queue.h"
#include "car_vector.h"

typedef struct generate_car_args {
    queue_t *entrance_queues;
    car_vector_t *cv;
} generate_car_args_t;

typedef struct entering_car_args {
    entrance_t *entrance;
    queue_t *entrance_queue;
    level_t *levels;
    car_vector_t *parked_cars;
} entering_car_args_t;

typedef struct leaving_car_args {
    int level_idx;
    level_t *levels;
    exit_t *exits;
    car_vector_t *parked_cars;
} leaving_car_args_t;

#endif