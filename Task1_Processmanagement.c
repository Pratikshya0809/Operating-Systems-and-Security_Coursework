#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTS_PER_THREAD 100000

long shared_counter = 0;   /* shared, unprotected */

void *increment_unsafe(void *arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        shared_counter++;   /* NOT atomic -> race condition */
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    long expected = (long)NUM_THREADS * INCREMENTS_PER_THREAD;

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, increment_unsafe, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Expected value       : %ld\n", expected);
    printf("Unsynchronized result: %ld\n", shared_counter);
    printf("(If these differ, that's the race condition)\n");

    return 0;
}
