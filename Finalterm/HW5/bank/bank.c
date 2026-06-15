#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_TRANSACTIONS 100000

int balance = 0;
pthread_mutex_t lock;

void *deposit(void *arg) {
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        pthread_mutex_lock(&lock);
        balance = balance + 1;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void *withdraw(void *arg) {
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        pthread_mutex_lock(&lock);
        balance = balance - 1;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void *unsafe_deposit(void *arg) {
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        balance = balance + 1;
    }
    return NULL;
}

void *unsafe_withdraw(void *arg) {
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        balance = balance - 1;
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    printf("=== Bank Account Simulation ===\n");
    printf("Transactions: %d deposits + %d withdrawals\n\n",
           NUM_TRANSACTIONS, NUM_TRANSACTIONS);

    /* ----- WITH mutex (safe) ----- */
    balance = 0;
    pthread_mutex_init(&lock, NULL);

    pthread_create(&t1, NULL, deposit, NULL);
    pthread_create(&t2, NULL, withdraw, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_mutex_destroy(&lock);

    printf("[WITH mutex]    Final balance: %d  (expected: 0)\n", balance);

    /* ----- WITHOUT mutex (unsafe) ----- */
    balance = 0;

    pthread_create(&t1, NULL, unsafe_deposit, NULL);
    pthread_create(&t2, NULL, unsafe_withdraw, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("[WITHOUT mutex] Final balance: %d  (expected: 0)\n", balance);

    return 0;
}
