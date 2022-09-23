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
#include <time.h>
#include <errno.h>  

#include "common.h"
#include "simulator.h"

bool create_shm_object(shm_t *shm) {
    shm_unlink(SHM_KEY);

    if ((shm->fd = shm_open(SHM_KEY, O_CREAT | O_RDWR, 0666)) < 0) {
        shm->data = NULL;
        return false;
    }

    if (ftruncate(shm->fd, SHM_SIZE) != 0) {
        shm->data = NULL;
        return false;
    }

    shm->data = mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) return false;

    return true;
}

void init_shm_data(shm_t *shm) {
    shm->data->entrances = (entrance_t *)malloc(ENTRANCES*sizeof(entrance_t)); 
    shm->data->exits = (exit_t *)malloc(EXITS*sizeof(exit_t));
    shm->data->levels = (level_t *)malloc(LEVELS*sizeof(level_t));

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

    for (int i = 0; i < ENTRANCES; i++) {
        lpr_sensor_t *lpr_sensor = malloc(sizeof(lpr_sensor_t));
        pthread_mutex_init(&lpr_sensor->mutex, &mutex_attr);
        pthread_cond_init(&lpr_sensor->cond_var, &cond_attr);

        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        pthread_mutex_init(&boom_gate->mutex, &mutex_attr);
        pthread_cond_init(&boom_gate->cond_var, &cond_attr);
        boom_gate->status = 'C';

        info_sign_t *info_sign = malloc(sizeof(info_sign_t));
        pthread_mutex_init(&info_sign->mutex, &mutex_attr);
        pthread_cond_init(&info_sign->cond_var, &cond_attr);

        entrance_t *entrance = malloc(sizeof(entrance_t));
        entrance->lpr_sensor = lpr_sensor;
        entrance->boom_gate = boom_gate;
        entrance->info_sign = info_sign;

        shm->data->entrances[i] = *entrance;
    }

    for (int i = 0; i < EXITS; i++) {
        lpr_sensor_t *lpr_sensor = malloc(sizeof(lpr_sensor_t));
        pthread_mutex_init(&lpr_sensor->mutex, &mutex_attr);
        pthread_cond_init(&lpr_sensor->cond_var, &cond_attr);

        boom_gate_t *boom_gate = malloc(sizeof(boom_gate_t));
        pthread_mutex_init(&boom_gate->mutex, &mutex_attr);
        pthread_cond_init(&boom_gate->cond_var, &cond_attr);
        boom_gate->status = 'C';

        exit_t *exit;
        exit->lpr_sensor = lpr_sensor;
        exit->boom_gate = boom_gate;

        shm->data->exits[i] = *exit;
    }

    for (int i = 0; i < LEVELS; i++) {
        lpr_sensor_t *lpr_sensor = malloc(sizeof(lpr_sensor_t));
        pthread_mutex_init(&lpr_sensor->mutex, &mutex_attr);
        pthread_cond_init(&lpr_sensor->cond_var, &cond_attr);

        level_t *level;
        level->lpr_sensor = lpr_sensor;
        level->alarm = 0;

        shm->data->levels[i] = *level;
    }
}

void free_shm_data(shm_t *shm) {
    for (int i = 0; i < ENTRANCES; i++) {
        entrance_t *entrance = &shm->data->entrances[i];
        free(entrance->lpr_sensor);
        free(entrance->boom_gate);
        free(entrance->info_sign);
    }
    free(shm->data->entrances);

    for (int i = 0; i < EXITS; i++) {
        exit_t *exit = &shm->data->exits[i]; 
        free(exit->lpr_sensor);
        free(exit->boom_gate);
    }
    free(shm->data->exits);

    for (int i = 0; i < LEVELS; i++) {
        level_t *level = &shm->data->levels[i];  
        free(level->lpr_sensor);
    }
    free(shm->data->levels);
}

char *generate_plate() {
    int plate_chars = 6;
    char *new_plate = malloc((plate_chars+1)*sizeof(char));
    for (int i = 0; i < (plate_chars/2); i++) {
        new_plate[i] = (rand() % 10) + 48;
    }
    for (int i = (plate_chars/2); i < plate_chars; i++) {
        new_plate[i] = (rand() % 26) + 65;
    }
    new_plate[plate_chars] = '\0';
    return new_plate;
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

void *simulate_cars(void *data) {
    cp_data_t *cp_data;
    cp_data = (cp_data_t *)data;

    printf("%p", cp_data);

    while (1) {
        
        msleep((rand() % 100) + 1);
    }
}

int main(void) {
    shm_t shm;
    if (!(create_shm_object(&shm))) {
        printf("Failed to create shm object.\n");
        return 0;
    }

    init_shm_data(&shm);

    srand(0);
    /*
    for (int i = 0; i < 10; i++) {
        char *new_plate = generate_plate();
        printf("plate: %s\n", new_plate);
        free(new_plate);
    }*/

    pthread_t car_sim_thread_id;
    pthread_create(&car_sim_thread_id, NULL, simulate_cars, shm.data);
    
    pthread_join(car_sim_thread_id, NULL);

    free_shm_data(&shm);

    return EXIT_SUCCESS;
}
