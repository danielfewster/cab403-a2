#ifndef MANAGER
#define MANAGER

#include "common.h"
#include "hashtable.h"

typedef struct status_display_args {
    shared_data_t *cp_data;
    htab_t *plates_info;
    unsigned long int *total_revenue;
} status_display_args_t;

typedef struct entrance_lpr_args {
    entrance_t *entrance;
    htab_t *plates_info;
    int entrance_idx;
} entrance_lpr_args_t;

typedef struct level_lpr_args {
    level_t *level;
    htab_t *plates_info;
    int level_idx;
} level_lpr_args_t;

typedef struct exit_lpr_args {
    exit_t *exit;
    htab_t *plates_info;
    unsigned long int *total_revenue;
    int exit_idx;
} exit_lpr_args_t;

typedef struct open_entrance_args {
    entrance_t *entrance;
    htab_t *plates_info;
} open_entrance_args_t;

typedef struct exit_args {
    exit_t *exit;
    htab_t *plates_info;
} exit_args_t;

#endif
