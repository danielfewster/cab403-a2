#ifndef MANAGER
#define MANAGER

#include "common.h"
#include "hashtable.h"

typedef struct last_lpr_plates {
    char last_entrance_plates[ENTRANCES][PLATE_CHARS+1];
    char last_level_plates[LEVELS][PLATE_CHARS+1];
    char last_exit_plates[EXITS][PLATE_CHARS+1];
} last_lpr_plates_t;

typedef struct status_display_args {
    shared_data_t *cp_data;
    int *cars_per_level;
    htab_t *plates_billing;
    last_lpr_plates_t *last_lpr_plates;
} status_display_args_t;

typedef struct entrance_lpr_args {
    entrance_t *entrance;
    htab_t *plates_h;
    int *cars_per_level;
    char *last_plate;
} entrance_lpr_args_t;

typedef struct level_lpr_args {
    level_t *level;
    htab_t *plates_h;
    int *cars_per_level;
    int level_idx;
    char *last_plate;
} level_lpr_args_t;

typedef struct exit_lpr_args {
    exit_t *exit;
    htab_t *plates_billing;
    htab_t *plates_time_entered;
    char *last_plate;
} exit_lpr_args_t;

#endif
