/*****************************************************************//**
 * \file   dbl_vector.h
 * \brief  API definition for the dbl_vector abstract data type.
 * 
 * \author Lawrence
 * \date   August 2021
 *********************************************************************/
#pragma once

#include <stdbool.h>
#include <math.h>

#include "simulator.h"

typedef struct car_vector {
	size_t size;
	size_t capacity;
	car_t *data;
} car_vector_t;

#define DV_INITIAL_CAPACITY 4
#define DV_GROWTH_FACTOR 1.25

void cv_init( car_vector_t* vec );

void cv_ensure_capacity( car_vector_t* vec, size_t new_size );

void cv_destroy( car_vector_t* vec );

void cv_copy( car_vector_t* vec, car_vector_t* dest );

void cv_clear( car_vector_t* vec );

void cv_push( car_vector_t* vec, car_t new_item );

void cv_pop( car_vector_t* vec );

car_t *cv_last( car_vector_t* vec );

void cv_insert_at( car_vector_t* vec, size_t pos, car_t new_item );

void cv_remove_at( car_vector_t* vec, size_t pos );

void cv_foreach( car_vector_t* vec, void (*callback)(car_t, void*), void* info );
