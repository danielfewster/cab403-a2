#ifndef P_QUEUE
#define P_QUEUE

struct Node {
    void *data;
    struct Node *next;
};

struct Node* newNode(void *data){
    struct Node *node = (struct Node*)malloc(sizeof(struct Node));

    node->data = data;
    node->next = NULL;

    return node;
}

struct Queue {
    struct Node *head;
    struct Node *tail;
};

typedef struct Queue queue_t;

#endif
