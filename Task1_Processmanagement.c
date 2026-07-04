#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTS_PER_THREAD 100000

long shared_counter_unsafe = 0;
long shared_counter_safe = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void *increment_unsafe(void *arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
          shared_counter_unsafe++;
    }
    return NULL;
}
void *increment_safe(void *arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        pthread_mutex_lock(&counter_mutex);
        shared_counter_safe++;
        pthread_mutex_unlock(&counter_mutex);
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
    printf("Unsynchronized result: %ld\n", shared_counter_unsafe);
 /* Safe version (mutex fix) */
  for (int i = 0; i < NUM_THREADS; i++)
    pthread_create(&threads[i], NULL, increment_safe, NULL);

  for (int i = 0; i < NUM_THREADS; i++)
    pthread_join(threads[i], NULL);

   printf("Mutex-protected result: %ld (CORRECT)\n", shared_counter_safe);

    return 0;
}
