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
        htab_add(d, plate, 0);
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

    entrance_t *e = args->entrance;

    while (1) {
        pthread_mutex_lock(&e->lpr_sensor.mutex); 
        while (e->lpr_sensor.license_plate[0] == 0)
            pthread_cond_wait(&e->lpr_sensor.cond_var, &e->lpr_sensor.mutex);

        item_t *item = htab_find(args->plates_h, e->lpr_sensor.license_plate);
        if (item == NULL) {
            pthread_mutex_lock(&e->info_sign.mutex);
            e->info_sign.display = 'X';
            pthread_cond_broadcast(&e->info_sign.cond_var);
            pthread_mutex_unlock(&e->info_sign.mutex);
        } else {
            bool found_level = false;
            for (int i = 0; i < LEVELS; i++) {
                if (args->cars_per_level[i] < LEVEL_CAPACITY) {
                    pthread_mutex_lock(&e->info_sign.mutex);
                    e->info_sign.display = i + 49;
                    pthread_cond_broadcast(&e->info_sign.cond_var);
                    pthread_mutex_unlock(&e->info_sign.mutex);
                    
                    item->value = time(NULL);

                    found_level = true;
                    break;
                }
            }

            if (!found_level) {
                pthread_mutex_lock(&e->info_sign.mutex);
                e->info_sign.display = 'X';
                pthread_cond_broadcast(&e->info_sign.cond_var);
                pthread_mutex_unlock(&e->info_sign.mutex);
            }
        }

        e->lpr_sensor.license_plate[0] = 0;
        pthread_mutex_unlock(&e->lpr_sensor.mutex);
    }
}

void *level_lpr_sensor(void *data) {
    level_lpr_args_t *args;
    args = (level_lpr_args_t *)data;

    level_t *l = args->level;

    while (1) {
        pthread_mutex_lock(&l->lpr_sensor.mutex);
        while (l->lpr_sensor.license_plate[0] == 0)
            pthread_cond_wait(&l->lpr_sensor.cond_var, &l->lpr_sensor.mutex);

        item_t *item = htab_find(args->plates_h, l->lpr_sensor.license_plate);
        if (item != NULL) {
            if (item->value) {
                args->cars_per_level[args->level_idx]--;
            } else {
                args->cars_per_level[args->level_idx]++;
            }
            item->value = !item->value;
        }

        l->lpr_sensor.license_plate[0] = 0;
        pthread_mutex_unlock(&l->lpr_sensor.mutex);
    }
}

void *exit_lpr_sensor(void *data) {
    exit_lpr_args_t *args;
    args = (exit_lpr_args_t *)data;

    exit_t *e = args->exit;

    while (1) {
        pthread_mutex_lock(&e->lpr_sensor.mutex);
        while (e->lpr_sensor.license_plate[0] == 0)
            pthread_cond_wait(&e->lpr_sensor.cond_var, &e->lpr_sensor.mutex);

        item_t *item = htab_find(args->plates_time_entered, e->lpr_sensor.license_plate);
        if (item != NULL) {
            unsigned long int time_entered = (unsigned long int)item->value;
            if (time_entered > 0) {
                unsigned long int bill = ((time(NULL)-time_entered)*1000)*5;
                item_t *bill_item = htab_find(args->plates_billing, item->key);
                if (bill_item != NULL) bill_item->value = bill;
            }
        }

        e->lpr_sensor.license_plate[0] = 0;
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
            printf("Capacity: %d/%d | ", args->cars_per_level[i], LEVEL_CAPACITY);
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
        int total_revenue = 0;
        htab_t *ht = args->plates_billing;
        for (size_t i = 0; i < ht->size; i++) {
            if (ht->buckets[i] != NULL) {
                for (item_t *j = ht->buckets[i]; j != NULL; j = j->next) {
                    if (j != NULL) {
                        total_revenue += j->value;
                    }
                }
            }
        }
        printf("Total Billing Revenue: $%'.2d\n", total_revenue/100);
        msleep(50);
    }
}

void *auto_open_entrance_bg(void *data) {
    entrance_t *e;
    e = (entrance_t *)data; 

    while (1) {
        while (!isdigit(e->info_sign.display))
            pthread_cond_wait(&e->info_sign.cond_var, &e->info_sign.mutex); 

        pthread_mutex_lock(&e->boom_gate.mutex);
        while (e->boom_gate.status != 'C')
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        e->boom_gate.status = 'R';
        pthread_cond_broadcast(&e->boom_gate.cond_var);
        pthread_mutex_unlock(&e->boom_gate.mutex);

        pthread_mutex_unlock(&e->info_sign.mutex);
    }
}

