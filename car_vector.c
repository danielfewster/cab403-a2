#include <stdio.h>
#include <stdlib.h>

#include "car_vector.h"

// Helper Functions

size_t min(size_t a, size_t b) {
    return a > b ? b : a;
}

size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

int is_init(car_vector_t* vec) {
    return sizeof(vec->data) >= sizeof(DV_INITIAL_CAPACITY*sizeof(car_t));
}

// Main Functions

void cv_init( car_vector_t* vec ) {
    vec->capacity = DV_INITIAL_CAPACITY;
    vec->size = 0;
    vec->data = (car_t *)malloc(DV_INITIAL_CAPACITY * sizeof(car_t));
}

void cv_ensure_capacity( car_vector_t* vec, size_t new_size ) {
    if (is_init(vec)) {
        if (new_size > vec->capacity) {
            vec->capacity = max(vec->capacity * DV_GROWTH_FACTOR, new_size);
            vec->data = realloc(vec->data, vec->capacity * sizeof(car_t));
        }
    }
}

void cv_destroy( car_vector_t* vec ) {
    if (is_init(vec)) {
        vec->capacity = 0;
        vec->size = 0;
        free(vec->data);
        vec->data = NULL;
    }
}

void cv_copy( car_vector_t* vec, car_vector_t* dest ) {
    if (is_init(vec) && is_init(dest) && vec != dest) {
        dest->size = vec->size;
        cv_ensure_capacity(dest, vec->size);
        for (size_t i = 0; i < vec->size; i++) {
            dest->data[i] = vec->data[i];
        }
    }
}

void cv_clear( car_vector_t* vec ) {
    if (is_init(vec)) {
        vec->size = 0;
    }
}

void cv_push( car_vector_t* vec, car_t new_item ) {
    if (is_init(vec)) {
        vec->size++;
        cv_ensure_capacity(vec, vec->size);
        vec->data[vec->size - 1] = new_item;
    }
}

void cv_pop( car_vector_t* vec ) {
    if (is_init(vec)) {
        if (vec->size > 0) {
            vec->size--;
        } else {
            vec->size = 0;
        }
    }
}

car_t *cv_last( car_vector_t* vec ) {
    car_t *result = NULL;
    if (vec->size > 0) {
        result = &vec->data[vec->size - 1];
    }
    return result;
}

void cv_insert_at( car_vector_t* vec, size_t pos, car_t new_item ) {
    if (is_init(vec)) {
        size_t loc = min(pos, vec->size);
        car_t old_data[vec->size];
        for (size_t i = 0; i < vec->size; i++) {
            old_data[i] = vec->data[i];
        }
        vec->size++;
        cv_ensure_capacity(vec, vec->size);
        for (size_t i = loc + 1; i < vec->size; i++) {
            vec->data[i] = old_data[i - 1];
        }
        vec->data[loc] = new_item;
    }
}

void cv_remove_at( car_vector_t* vec, size_t pos ) {
    if (is_init(vec)) {
        size_t loc = min(pos, vec->size);
        car_t old_data[vec->size];
        for (size_t i = 0; i < vec->size; i++) {
            old_data[i] = vec->data[i];
        }
        for (size_t i = loc; i < vec->size; i++) {
            vec->data[i] = old_data[i + 1];
        }
        if (pos < vec->size) vec->size--;
    }
}

void cv_foreach( car_vector_t* vec, void (*callback)(car_t, void*), void* info ) {
    if (is_init(vec)) {
        for (size_t i = 0; i < vec->size; i++) {
            callback(vec->data[i], info);
        }
    }
}

