#include <stdio.h>
#include <stdlib.h>
#include "dbl_vector.h"

// Helper Functions

size_t min(size_t a, size_t b) {
    return a > b ? b : a;
}

size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

int is_init(dbl_vector_t* vec) {
    return vec->size >= 0;
}

// Main Functions

void dv_init( dbl_vector_t* vec ) {
    vec->capacity = DV_INITIAL_CAPACITY;
    vec->size = 0;
    vec->data = (double *)malloc(DV_INITIAL_CAPACITY * sizeof(double));
}

void dv_ensure_capacity( dbl_vector_t* vec, size_t new_size ) {
    if (is_init(vec)) {
        if (new_size > vec->capacity) {
            vec->capacity = max(vec->capacity * DV_GROWTH_FACTOR, new_size);
            vec->data = realloc(vec->data, vec->capacity * sizeof(double));
        }
    }
}

void dv_destroy( dbl_vector_t* vec ) {
    if (is_init(vec)) {
        vec->capacity = 0;
        vec->size = 0;
        free(vec->data);
        vec->data = NULL;
    }
}

void dv_copy( dbl_vector_t* vec, dbl_vector_t* dest ) {
    if (is_init(vec) && is_init(dest) && vec != dest) {
        dest->size = vec->size;
        dv_ensure_capacity(dest, vec->size);
        for (size_t i = 0; i < vec->size; i++) {
            dest->data[i] = vec->data[i];
        }
    }
}

void dv_clear( dbl_vector_t* vec ) {
    if (is_init(vec)) {
        vec->size = 0;
    }
}

void dv_push( dbl_vector_t* vec, double new_item ) {
    if (is_init(vec)) {
        vec->size++;
        dv_ensure_capacity(vec, vec->size);
        vec->data[vec->size - 1] = new_item;
    }
}

void dv_pop( dbl_vector_t* vec ) {
    if (is_init(vec)) {
        if (vec->size > 0) {
            vec->size--;
        } else {
            vec->size = 0;
        }
    }
}

double dv_last( dbl_vector_t* vec ) {
    double result = NAN;
    if (vec->size > 0) {
        result = vec->data[vec->size - 1];
    }
    return result;
}

void dv_insert_at( dbl_vector_t* vec, size_t pos, double new_item ) {
    if (is_init(vec)) {
        size_t loc = min(pos, vec->size);
        double old_data[vec->size];
        for (size_t i = 0; i < vec->size; i++) {
            old_data[i] = vec->data[i];
        }
        vec->size++;
        dv_ensure_capacity(vec, vec->size);
        for (size_t i = loc + 1; i < vec->size; i++) {
            vec->data[i] = old_data[i - 1];
        }
        vec->data[loc] = new_item;
    }
}

void dv_remove_at( dbl_vector_t* vec, size_t pos ) {
    if (is_init(vec)) {
        size_t loc = min(pos, vec->size);
        double old_data[vec->size];
        for (size_t i = 0; i < vec->size; i++) {
            old_data[i] = vec->data[i];
        }
        for (size_t i = loc; i < vec->size; i++) {
            vec->data[i] = old_data[i + 1];
        }
        if (pos < vec->size) vec->size--;
    }
}

void dv_foreach( dbl_vector_t* vec, void (*callback)(double, void*), void* info ) {
    if (is_init(vec)) {
        for (size_t i = 0; i < vec->size; i++) {
            callback(vec->data[i], info);
        }
    }
}

