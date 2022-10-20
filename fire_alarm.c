#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "common.h"

static int alarm_type = 0;
static int alarm_active = 0;
static pthread_mutex_t alarm_mutex;
static pthread_cond_t alarm_condvar;

static int msleep(long msec)
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

static int get_shm_object(shared_memory_t *shm) {
    if ((shm->fd = shm_open(SHM_KEY, O_RDWR, 0)) < 0) {
        shm->data = NULL;
        return 0;
    }

    shm->data = mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) return 0;

    return 1;
}

static int compare(const void *first, const void *second) {
	return *((const int *)first) - *((const int *)second);
}

static void *level_temp_monitor(void *data) {
    level_t *l = (level_t *)data;

    const int MIN_HIGH_TEMP = 58;
    const int MIN_ROR = 8;
    const int RAW_BUFF_SIZE = 5;
    const int SMOOTH_BUFF_SIZE = 30;

    int raw_idx = 0;
    int raw_count = 0;
    int raw_circ_buff[RAW_BUFF_SIZE];
    for (int i = 0; i < RAW_BUFF_SIZE; i++)
        raw_circ_buff[i] = 0;

    int smooth_idx = 0;
    int smooth_count = 0;
    int smooth_circ_buff[SMOOTH_BUFF_SIZE];
    for (int i = 0; i < SMOOTH_BUFF_SIZE; i++)
        smooth_circ_buff[i] = 0;

    while (1) {
        if (smooth_count >= SMOOTH_BUFF_SIZE) {
            int high_temp_count = 0;
            for (int i = 0; i < SMOOTH_BUFF_SIZE; i++) {
                if (smooth_circ_buff[i] >= MIN_HIGH_TEMP) {
                    high_temp_count++;
                }
            }

            // NOTE: Fixed Temperature
            if (high_temp_count >= (SMOOTH_BUFF_SIZE*0.9)) {
                pthread_mutex_lock(&alarm_mutex);
                alarm_type = 1;
                alarm_active = 1;
                pthread_cond_broadcast(&alarm_condvar);
                pthread_mutex_unlock(&alarm_mutex);
            }

            // NOTE: Rate of Rise
            int oldest_temp = smooth_circ_buff[smooth_idx];
            int newest_temp = smooth_circ_buff[((smooth_idx-1)+SMOOTH_BUFF_SIZE)%SMOOTH_BUFF_SIZE];
            if ((newest_temp - oldest_temp) >= MIN_ROR) {
                pthread_mutex_lock(&alarm_mutex);
                alarm_type = 2;
                alarm_active = 1;
                pthread_cond_broadcast(&alarm_condvar);
                pthread_mutex_unlock(&alarm_mutex);
            }
        }

        if (raw_count >= RAW_BUFF_SIZE) {
            int raw_temp_buff[RAW_BUFF_SIZE];
            for (int i = 0; i < RAW_BUFF_SIZE; i++)
                raw_temp_buff[i] = raw_circ_buff[i];

            qsort(raw_temp_buff, RAW_BUFF_SIZE, sizeof(int), compare);
            smooth_circ_buff[smooth_idx++] = raw_temp_buff[(RAW_BUFF_SIZE/2)-1];
            smooth_idx %= SMOOTH_BUFF_SIZE;
            if (smooth_count < SMOOTH_BUFF_SIZE) smooth_count++;
        }

        raw_circ_buff[raw_idx++] = l->temp_sensor;
        raw_idx %= RAW_BUFF_SIZE;
        if (raw_count < RAW_BUFF_SIZE) raw_count++;

        msleep(2);
    }

    return NULL;
}

static void *open_bg(void *data) {
    boom_gate_t *bg = (boom_gate_t *)data;

    pthread_mutex_lock(&bg->mutex);
    while (1) {
		if (bg->status == (char)'C') {
			bg->status = (char)'R';
			pthread_cond_broadcast(&bg->cond_var);
		}
		pthread_cond_wait(&bg->cond_var, &bg->mutex);
    }
    pthread_mutex_unlock(&bg->mutex);

    return NULL;
}

int main(void) {
    shared_memory_t shm;
    if (!(get_shm_object(&shm))) {
        printf("Failed to get shm object.\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&alarm_mutex, NULL);
    pthread_cond_init(&alarm_condvar, NULL);

    pthread_t temp_monitor[LEVELS];
    for (int i = 0; i < LEVELS; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, level_temp_monitor, &shm.data->levels[i]);
        temp_monitor[i] = th_id;
    }

    pthread_mutex_lock(&alarm_mutex);
    while (!alarm_active)
        pthread_cond_wait(&alarm_condvar, &alarm_mutex);
    pthread_mutex_unlock(&alarm_mutex);

    if (alarm_type == 1) {
        printf("FIXED TEMP: DETECTED FIRE!\n");
    } else if (alarm_type == 2) {
        printf("RATE-OF-RISE: DETECTED FIRE!\n");
    }

    printf("ALARM ACTIVE!\n");

    // NOTE: Activate all alarms
    for (int i = 0; i < LEVELS; i++) {
        shm.data->levels[i].alarm = 1;
    }

    // NOTE: Open all boom gates
    pthread_t open_entrance_bg[ENTRANCES];
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, open_bg, &shm.data->entrances[i].boom_gate);
        open_entrance_bg[i] = th_id;
    }

    pthread_t open_exit_bg[EXITS];
    for (int i = 0; i < EXITS; i++) {
        pthread_t th_id;
        pthread_create(&th_id, NULL, open_bg, &shm.data->exits[i].boom_gate);
        open_exit_bg[i] = th_id;
    }

    // NOTE: Show "EVACUATE" msg on info signs
    const int msg_size = 8;
    char evac_msg[] = {'E', 'V', 'A', 'C', 'U', 'A', 'T', 'E'};
    while (1) {
        for (int i = 0; i < msg_size; i++) {
            char c = evac_msg[i];
            for (int j = 0; j < ENTRANCES; j++) {
                entrance_t *e = &shm.data->entrances[j];
                pthread_mutex_lock(&e->info_sign.mutex);
                e->info_sign.display = c;
                pthread_cond_broadcast(&e->info_sign.cond_var);
                pthread_mutex_unlock(&e->info_sign.mutex);
            }
            msleep(20);
        }
    }

    for (int i = 0; i < LEVELS; i++) {
        pthread_join(temp_monitor[i], NULL);
    }

    for (int i = 0; i < ENTRANCES; i++) {
        pthread_join(open_entrance_bg[i], NULL);
    }

    for (int i = 0; i < EXITS; i++) {
        pthread_join(open_exit_bg[i], NULL);
    }

    return EXIT_SUCCESS;
}
