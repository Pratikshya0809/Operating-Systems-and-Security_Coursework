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
/*  PRODUCER-CONSUMER USING SEMAPHORES */

#include <semaphore.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define ITEMS_TO_PRODUCE 10

int buffer[BUFFER_SIZE];
int in = 0, out = 0;
sem_t empty_slots, full_slots;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void *producer(void *arg) {
    for (int i = 1; i <= ITEMS_TO_PRODUCE; i++) {
        sem_wait(&empty_slots);
        pthread_mutex_lock(&buffer_mutex);
        buffer[in] = i;
        printf("[Producer] produced item %d at slot %d\n", i, in);
        in = (in + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&full_slots);
        usleep(10000);
    }
    return NULL;
}

void *consumer(void *arg) {
    for (int i = 1; i <= ITEMS_TO_PRODUCE; i++) {
        sem_wait(&full_slots);
        pthread_mutex_lock(&buffer_mutex);
        int item = buffer[out];
        printf("[Consumer] consumed item %d from slot %d\n", item, out);
        out = (out + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&empty_slots);
        usleep(15000);
    }
    return NULL;
}

/* Producer-Consumer demo */

void run_producer_consumer_demo() {
    printf("\n===== PART 2: PRODUCER-CONSUMER (SEMAPHORES) =====\n");
    sem_init(&empty_slots, 0, BUFFER_SIZE);
    sem_init(&full_slots, 0, 0);

    pthread_t prod_thread, cons_thread;
    pthread_create(&prod_thread, NULL, producer, NULL);
    pthread_create(&cons_thread, NULL, consumer, NULL);
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);
}
