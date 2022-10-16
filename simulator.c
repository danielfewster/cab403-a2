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
#include <math.h>

#include "common.h"
#include "simulator.h"
#include "p_queue.c"
#include "car_vector.c"

int entrance_queue_cap = 10;
pthread_mutex_t rand_lock;

bool create_shm_object(shared_memory_t *shm) {
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

void init_shm_data(shared_memory_t *shm) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

    for (int i = 0; i < ENTRANCES; i++) {
        entrance_t *e = &shm->data->entrances[i];

        pthread_mutex_init(&e->lpr_sensor.mutex, &mutex_attr);
        pthread_cond_init(&e->lpr_sensor.cond_var, &cond_attr);

        pthread_mutex_init(&e->boom_gate.mutex, &mutex_attr);
        pthread_cond_init(&e->boom_gate.cond_var, &cond_attr);
        e->boom_gate.status = 'C';

        pthread_mutex_init(&e->info_sign.mutex, &mutex_attr);
        pthread_cond_init(&e->info_sign.cond_var, &cond_attr);
        e->info_sign.display = '*';
    }
    
    for (int i = 0; i < LEVELS; i++) {
        level_t *l = &shm->data->levels[i];
        
        pthread_mutex_init(&l->lpr_sensor.mutex, &mutex_attr);
        pthread_cond_init(&l->lpr_sensor.cond_var, &cond_attr);

        l->alarm = 0;
    }

    for (int i = 0; i < EXITS; i++) {
        exit_t *e = &shm->data->exits[i];

        pthread_mutex_init(&e->lpr_sensor.mutex, &mutex_attr);
        pthread_cond_init(&e->lpr_sensor.cond_var, &cond_attr);

        pthread_mutex_init(&e->boom_gate.mutex, &mutex_attr);
        pthread_cond_init(&e->boom_gate.cond_var, &cond_attr);
        e->boom_gate.status = 'C';
    }
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

bool load_plates(char *plates[]) {
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
        plates[i] = plate;
    }

    fclose(file);

    return true;
}

char *generate_plate() {
    char *new_plate = (char *)malloc((PLATE_CHARS+1)*sizeof(char));
    pthread_mutex_lock(&rand_lock);
    for (int i = 0; i < (PLATE_CHARS/2); i++) {
        new_plate[i] = (rand() % 10) + 48;
    }
    for (int i = (PLATE_CHARS/2); i < PLATE_CHARS; i++) {
        new_plate[i] = (rand() % 26) + 65;
    }
    pthread_mutex_unlock(&rand_lock);
    new_plate[PLATE_CHARS] = '\0';
    return new_plate;
}

bool is_plate_in_vec(char *plate, car_vector_t *vec) {
    for (size_t i = 0; i < vec->size; i++) {
        if (strncmp(vec->data[i].license_plate, plate, PLATE_CHARS) == 0) {
            return true;
        }
    }
    return false;
}

bool is_plate_in_queue(char *plate, queue_t *queue) {
    for (struct Node *h = queue->head; h != NULL; h = h->next) {
        car_t *c = (car_t *)h->data;
        if (strncmp(c->license_plate, plate, PLATE_CHARS) == 0) {
            return true;
        }
    }
    return false;
}

void *generate_cars(void *data) {
    generate_car_args_t *args;
    args = (generate_car_args_t *)data;

    char *plates[100];
    load_plates(plates);

    while (1) {
        pthread_mutex_lock(&rand_lock);
        int sleep_time = (rand() % 100) + 1;
        int valid_toggle = rand() % 2;
        int rand_idx = rand() % ENTRANCES;
        pthread_mutex_unlock(&rand_lock);
        
        queue_t *q = &args->entrance_queues[rand_idx];
        if (q->size < entrance_queue_cap) {
            car_t *new_car = (car_t *)malloc(sizeof(car_t));

            char *new_plate;

            if (valid_toggle) {
                pthread_mutex_lock(&rand_lock);
                new_plate = plates[rand() % 100];
                pthread_mutex_unlock(&rand_lock);
            } else {
                new_plate = generate_plate();
            }

            while (is_plate_in_vec(new_plate, args->cv) || is_plate_in_queue(new_plate, q)) {
                if (valid_toggle) {
                    pthread_mutex_lock(&rand_lock);
                    new_plate = plates[rand() % 100];
                    pthread_mutex_unlock(&rand_lock);
                } else {
                    new_plate = generate_plate();
                }
            }

            strcpy(new_car->license_plate, new_plate);
            enqueue(q, (void *)new_car);
        }
        
        msleep(sleep_time);
    }
}

