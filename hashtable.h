#ifndef HASHTABLE
#define HASHTABLE

#include <stdio.h> 

// An item inserted into a hash table.
// As hash collisions can occur, multiple items can exist in one bucket.
// Therefore, each bucket is a linked list of items that hashes to that bucket.
typedef enum { 
    ENTRANCE,
    LEVEL,
    EXIT,
    NONE
} location;

typedef struct car_info {
    unsigned long int last_time_entered;
    unsigned long int last_bill;
    location loc_name;
    int loc_index;
    bool parked;
} car_info_t;

typedef struct item item_t;
struct item
{
    char *key;
    car_info_t info;
    item_t *next;
};

// A hash table mapping a string to an integer.
typedef struct htab htab_t;
struct htab
{
    item_t **buckets;
    size_t size;
};

#endif
