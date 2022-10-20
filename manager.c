#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "hashtable.c"
#include "common.h"
#include "manager.h"

pthread_mutex_t rand_lock;

bool get_shm_object(shared_memory_t *shm) {
    if ((shm->fd = shm_open(SHM_KEY, O_RDWR, 0)) < 0) {
        shm->data = NULL;
        return false;
    }

    shm->data = mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) return false;

    return true;
}

bool load_plates(htab_t *d) {
    FILE *file = fopen("plates.txt", "r");
    if (file == NULL) return false;

    int plates_amount = 100;
    int plate_chars = 6;
    
    int idx = 0;
    char plates_buff[plate_chars*plates_amount];
    for (char c = '!'; c != EOF; c = fgetc(file)) {
        if (isalnum(c)) {
            plates_buff[idx++] = c;
        }
    }

    idx = 0;
    for (int i = 0; i < plates_amount; i++) {
        char *plate = (char *)malloc((plate_chars+1)*sizeof(char));
        for (int j = 0; j < plate_chars; j++) {
            plate[j] = plates_buff[idx++];
        }
        plate[plate_chars] = '\0';
        htab_add(d, plate);
    }

    fclose(file);

    return true;
}

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int get_cars_in_level(htab_t *h, int level_idx) {
    int num_cars = 0;
    for (size_t i = 0; i < h->size; i++) {
        if (h->buckets[i] != NULL) {
            for (item_t *j = h->buckets[i]; j != NULL; j = j->next) {
                if (j->info.loc_name == LEVEL && j->info.loc_index == level_idx 
                        && j->info.parked) {
                    num_cars++;
                }
            }
        }
    } 
    return num_cars;
}

void *entrance_lpr_sensor(void *data) {
    entrance_lpr_args_t *args;
    args = (entrance_lpr_args_t *)data;

    entrance_t *e = args->entrance;

    char last_plate[6];

    while (1) {
        pthread_mutex_lock(&e->lpr_sensor.mutex); 
        while (strcmp(e->lpr_sensor.license_plate, last_plate) == 0)
            pthread_cond_wait(&e->lpr_sensor.cond_var, &e->lpr_sensor.mutex);

        item_t *item = htab_find(args->plates_info, e->lpr_sensor.license_plate);
        if (item == NULL) {
            pthread_mutex_lock(&e->info_sign.mutex);
            e->info_sign.display = 'X';
            pthread_cond_broadcast(&e->info_sign.cond_var);
            pthread_mutex_unlock(&e->info_sign.mutex);
        } else {
            bool found_level = false;
            for (int i = 0; i < LEVELS; i++) {
                if (get_cars_in_level(args->plates_info, i) < LEVEL_CAPACITY) {
                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    item->info.last_time_entered = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

                    item->info.loc_index = args->entrance_idx;
                    item->info.loc_name = ENTRANCE;

                    pthread_mutex_lock(&e->info_sign.mutex);
                    e->info_sign.display = i + 49;
                    pthread_cond_broadcast(&e->info_sign.cond_var);
                    pthread_mutex_unlock(&e->info_sign.mutex);

                    found_level = true;
                    break;
                }
            }

            if (found_level) {
                pthread_mutex_lock(&e->boom_gate.mutex);
                while (!(e->boom_gate.status == 'C' || e->boom_gate.status == 'O'))
                    pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
                if (e->boom_gate.status == 'C') {
                    e->boom_gate.status = 'R';
                    pthread_cond_broadcast(&e->boom_gate.cond_var);
                }
                pthread_mutex_unlock(&e->boom_gate.mutex);
            } else {
                pthread_mutex_lock(&e->info_sign.mutex);
                e->info_sign.display = 'F';
                pthread_cond_broadcast(&e->info_sign.cond_var);
                pthread_mutex_unlock(&e->info_sign.mutex);
            }
        }

        strcpy(last_plate, e->lpr_sensor.license_plate);
        pthread_mutex_unlock(&e->lpr_sensor.mutex);
    }
}

