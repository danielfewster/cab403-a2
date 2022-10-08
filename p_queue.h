#ifndef P_QUEUE
#define P_QUEUE

struct Node {
    void *data;
    struct Node *next;
};

struct Queue {
    struct Node *head;
    struct Node *tail;
    int size;
};

typedef struct Queue queue_t;

#endif
