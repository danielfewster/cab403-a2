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

#include "dict.c"
#include "common.h"
#include "manager.h"

pthread_mutex_t rand_lock;

bool get_shm_object(shm_t *shm) {
    if ((shm->fd = shm_open(SHM_KEY, O_RDWR, 0)) < 0) {
        shm->data = NULL;
        return false;
    }

    shm->data = mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) return false;

    return true;
}

bool load_plates(Dict *d) {
    FILE *file = fopen("plates.txt", "r");
    if (file == NULL) return false;

    int plates_amount = 100;
    int plate_chars = 6;
    int idx = 0;
    char plates_buff[plate_chars*plates_amount];
    for (char c; c != EOF; c = fgetc(file)) {
        if (isalnum(c)) {
            plates_buff[idx++] = c;
        }
    }

    idx = 0;
    for (int i = 0; i < plates_amount; i++) {
        char plate[plate_chars];
        for (int j = 0; j < plate_chars; j++) {
            plate[j] = plates_buff[idx++];
        }
        DictInsert(*d, plate, "0");
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

void *entrance_lpr_sensor(void *data) {
    entrance_lpr_args_t *args;
    args = (entrance_lpr_args_t *)data;

    while (1) {
        entrance_t *e = &args->entrance;

        pthread_mutex_lock(&e->lpr_sensor->mutex);
        pthread_cond_wait(&e->lpr_sensor->cond_var, &e->lpr_sensor->mutex); 

        elt_t *el = DictSearch(args->plates_h, &e->lpr_sensor->license_plate);
        if (el == 0) {
            pthread_mutex_lock(&e->info_sign->mutex);
            e->info_sign->display = 'X';
            pthread_cond_broadcast(&e->info_sign->cond_var);
            pthread_mutex_unlock(&e->info_sign->mutex);
        } else {
            bool found_level = false;
            for (int i = 0; i < LEVELS; i++) {
                if (args->cars_per_level[i] < 20) {
                    pthread_mutex_lock(&e->info_sign->mutex);
                    e->info_sign->display = i + 49;
                    pthread_cond_broadcast(&e->info_sign->cond_var);
                    pthread_mutex_unlock(&e->info_sign->mutex);

                    pthread_mutex_lock(&e->boom_gate->mutex);
                    e->boom_gate->status = 'R';
                    pthread_cond_broadcast(&e->boom_gate->cond_var);
                    pthread_mutex_unlock(&e->boom_gate->mutex);

                    pthread_mutex_lock(&e->boom_gate->mutex);
                    pthread_cond_wait(&e->boom_gate->cond_var, &e->boom_gate->mutex);
                    msleep(20);
                    e->boom_gate->status = 'L';
                    pthread_cond_broadcast(&e->boom_gate->cond_var);
                    pthread_mutex_unlock(&e->boom_gate->mutex);

                    DictInsert(args->plates_h, el->key, time(NULL));

                    found_level = true;
                    break;
                }
            }

            if (!found_level) {
                pthread_mutex_lock(&e->info_sign->mutex);
                e->info_sign->display = 'X';
                pthread_cond_broadcast(&e->info_sign->cond_var);
                pthread_mutex_unlock(&e->info_sign->mutex);
            }
        }

        pthread_mutex_unlock(&e->lpr_sensor->mutex);
    }
}

void *level_lpr_sensor(void *data) {
    level_lpr_args_t *args;
    args = (level_lpr_args_t *)data;

    while (1) {
        level_t *l = &args->level;

        pthread_mutex_lock(&l->lpr_sensor->mutex);
        pthread_cond_wait(&l->lpr_sensor->cond_var, &l->lpr_sensor->mutex); 

        if ((*args->plates_h)->size < 20) {
            char *plate = &l->lpr_sensor->license_plate;
            elt_t *el = DictSearch(args->plates_h, plate);
            if (el == 0) {
                DictInsert(args->plates_h, plate, 0);
            } else {
                DictDelete(args->plates_h, el->key);
            }
        }

        pthread_mutex_unlock(&l->lpr_sensor->mutex);
    }
}

void *exit_lpr_sensor(void *data) {
    exit_lpr_args_t *args;
    args = (exit_lpr_args_t *)data;

    while (1) {
        exit_t *e = &args->exit;

        pthread_mutex_lock(&e->lpr_sensor->mutex);
        pthread_cond_wait(&e->lpr_sensor->cond_var, &e->lpr_sensor->mutex);

        elt_t *el = DictSearch(args->plate_time_entered, &e->lpr_sensor->license_plate);
        if (el != 0) {
            unsigned long int bill = (time(NULL) - (int)el->value) * 1000;

            elt_t *bill_node = DictSearch(args->plates_billing, el->key);
            if (bill_node == 0) {
                DictInsert(args->plates_billing, el->key, bill);
            } else {
                DictInsert(args->plates_billing, el->key, bill_node->value+bill);
            }

            pthread_mutex_lock(&e->boom_gate->mutex);
            e->boom_gate->status = 'R';
            pthread_cond_broadcast(&e->boom_gate->cond_var);
            pthread_mutex_unlock(&e->boom_gate->mutex);

            pthread_mutex_lock(&e->boom_gate->mutex);
            pthread_cond_wait(&e->boom_gate->cond_var, &e->boom_gate->mutex);
            msleep(20);
            e->boom_gate->status = 'L';
            pthread_cond_broadcast(&e->boom_gate->cond_var);
            pthread_mutex_unlock(&e->boom_gate->mutex);
        }

        pthread_mutex_unlock(&e->lpr_sensor->mutex);
    }
}

void *status_display(void *data) {
    status_display_args_t *args;
    args = (status_display_args_t *)data;

    while (1) {
        system("clear");

        for (int i = 0; i < ENTRANCES; i++) {
            entrance_t *e = &args->cp_data->entrances[i];
            printf("Entrance #%d\n", i);
            printf("LPR: %s | ", e->lpr_sensor->license_plate);
            printf("Boom Gate: %c | ", e->boom_gate->status);
            printf("Info Sign: %c\n", e->info_sign->display);
        }

        for (int i = 0; i < LEVELS; i++) {
            level_t *l = &args->cp_data->levels[i];
            printf("Level #%d\n", i);
            printf("LPR: %s | ", l->lpr_sensor->license_plate);
            printf("Capacity: %d/%d\n", args->plates_per_level[i]->size, LEVEL_CAPACITY);
        }

        for (int i = 0; i < EXITS; i++) {
            exit_t *e = &args->cp_data->exits[i];
            printf("Exit #%d\n", i);
            printf("LPR: %s | ", e->lpr_sensor->license_plate);
            printf("Boom Gate: %c\n", e->boom_gate->status);
        }

        int total_revenue = 0;
        for (int i = 0; i < (*args->plates_billing)->size; i++) {
            elt_t *el = (*args->plates_billing)->table[i];
            total_revenue += (int)el->value;
        }
        printf("Total Billing Revenue: $%d\n", total_revenue);

        msleep(50);
    }
}

int main(void) {
    shm_t shm;
    if (!(get_shm_object(&shm))) {
        printf("Failed to get shm object.\n");
        return EXIT_FAILURE;
    }

    srand(0);

    Dict plate_time_entered = DictCreate();
    if (!load_plates(&plate_time_entered)) {
        return EXIT_FAILURE;
    }

    Dict plates_billing = DictCreate();
    if (!load_plates(&plates_billing)) {
        return EXIT_FAILURE;
    }

    Dict plates_per_level[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        plates_per_level[i] = DictCreate();
    }

    int cars_per_level[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        cars_per_level[i] = 0;
    }

    pthread_t status_display_th_id;

    status_display_args_t status_display_args;
    status_display_args.cp_data = shm.data;
    status_display_args.plates_per_level = &plates_per_level;
    status_display_args.plates_billing = &plates_billing;

    pthread_create(&status_display_th_id, NULL, status_display, &status_display_args);

    pthread_t entrance_lpr_th_ids[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;

        entrance_lpr_args_t args;
        args.entrance = &shm.data->entrances[i];
        args.plates_h = &plate_time_entered;
        args.cars_per_level = &cars_per_level;

        pthread_create(&th_id, NULL, entrance_lpr_sensor, &args);
        entrance_lpr_th_ids[i] = th_id;
    }

    pthread_t level_lpr_th_ids[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        pthread_t th_id;

        level_lpr_args_t args;
        args.level = &shm.data->levels[i];
        args.plates_h = &plates_per_level[i];
        args.cars_per_level = &cars_per_level;
        args.level_idx = i;

        pthread_create(&th_id, NULL, level_lpr_sensor, &args);
        level_lpr_th_ids[i] = th_id;
    }

    pthread_t exit_lpr_th_ids[EXITS];
    for (int i = 0; i < EXITS; i++) {
        pthread_t th_id;

        exit_lpr_args_t args;
        args.exit = &shm.data->exits[i];
        args.plates_billing = &plates_billing;
        args.plate_time_entered = &plate_time_entered;
        args.cars_per_level = &cars_per_level;

        pthread_create(&th_id, NULL, exit_lpr_sensor, &args);
        exit_lpr_th_ids[i] = th_id;
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

    // TODO: create billings.text

    DictDestroy(plate_time_entered);
    DictDestroy(plates_billing);

    for (int i = 0; i < LEVELS; i++) {
        DictDestroy(plates_per_level[i]);
    }

    return EXIT_SUCCESS;
}