void *entering_car(void *data) {
    entering_car_args_t *args;
    args = (entering_car_args_t *)data;

    entrance_t *e = args->entrance;

    while (1) {
        if (!isEmpty(args->entrance_queue)) {
            car_t *front_car = (car_t *)dequeue(args->entrance_queue);
            msleep(2);

            pthread_mutex_lock(&e->lpr_sensor.mutex);
            strncpy(e->lpr_sensor.license_plate, front_car->license_plate, PLATE_CHARS);
            pthread_cond_broadcast(&e->lpr_sensor.cond_var);
            pthread_mutex_unlock(&e->lpr_sensor.mutex);

            printf("%s arrives and waits for info sign.\n", front_car->license_plate);

            pthread_mutex_lock(&e->info_sign.mutex);
            while (e->info_sign.display == '*')
                pthread_cond_wait(&e->info_sign.cond_var, &e->info_sign.mutex); 

            if (isdigit(e->info_sign.display)) {
                int level_to_park = (e->info_sign.display-'0')-1;
                front_car->level_parked = level_to_park;

                printf("%s permitted to park at level %d.\n", 
                    front_car->license_plate, level_to_park);

                pthread_mutex_lock(&e->boom_gate.mutex);
                while (e->boom_gate.status != 'O')
                    pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
                pthread_mutex_unlock(&e->boom_gate.mutex);

                msleep(10);

                level_t *l = &args->levels[level_to_park];

                pthread_mutex_lock(&l->lpr_sensor.mutex);
                strncpy(l->lpr_sensor.license_plate, front_car->license_plate, PLATE_CHARS);
                pthread_cond_broadcast(&l->lpr_sensor.cond_var);
                pthread_mutex_unlock(&l->lpr_sensor.mutex);

                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                unsigned long int curr_time = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

                pthread_mutex_lock(&rand_lock);
                front_car->parking_exp = curr_time + ((rand()%9900)+100);
                pthread_mutex_unlock(&rand_lock);

                printf("%s will park for %ld ms.\n", 
                    front_car->license_plate, front_car->parking_exp - curr_time);

                cv_push(args->parked_cars, *front_car);
            } else {
                printf("%s not permitted and leaves.\n", front_car->license_plate);
                free(front_car);
            }

            e->info_sign.display = '*';
            pthread_mutex_unlock(&e->info_sign.mutex);   
        }
    }
}

void *leaving_cars(void *data) {
    leaving_car_args_t *args;
    args = (leaving_car_args_t *)data;

    while (1) {
        for (size_t i = 0; i < args->parked_cars->size; i++) {
            car_t *curr_car = &args->parked_cars->data[i];
            if (curr_car != NULL) {
                if (curr_car->level_parked == args->level_idx) {
                    level_t *l = &args->levels[args->level_idx];

                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    unsigned long int curr_time = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);

                    if (curr_time > curr_car->parking_exp) {
                        printf("%s's parking has expired and prepares to leave.\n", 
                            curr_car->license_plate);

                        pthread_mutex_lock(&l->lpr_sensor.mutex);
                        strncpy(l->lpr_sensor.license_plate, curr_car->license_plate, PLATE_CHARS);
                        pthread_cond_broadcast(&l->lpr_sensor.cond_var);
                        pthread_mutex_unlock(&l->lpr_sensor.mutex);

                        printf("%s starts to leave.\n", curr_car->license_plate);

                        msleep(10);

                        pthread_mutex_lock(&rand_lock);
                        int rand_idx = rand() % EXITS;
                        pthread_mutex_unlock(&rand_lock);

                        exit_t *e = &args->exits[rand_idx];

                        pthread_mutex_lock(&e->lpr_sensor.mutex);
                        strncpy(e->lpr_sensor.license_plate, curr_car->license_plate, PLATE_CHARS);
                        pthread_cond_broadcast(&e->lpr_sensor.cond_var);
                        pthread_mutex_unlock(&e->lpr_sensor.mutex);

                        pthread_mutex_lock(&e->boom_gate.mutex);
                        while (e->boom_gate.status != 'O')
                            pthread_cond_wait(&e->boom_gate.cond_var, &e->boom_gate.mutex);
                        pthread_mutex_unlock(&e->boom_gate.mutex);

                        printf("%s left at exit %d.\n", curr_car->license_plate, rand_idx);
                        cv_remove_at(args->parked_cars, i);
                    }
                }
            }
        }
    }
}

