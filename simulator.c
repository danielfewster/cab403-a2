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
#include <string.h>
#include <ctype.h>

#include "common.h"
#include "simulator.h"
#include "car_queue.c"
#include "car_vector.c"

pthread_mutex_t rand_lock;

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

        exit_t *exit = malloc(sizeof(exit_t));
        exit->lpr_sensor = lpr_sensor;
        exit->boom_gate = boom_gate;

        shm->data->exits[i] = *exit;
    }

    for (int i = 0; i < LEVELS; i++) {
        lpr_sensor_t *lpr_sensor = malloc(sizeof(lpr_sensor_t));
        pthread_mutex_init(&lpr_sensor->mutex, &mutex_attr);
        pthread_cond_init(&lpr_sensor->cond_var, &cond_attr);

        level_t *level = malloc(sizeof(level_t));
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
    pthread_mutex_lock(&rand_lock);
    for (int i = 0; i < (plate_chars/2); i++) {
        new_plate[i] = (rand() % 10) + 48;
    }
    for (int i = (plate_chars/2); i < plate_chars; i++) {
        new_plate[i] = (rand() % 26) + 65;
    }
    pthread_mutex_unlock(&rand_lock);
    //new_plate[plate_chars] = '\0';
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

void *generate_cars(void *data) {
    queue_t *entrance_queues;
    entrance_queues = (queue_t *)data;

    while (1) {
        pthread_mutex_lock(&rand_lock);
        int rand_idx = rand() % ENTRANCES;
        int sleep_time = (rand() % 100) + 1;
        pthread_mutex_unlock(&rand_lock);
        
        queue_t *q = &entrance_queues[rand_idx];
        if (!isFull(q)) {
            car_t *car = (car_t *)malloc(sizeof(car_t));
            strcpy(car->license_plate, generate_plate());
            enqueue(q, car);
        }

        msleep(sleep_time);
    }
}

void *entering_car(void *data) {
    entering_car_args_t *args;
    args = (entering_car_args_t *)data;

    while (1) {
        car_t *front_car = dequeue(args->entrance_queue);
        if (front_car == NULL) continue;
        msleep(2);

        entrance_t *e = args->entrance;

        pthread_mutex_lock(&e->lpr_sensor->mutex);
        strcpy(e->lpr_sensor->license_plate, front_car->license_plate);
        pthread_cond_broadcast(&e->lpr_sensor->cond_var);
        pthread_mutex_unlock(&e->lpr_sensor->mutex);

        pthread_mutex_lock(&e->info_sign->mutex);
        pthread_cond_wait(&e->info_sign->cond_var, &e->info_sign->mutex); 

        if (isdigit(e->info_sign->display)) {
            int level_to_park = e->info_sign->display - '0';

            pthread_mutex_lock(&e->boom_gate->mutex);
            while (e->boom_gate->status != 'O')
                pthread_cond_wait(&e->boom_gate->cond_var, &e->boom_gate->mutex); 

            msleep(10);

            level_t *l = &args->levels[level_to_park-1];

            front_car->level_parked = level_to_park;

            pthread_mutex_lock(&l->lpr_sensor->mutex);
            strcpy(l->lpr_sensor->license_plate, front_car->license_plate);
            pthread_cond_broadcast(&l->lpr_sensor->cond_var);
            pthread_mutex_unlock(&l->lpr_sensor->mutex);

            pthread_mutex_lock(&rand_lock);
            front_car->parking_exp = (time(NULL)*1000) + ((rand()%9900)+100);
            pthread_mutex_unlock(&rand_lock);

            cv_push(args->parked_cars, *front_car);

            pthread_mutex_unlock(&e->boom_gate->mutex);
        } else {
            free(front_car->license_plate);
            free(front_car);
        }

        pthread_mutex_unlock(&e->info_sign->mutex);
    }
}

void *leaving_cars(void *data) {
    leaving_car_args_t *args;
    args = (leaving_car_args_t *)data;

    while (1) {
        unsigned long int curr_time = time(NULL) * 1000;
        level_t *l = &args->levels[args->level_idx];

        for (int i = 0; i < args->parked_cars->size; i++) {
            car_t *curr_car = &args->parked_cars->data[i];
            if (curr_car->level_parked-1 == args->level_idx) {
                if (curr_time > curr_car->parking_exp) {
                    msleep(10);

                    pthread_mutex_lock(&l->lpr_sensor->mutex);
                    strcpy(l->lpr_sensor->license_plate, curr_car->license_plate);
                    pthread_cond_broadcast(&l->lpr_sensor->cond_var);
                    pthread_mutex_unlock(&l->lpr_sensor->mutex);

                    pthread_mutex_lock(&rand_lock);
                    int rand_idx = rand() % EXITS;
                    pthread_mutex_unlock(&rand_lock);

                    exit_t *e = &args->exits[rand_idx];

                    pthread_mutex_lock(&e->lpr_sensor->mutex);
                    strcpy(e->lpr_sensor->license_plate, curr_car->license_plate);
                    pthread_cond_broadcast(&e->lpr_sensor->cond_var);
                    pthread_mutex_unlock(&e->lpr_sensor->mutex);

                    pthread_mutex_lock(&e->boom_gate->mutex);
                    pthread_cond_wait(&e->boom_gate->cond_var, &e->boom_gate->mutex); 

                    if (e->boom_gate->status == 'O') { 
                        cv_remove_at(args->parked_cars, i);
                        free(curr_car->license_plate);
                        free(curr_car);
                    }

                    pthread_mutex_unlock(&e->boom_gate->mutex);
                }
            }
        }
    }
}

void *sim_entrance_boom_gate(void *data) {
    entrance_t *entrance = (entrance_t *)data;

    while (1) {
        boom_gate_t *bg = entrance->boom_gate;

        pthread_mutex_lock(&bg->mutex);
        pthread_cond_wait(&bg->cond_var, &bg->mutex); 

        if (bg->status == 'R') {
            msleep(10);
            bg->status = 'O';
            pthread_cond_broadcast(&bg->cond_var);
        } else if (bg->status == 'L') {
            msleep(10);
            bg->status = 'C';
            pthread_cond_broadcast(&bg->cond_var);
        }

        pthread_mutex_unlock(&bg->mutex);
    }
}

void *sim_exit_boom_gate(void *data) {
    exit_t *exit = (exit_t *)data;

    while (1) {
        boom_gate_t *bg = exit->boom_gate;

        pthread_mutex_lock(&bg->mutex);
        pthread_cond_wait(&bg->cond_var, &bg->mutex); 

        if (bg->status == 'R') {
            msleep(10);
            bg->status = 'O';
            pthread_cond_broadcast(&bg->cond_var);
        } else if (bg->status == 'L') {
            msleep(10);
            bg->status = 'C';
            pthread_cond_broadcast(&bg->cond_var);
        }

        pthread_mutex_unlock(&bg->mutex);
    }
}

int main(void) {
    shm_t shm;
    if (!(create_shm_object(&shm))) {
        printf("Failed to create shm object.\n");
        return 0;
    }

    car_vector_t parked_cars;
    cv_init(&parked_cars);

    pthread_mutex_init(&rand_lock, NULL);

    init_shm_data(&shm);

    srand(0);

    int entrance_queue_capacity = 10;
    queue_t entrance_queues[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        queue_t *q = create_queue(entrance_queue_capacity);
        entrance_queues[i] = *q;
    }

    pthread_t generate_cars_th_id;
    pthread_create(&generate_cars_th_id, NULL, generate_cars, &entrance_queues);

    pthread_t sim_entering_cars[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;

        entering_car_args_t args;
        args.entrance = &shm.data->entrances[i];
        args.entrance_queue = &entrance_queues[i];
        args.levels = shm.data->levels;
        args.parked_cars = &parked_cars;

        pthread_create(&th_id, NULL, entering_car, &args);
        sim_entering_cars[i] = th_id;
    }

    pthread_t sim_leaving_cars[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        pthread_t th_id;

        leaving_car_args_t args;
        args.level_idx = i;
        args.exits = shm.data->exits;
        args.levels = shm.data->levels;
        args.parked_cars = &parked_cars;

        pthread_create(&th_id, NULL, leaving_cars, &args);
        sim_leaving_cars[i] = th_id;
    }

    pthread_t sim_entrance_boom_gates[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, sim_entrance_boom_gate, &shm.data->entrances[i]);
        sim_entrance_boom_gates[i] = th_id;
    }

    pthread_t sim_exit_boom_gates[EXITS];
    for (int i = 0; i < EXITS; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, sim_exit_boom_gate, &shm.data->exits[i]);
        sim_exit_boom_gates[i] = th_id;
    }

    pthread_join(generate_cars_th_id, NULL);

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(sim_entering_cars[i], NULL);
    }

    for (int i = 0; i < LEVELS; i++) {
        pthread_join(sim_leaving_cars[i], NULL);
    }

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(sim_entrance_boom_gates[i], NULL);
    }

    for (int i = 0; i < EXITS; i++) {
        pthread_join(sim_exit_boom_gates[i], NULL);
    }

    for (size_t i = 0; i < parked_cars.size; i++) {
        free(&parked_cars.data[i].license_plate);
        free(&parked_cars.data[i]);
    }

    cv_destroy(&parked_cars);

    for (int i = 0; i < ENTRANCES; i++) {
        free(&entrance_queues[i]);
    }

    free_shm_data(&shm);

    return EXIT_SUCCESS;
}
