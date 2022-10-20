#ifndef COMMON
#define COMMON

#include <pthread.h>

#define PLATE_CHARS 6
#define RAND_SEED 0

// CAR PARK CONSTANTS
#define ENTRANCES 5 
#define EXITS 5
#define LEVELS 5 
#define LEVEL_CAPACITY 20

// SHM CONSTANTS
#define SHM_SIZE 2920
#define SHM_KEY "PARKING"

typedef struct info_sign {
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    char display;
    char padding[7];
} info_sign_t;

typedef struct boom_gate {
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    char status;
    char padding[7];
} boom_gate_t;

typedef struct lpr_sensor {
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    char license_plate[6];
    char padding[2];
} lpr_sensor_t;

typedef struct entrance {
    lpr_sensor_t lpr_sensor;
    boom_gate_t boom_gate;
    info_sign_t info_sign;
} entrance_t;

typedef struct exit {
    lpr_sensor_t lpr_sensor;
    boom_gate_t boom_gate;
} exit_t;

typedef struct level {
    lpr_sensor_t lpr_sensor;
    volatile signed short int temp_sensor;
    volatile unsigned char alarm;
    char padding[5];
} level_t;

typedef struct shared_data {
    entrance_t entrances[ENTRANCES];
    exit_t exits[EXITS];
    level_t levels[LEVELS];
} shared_data_t;

typedef struct shared_memory {
    int fd;
    shared_data_t *data;
} shared_memory_t;

#endif