void *level_lpr_sensor(void *data) {
    level_lpr_args_t *args;
    args = (level_lpr_args_t *)data;

    level_t *l = args->level;

    char last_plate[6];

    while (1) {
        pthread_mutex_lock(&l->lpr_sensor.mutex);
        while (strcmp(l->lpr_sensor.license_plate, last_plate) == 0)
            pthread_cond_wait(&l->lpr_sensor.cond_var, &l->lpr_sensor.mutex);

        item_t *item = htab_find(args->plates_info, l->lpr_sensor.license_plate);
        if (item != NULL) {
            if (item->info.loc_name == ENTRANCE) {
                item->info.loc_name = LEVEL;
                item->info.loc_index = args->level_idx;
                item->info.parked = true;
            } else if (item->info.loc_name == LEVEL) {
                item->info.parked = false;
            }
        }

        strcpy(last_plate, l->lpr_sensor.license_plate);
        pthread_mutex_unlock(&l->lpr_sensor.mutex);
    }
}

void *exit_lpr_sensor(void *data) {
    exit_lpr_args_t *args;
    args = (exit_lpr_args_t *)data;

    exit_t *e = args->exit;

    char last_plate[6];

    while (1) {
        pthread_mutex_lock(&e->lpr_sensor.mutex);
        while (strcmp(e->lpr_sensor.license_plate, last_plate) == 0)
            pthread_cond_wait(&e->lpr_sensor.cond_var, &e->lpr_sensor.mutex);

        pthread_mutex_lock(&e->boom_gate.mutex);
        while (!(e->boom_gate.status == 'C' || e->boom_gate.status == 'O'))
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        if (e->boom_gate.status == 'C') {
            e->boom_gate.status = 'R';
            pthread_cond_broadcast(&e->boom_gate.cond_var);
        }
        pthread_mutex_unlock(&e->boom_gate.mutex);

        item_t *item = htab_find(args->plates_info, e->lpr_sensor.license_plate);
        if (item != NULL) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            unsigned long int curr_time = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

            unsigned long int time_entered = item->info.last_time_entered;
            unsigned long int bill = (curr_time-time_entered)*5;
            *(args->total_revenue) += bill;
            item->info.last_bill = bill;
            FILE *file = fopen("billings.txt", "a+");
            if (file != NULL) {
                fprintf(file, "%s $%.2f\n", e->lpr_sensor.license_plate, (double)bill/100);
                fclose(file);
            }

            item->info.loc_name = EXIT;
            item->info.loc_index = args->exit_idx;
        }

        strcpy(last_plate, e->lpr_sensor.license_plate);
        pthread_mutex_unlock(&e->lpr_sensor.mutex);
    }
}

void *status_display(void *data) {
    status_display_args_t *args;
    args = (status_display_args_t *)data;

    while (1) {
        system("clear");
        for (int i = 0; i < ENTRANCES; i++) {
            entrance_t *e = &args->cp_data->entrances[i];
            printf("Entrance #%d | ", i);
            printf("Boom Gate: %c | ", e->boom_gate.status);
            printf("Info Sign: %c | ", e->info_sign.display);
            printf("LPR: %s\n", e->lpr_sensor.license_plate);
        }
        printf("\n");
        for (int i = 0; i < LEVELS; i++) {
            level_t *l = &args->cp_data->levels[i];
            printf("Level #%d | ", i);
            printf("Capacity: %2d/%2d | ", 
                get_cars_in_level(args->plates_info, i), LEVEL_CAPACITY);
            printf("Temp Sensors: %d | ", l->temp_sensor);
            printf("Alarm: %d | ", l->alarm);
            printf("LPR: %s\n", l->lpr_sensor.license_plate);
        }
        printf("\n");
        for (int i = 0; i < EXITS; i++) {
            exit_t *e = &args->cp_data->exits[i];
            printf("Exit #%d | ", i);
            printf("Boom Gate: %c | ", e->boom_gate.status);
            printf("LPR: %s\n", e->lpr_sensor.license_plate);
        }
        printf("\n");
        printf("Total Billing Revenue: $%.2f\n", 
            (double)*args->total_revenue/100);
        msleep(50);
    }
}

void *auto_close_entrance_bg(void *data) {
    entrance_t *e = (entrance_t *)data;

    while (1) {
        pthread_mutex_lock(&e->boom_gate.mutex);
        while (e->boom_gate.status != 'O')
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        pthread_mutex_unlock(&e->boom_gate.mutex);

        msleep(20);

        pthread_mutex_lock(&e->boom_gate.mutex);
        e->boom_gate.status = 'L';
        pthread_cond_broadcast(&e->boom_gate.cond_var); 
        pthread_mutex_unlock(&e->boom_gate.mutex);
    }
}

