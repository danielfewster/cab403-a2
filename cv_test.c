#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simulator.h"
#include "common.h"
#include "car_vector.c"

int main(void) {
    car_t carA;
    carA.parking_time = 10;

    car_t carB;
    carB.parking_time = 20;

    car_t carC;
    carC.parking_time = 30;

    car_vector_t car_vec;
    cv_init(&car_vec);

    cv_push(&car_vec, carA);
    cv_push(&car_vec, carB);
    cv_push(&car_vec, carC);

    printf("car vec size: %ld\n", car_vec.size);

    for (size_t i = 0; i < car_vec.size; i++) {
        printf("car parking time %d\n", car_vec.data[i].parking_time);
    }

    cv_pop(&car_vec);

    printf("car vec size: %ld\n", car_vec.size);

    for (size_t i = 0; i < car_vec.size; i++) {
        printf("car parking time %d\n", car_vec.data[i].parking_time);
    }

    cv_destroy(&car_vec);
}