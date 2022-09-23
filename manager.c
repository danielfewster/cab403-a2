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

bool get_shm_object(shm_t *shm) {
    if ((shm->fd = shm_open(SHM_KEY, O_RDWR, 0)) < 0) {
        shm->data = NULL;
        return false;
    }

    shm->data = mmap(0, SHM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) return false;

    return true;
}

int main(void) {
    shm_t shm;
    if (!(get_shm_object(&shm))) {
        printf("Failed to get shm object.\n");
        return EXIT_FAILURE;
    }

    Dict plates_dict = DictCreate();

    FILE *file = fopen("plates.txt", "r");
    if (file == NULL) return EXIT_FAILURE;

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
        char plate[plate_chars+1];
        for (int j = 0; j < plate_chars; j++) {
            plate[j] = plates_buff[idx++];
        }
        plate[plate_chars] = '\0';
        DictInsert(plates_dict, plate, "0");
    }

    // NOTE: why dict? making key value to what? or do we just care about bucket scaling?
    const elt_t *v = DictSearch(plates_dict, "799BUO");
    printf("%s\n", v->key);

    fclose(file);
    DictDestroy(plates_dict);

    return EXIT_SUCCESS;
}