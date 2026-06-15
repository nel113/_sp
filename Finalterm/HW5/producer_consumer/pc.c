#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define ITEMS_TO_PRODUCE 20

int buffer[BUFFER_SIZE];
int count = 0;    /* number of items in the buffer */
int in = 0;       /* next write index */
int out = 0;      /* next read index  */

pthread_mutex_t mutex;
pthread_cond_t  not_full;
pthread_cond_t  not_empty;

void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < ITEMS_TO_PRODUCE; i++) {
        int item = id * 100 + i;

        pthread_mutex_lock(&mutex);

        /* Wait while the buffer is full */
        while (count == BUFFER_SIZE) {
            pthread_cond_wait(&not_full, &mutex);
        }

        /* Insert the item */
        buffer[in] = item;
        in = (in + 1) % BUFFER_SIZE;
        count++;

        printf("[Producer %d] produced %3d  | buffer: %d/%d\n",
               id, item, count, BUFFER_SIZE);

        /* Signal that the buffer is not empty */
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);

        usleep(rand() % 100000); /* simulate work */
    }
    return NULL;
}

void *consumer(void *arg) {
    int id = *(int *)arg;
    int consumed = 0;
    int total = ITEMS_TO_PRODUCE * 2; /* 2 producers */

    while (consumed < total) {
        pthread_mutex_lock(&mutex);

        /* Wait while the buffer is empty AND items still expected */
        while (count == 0 && consumed < total) {
            pthread_cond_wait(&not_empty, &mutex);
        }

        if (count == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        /* Remove the item */
        int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        count--;
        consumed++;

        printf("[Consumer %d] consumed %3d  | buffer: %d/%d\n",
               id, item, count, BUFFER_SIZE);

        /* Signal that the buffer is not full */
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        usleep(rand() % 150000); /* simulate work */
    }
    printf("[Consumer %d] finished (consumed %d items)\n", id, consumed);
    return NULL;
}

int main() {
    pthread_t prod[2], cons[2];
    int prod_ids[2] = {1, 2};
    int cons_ids[2] = {1, 2};

    printf("=== Producer-Consumer Problem ===\n");
    printf("Buffer size: %d  |  Items per producer: %d  |  Total items: %d\n\n",
           BUFFER_SIZE, ITEMS_TO_PRODUCE, ITEMS_TO_PRODUCE * 2);

    srand(1); /* deterministic rand for demo */

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    for (int i = 0; i < 2; i++) {
        pthread_create(&prod[i], NULL, producer, &prod_ids[i]);
        pthread_create(&cons[i], NULL, consumer, &cons_ids[i]);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(prod[i], NULL);
        pthread_join(cons[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

    printf("\nAll producers and consumers finished.\n");
    return 0;
}