void *auto_close_exit_bg(void *data) {
    exit_t *e = (exit_t *)data;

    while (1) {
        pthread_mutex_lock(&e->boom_gate.mutex);
        while (e->boom_gate.status != 'O')
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        pthread_mutex_unlock(&e->boom_gate.mutex);

        msleep(20);

        pthread_mutex_lock(&e->boom_gate.mutex);
        e->boom_gate.status = 'L';
        pthread_cond_broadcast(&e->boom_gate.cond_var);
        pthread_mutex_unlock(&e->boom_gate.mutex);
    }
}

int main(void) {
    shared_memory_t shm;
    if (!(get_shm_object(&shm))) {
        printf("Failed to get shm object.\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&rand_lock, NULL);

    srand(RAND_SEED);

    unsigned long int total_revenue = 0;

    size_t buckets = 16;

    htab_t plates_info;
    if (!htab_init(&plates_info, buckets)) {
        printf("Failed to init plates_info.\n");
        return EXIT_FAILURE;
    }

    if (!load_plates(&plates_info)) {
        printf("Failed to load plates.\n");
        return EXIT_FAILURE; 
    }
    
    pthread_t status_display_th_id;
    status_display_args_t status_display_args;
    status_display_args.cp_data = shm.data;
    status_display_args.plates_info = &plates_info;
    status_display_args.total_revenue = &total_revenue;
    pthread_create(&status_display_th_id, NULL, status_display, &status_display_args);
    
    pthread_t entrance_lpr_th_ids[ENTRANCES];
    entrance_lpr_args_t entrance_lpr_args[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;

        entrance_lpr_args_t args;
        args.entrance = &shm.data->entrances[i];
        args.plates_info = &plates_info;
        args.entrance_idx = i;
        entrance_lpr_args[i] = args;

        pthread_create(&th_id, NULL, entrance_lpr_sensor, &entrance_lpr_args[i]);
        entrance_lpr_th_ids[i] = th_id;
    }
    
    pthread_t level_lpr_th_ids[LEVELS];
    level_lpr_args_t level_lpr_args[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        pthread_t th_id;

        level_lpr_args_t args;
        args.level = &shm.data->levels[i];
        args.plates_info = &plates_info;
        args.level_idx = i;
        level_lpr_args[i] = args;

        pthread_create(&th_id, NULL, level_lpr_sensor, &level_lpr_args[i]);
        level_lpr_th_ids[i] = th_id;
    }

    pthread_t exit_lpr_th_ids[EXITS];
    exit_lpr_args_t exit_lpr_args[EXITS];
    for (int i = 0; i < EXITS; i++) {
        pthread_t th_id;

        exit_lpr_args_t args;
        args.exit = &shm.data->exits[i];
        args.plates_info = &plates_info;
        args.total_revenue = &total_revenue;
        args.exit_idx = i;
        exit_lpr_args[i] = args;

        pthread_create(&th_id, NULL, exit_lpr_sensor, &exit_lpr_args[i]);
        exit_lpr_th_ids[i] = th_id;
    }

    pthread_t close_entrance_bg[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, auto_close_entrance_bg, &shm.data->entrances[i]);
        close_entrance_bg[i] = th_id;
    }

    pthread_t close_exit_bg[EXITS];
    for (int i = 0; i < EXITS; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, auto_close_exit_bg, &shm.data->exits[i]);
        close_exit_bg[i] = th_id;
    }
    
    pthread_join(status_display_th_id, NULL);

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(entrance_lpr_th_ids[i], NULL);
    }

    for (int i = 0; i < LEVELS; i++) {
        pthread_join(level_lpr_th_ids[i], NULL);
    }

    for (int i = 0; i < EXITS; i++) {
        pthread_join(exit_lpr_th_ids[i], NULL);
    }

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(close_entrance_bg[i], NULL);
    }

    for (int i = 0; i < EXITS; i++) {
        pthread_join(close_exit_bg[i], NULL);
    }
    
    htab_destroy(&plates_info);

    return EXIT_SUCCESS;
}
