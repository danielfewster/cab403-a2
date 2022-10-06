#ifndef MANAGER
#define MANAGER

#include "common.h"
#include "dict.h"

typedef struct entrance_lpr_args {
    entrance_t *entrance;
    Dict *plates_h;
    int *cars_per_level;
} entrance_lpr_args_t;

typedef struct level_lpr_args {
    int level_idx;
    level_t *level;
    Dict *plates_h;
    int *cars_per_level;
} level_lpr_args_t;

typedef struct exit_lpr_args {
    exit_t *exit;
    Dict *plates_billing;
    Dict *plate_time_entered;
    int *cars_per_level;
} exit_lpr_args_t;

typedef struct status_display_args {
    cp_data_t *cp_data;
    Dict *plate_h;
} status_display_args_t;

#endif
