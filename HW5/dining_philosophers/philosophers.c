#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_PHILOSOPHERS 5
#define MEALS 3

pthread_mutex_t forks[NUM_PHILOSOPHERS];

void *philosopher(void *arg) {
    int id = *(int *)arg;
    int left = id;
    int right = (id + 1) % NUM_PHILOSOPHERS;
    int eaten = 0;

    while (eaten < MEALS) {
        /* ---- THINKING ---- */
        printf("Philosopher %d is thinking.\n", id);
        usleep(rand() % 200000);

        /* ---- PICK UP FORKS (ordered to avoid deadlock) ---- */
        /*
         * To prevent deadlock, the last philosopher picks up the
         * right fork first instead of the left. This breaks the
         * circular wait condition.
         */
        if (id == NUM_PHILOSOPHERS - 1) {
            pthread_mutex_lock(&forks[right]);
            printf("Philosopher %d picked up right fork %d.\n", id, right);
            pthread_mutex_lock(&forks[left]);
            printf("Philosopher %d picked up left  fork %d.\n", id, left);
        } else {
            pthread_mutex_lock(&forks[left]);
            printf("Philosopher %d picked up left  fork %d.\n", id, left);
            pthread_mutex_lock(&forks[right]);
            printf("Philosopher %d picked up right fork %d.\n", id, right);
        }

        /* ---- EATING ---- */
        eaten++;
        printf(">>> Philosopher %d is EATING (meal %d/%d).\n",
               id, eaten, MEALS);
        usleep(rand() % 200000);

        /* ---- PUT DOWN FORKS ---- */
        pthread_mutex_unlock(&forks[left]);
        pthread_mutex_unlock(&forks[right]);
        printf("Philosopher %d put down both forks.\n", id);
    }

    printf("=== Philosopher %d is DONE (ate %d meals). ===\n", id, MEALS);
    return NULL;
}

int main() {
    pthread_t philosophers[NUM_PHILOSOPHERS];
    int ids[NUM_PHILOSOPHERS];

    printf("=== Dining Philosophers Problem ===\n");
    printf("Philosophers: %d  |  Meals each: %d\n", NUM_PHILOSOPHERS, MEALS);
    printf("Deadlock prevention: asymmetric fork acquisition\n\n");

    srand(2);

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_init(&forks[i], NULL);
        ids[i] = i;
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_create(&philosophers[i], NULL, philosopher, &ids[i]);
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(philosophers[i], NULL);
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_destroy(&forks[i]);
    }

    printf("\nAll philosophers have finished their meals.\n");
    return 0;
}