void *auto_open_exit_bg(void *data) {
    exit_t *e;
    e = (exit_t *)data; 

    while (1) {
        pthread_mutex_lock(&e->lpr_sensor.mutex);
        while (e->lpr_sensor.license_plate[0] == 0)
            pthread_cond_wait(&e->lpr_sensor.cond_var, &e->lpr_sensor.mutex);
        pthread_mutex_unlock(&e->lpr_sensor.mutex);

        pthread_mutex_lock(&e->boom_gate.mutex);
        while (e->boom_gate.status != 'C')
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        e->boom_gate.status = 'R';
        pthread_cond_broadcast(&e->boom_gate.cond_var);
        pthread_mutex_unlock(&e->boom_gate.mutex);
    }
}

void *auto_close_entrance_bg(void *data) {
    entrance_t *e;
    e = (entrance_t *)data;

    while (1) {
        pthread_mutex_lock(&e->boom_gate.mutex);
        while (e->boom_gate.status != 'O')
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        msleep(20);
        e->boom_gate.status = 'L';
        pthread_cond_broadcast(&e->boom_gate.cond_var); 
        pthread_mutex_unlock(&e->boom_gate.mutex);
    }
}

void *auto_close_exit_bg(void *data) {
    exit_t *e;
    e = (exit_t *)data;

    while (1) {
        pthread_mutex_lock(&e->boom_gate.mutex);
        while (e->boom_gate.status != 'O')
            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
        msleep(20);
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

    srand(RAND_SEED);

    size_t buckets = 16;

    htab_t plates_billing;
    if (!htab_init(&plates_billing, buckets)) {
        printf("Failed to init plates_billing\n");
        return EXIT_FAILURE;
    }

    if (!load_plates(&plates_billing)) {
        printf("Failed to load plates: billing\n");
        return EXIT_FAILURE; 
    }

    htab_t plates_time_entered;
    if (!htab_init(&plates_time_entered, buckets)) {
        printf("Failed to init plates_time_entered\n");
        return EXIT_FAILURE;
    }

    if (!load_plates(&plates_time_entered)) {
        printf("Failed to load plates: time entered\n");
        return EXIT_FAILURE;
    }

    htab_t plates_in_levels;
    if (!htab_init(&plates_in_levels, buckets)) {
        printf("Failed to init plates_in_levels\n");
        return EXIT_FAILURE;
    }

    if (!load_plates(&plates_in_levels)) {
        printf("Failed to load plates: plates in levels\n");
        return EXIT_FAILURE;
    }
    
    int cars_per_level[LEVELS];
    for (int i = 0; i < LEVELS; i++)
        cars_per_level[i] = 0;
    
    pthread_t status_display_th_id;

    status_display_args_t status_display_args;
    status_display_args.cp_data = shm.data;
    status_display_args.cars_per_level = (int *)&cars_per_level;
    status_display_args.plates_billing = &plates_billing;

    pthread_create(&status_display_th_id, NULL, status_display, &status_display_args);
    
    pthread_t entrance_lpr_th_ids[ENTRANCES];
    entrance_lpr_args_t entrance_lpr_args[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;

        entrance_lpr_args_t args;
        args.entrance = &shm.data->entrances[i];
        args.plates_h = &plates_time_entered;
        args.cars_per_level = (int *)&cars_per_level;
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
        args.plates_h = &plates_in_levels;
        args.cars_per_level = (int *)&cars_per_level;
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
        args.plates_billing = &plates_billing;
        args.plates_time_entered = &plates_time_entered;
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

    pthread_t open_entrance_bg[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, auto_open_entrance_bg, &shm.data->entrances[i]);
        open_entrance_bg[i] = th_id;
    }
    
    pthread_t open_exit_bg[EXITS];
    for (int i = 0; i < EXITS; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, auto_open_exit_bg, &shm.data->exits[i]);
        open_exit_bg[i] = th_id;
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

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(open_entrance_bg[i], NULL);
    }

    for (int i = 0; i < EXITS; i++) {
        pthread_join(open_exit_bg[i], NULL);
    }
    
    // TODO: create billings.text

    htab_destroy(&plates_billing);
    htab_destroy(&plates_time_entered);
    htab_destroy(&plates_in_levels);

    return EXIT_SUCCESS;
}