void *sim_entrance_boom_gate(void *data) {
    entrance_t *entrance = (entrance_t *)data;

    while (1) {
        boom_gate_t *bg = &entrance->boom_gate;

        pthread_mutex_lock(&bg->mutex);
        while (bg->status == 'O' || bg->status == 'C')
            pthread_cond_wait(&bg->cond_var, &bg->mutex);

        if (bg->status == 'R') {
            printf("    Raising Entrance Boom Gate...\n");
            msleep(10);
            bg->status = 'O';
            pthread_cond_broadcast(&bg->cond_var);
        } else if (bg->status == 'L') {
            printf("    Lowering Entrance Boom Gate...\n");
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
        boom_gate_t *bg = &exit->boom_gate;

        pthread_mutex_lock(&bg->mutex);
        while (bg->status == 'O' || bg->status == 'C')
            pthread_cond_wait(&bg->cond_var, &bg->mutex);
        
        if (bg->status == 'R') {
            printf("    Raising Exit Boom Gate...\n");
            msleep(10);
            bg->status = 'O';
            pthread_cond_broadcast(&bg->cond_var);
        } else if (bg->status == 'L') {
            printf("    Lowering Exit Boom Gate...\n");
            msleep(10);
            bg->status = 'C';
            pthread_cond_broadcast(&bg->cond_var);
        }

        pthread_mutex_unlock(&bg->mutex);
    }
}

void *level_temp_sensors(void *data) {
    level_t *l;
    l = (level_t *)data;

    while (1) {
        pthread_mutex_lock(&rand_lock);
        int sleep_time = (rand() % 5) + 1;
        int new_temp = (rand() % 20) + 20;
        if ((rand() % 10) == 0) new_temp += rand() % 60;
        pthread_mutex_unlock(&rand_lock);

        l->temp_sensor = new_temp;
        msleep(sleep_time);
    }
}

int main(void) {
    shared_memory_t shm;
    if (!(create_shm_object(&shm))) {
        printf("Failed to create shm object.\n");
        return EXIT_FAILURE;
    }

    init_shm_data(&shm);

    car_vector_t parked_cars;
    cv_init(&parked_cars);

    pthread_mutex_init(&rand_lock, NULL);

    srand(RAND_SEED);

    queue_t entrance_queues[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        queue_t *q = newQueue();
        entrance_queues[i] = *q;
    }

    pthread_t generate_cars_th_id;
    generate_car_args_t generate_car_args;
    generate_car_args.cv = &parked_cars;
    generate_car_args.entrance_queues = (queue_t *)&entrance_queues;
    pthread_create(&generate_cars_th_id, NULL, generate_cars, &generate_car_args);

    pthread_t sim_entering_cars[ENTRANCES];
    entering_car_args_t entering_car_args[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;

        entering_car_args_t args;
        args.entrance = &shm.data->entrances[i];
        args.entrance_queue = &entrance_queues[i];
        args.levels = shm.data->levels;
        args.parked_cars = &parked_cars;
        entering_car_args[i] = args;

        pthread_create(&th_id, NULL, entering_car, &entering_car_args[i]);
        sim_entering_cars[i] = th_id;
    }

    pthread_t sim_leaving_cars[LEVELS];
    leaving_car_args_t leaving_car_args[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        pthread_t th_id;

        leaving_car_args_t args;
        args.level_idx = i;
        args.exits = shm.data->exits;
        args.levels = shm.data->levels;
        args.parked_cars = &parked_cars;
        leaving_car_args[i] = args;

        pthread_create(&th_id, NULL, leaving_cars, &leaving_car_args[i]);
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
    
    pthread_t sim_temp_sensors[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, level_temp_sensors, &shm.data->levels[i]);
        sim_temp_sensors[i] = th_id;
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
    
    for (int i = 0; i < LEVELS; i++) {
        pthread_join(sim_temp_sensors[i], NULL);
    }
    
    cv_destroy(&parked_cars);

    for (int i = 0; i < ENTRANCES; i++) {
        free(&entrance_queues[i]);
    }

    munmap(shm.data, SHM_SIZE);

    return EXIT_SUCCESS;
}
