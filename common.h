#ifndef COMMON
#define COMMON

#include <pthread.h>

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
    lpr_sensor_t *lpr_sensor;
    boom_gate_t *boom_gate;
    info_sign_t *info_sign;
} entrance_t;

typedef struct exit {
    lpr_sensor_t *lpr_sensor;
    boom_gate_t *boom_gate;
} exit_t;

typedef struct level {
    lpr_sensor_t *lpr_sensor;
    signed short int temp_sensor;
    unsigned char alarm;
    char padding[5];
} level_t;

typedef struct cp_data {
    entrance_t *entrances;
    exit_t *exits;
    level_t *levels;
} cp_data_t;

typedef struct shm {
    int fd;
    cp_data_t *data;
} shm_t;

#endif