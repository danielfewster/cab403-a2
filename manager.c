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

#include "dict.c"
#include "common.h"
#include "manager.h"

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

void *monitor_lpr_sensors(void *data) {
    thread_args_t *args;
    args = (thread_args_t *)data;

    while (1) {
        for (int i = 0; i < ENTRANCES; i++) {
            entrance_t *e = &args->cp_data->entrances[i];

            pthread_mutex_lock(&e->lpr_sensor->mutex);
            pthread_cond_wait(&e->lpr_sensor->cond_var, &e->lpr_sensor->mutex); 
        
            

            pthread_mutex_unlock(&e->lpr_sensor->mutex);
        }
    }
}

int main(void) {
    shm_t shm;
    if (!(get_shm_object(&shm))) {
        printf("Failed to get shm object.\n");
        return EXIT_FAILURE;
    }

    Dict plates_dict = DictCreate();

    if (!load_plates(&plates_dict)) {
        return EXIT_FAILURE;
    }

    thread_args_t thread_args;
    thread_args.cp_data = shm.data;

    pthread_t monitor_lpr_t_id;

    pthread_create(&monitor_lpr_t_id, NULL, monitor_lpr_sensors, &thread_args);

    pthread_join(monitor_lpr_t_id, NULL);

    DictDestroy(plates_dict);

    return EXIT_SUCCESS;
}
