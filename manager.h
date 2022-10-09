#ifndef MANAGER
#define MANAGER

#include "common.h"
#include "hashtable.h"

typedef struct entrance_lpr_args {
    entrance_t *entrance;
    htab_t *plates_h;
    int *cars_per_level;
} entrance_lpr_args_t;

typedef struct level_lpr_args {
    level_t *level;
    htab_t *plates_h;
    int *cars_per_level;
    int level_idx;
} level_lpr_args_t;

typedef struct exit_lpr_args {
    exit_t *exit;
    htab_t *plates_billing;
    htab_t *plates_time_entered;
} exit_lpr_args_t;

typedef struct status_display_args {
    shared_data_t *cp_data;
    int *cars_per_level;
    htab_t *plates_billing;
} status_display_args_t;

#endif
