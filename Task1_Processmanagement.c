
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

/* RACE CONDITION DEMONSTRATION */

#define NUM_THREADS_RACE 4
#define INCREMENTS_PER_THREAD 100000

long shared_counter_unsafe = 0;
long shared_counter_safe = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void *increment_unsafe(void *arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        shared_counter_unsafe++;   /* NOT atomic -> race condition */
    }
    return NULL;
}

void *increment_safe(void *arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        pthread_mutex_lock(&counter_mutex);
        shared_counter_safe++;     /* protected critical section */
        pthread_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

void run_race_condition_demo() {
    printf("\n===== PART 1: RACE CONDITION DEMO =====\n");
    pthread_t threads[NUM_THREADS_RACE];
    long expected = (long)NUM_THREADS_RACE * INCREMENTS_PER_THREAD;

    /* --- Unsynchronized version --- */
    for (int i = 0; i < NUM_THREADS_RACE; i++)
        pthread_create(&threads[i], NULL, increment_unsafe, NULL);
    for (int i = 0; i < NUM_THREADS_RACE; i++)
        pthread_join(threads[i], NULL);

    printf("Expected value       : %ld\n", expected);
    printf("Unsynchronized result: %ld  %s\n", shared_counter_unsafe,
           (shared_counter_unsafe != expected) ? "(RACE CONDITION - lost updates)" : "(got lucky this run)");

    /* --- Synchronized version (mutex fix) --- */
    for (int i = 0; i < NUM_THREADS_RACE; i++)
        pthread_create(&threads[i], NULL, increment_safe, NULL);
    for (int i = 0; i < NUM_THREADS_RACE; i++)
        pthread_join(threads[i], NULL);

    printf("Mutex-protected result: %ld  %s\n", shared_counter_safe,
           (shared_counter_safe == expected) ? "(CORRECT - race fixed)" : "(unexpected!)");
}

/* PRODUCER-CONSUMER USING SEMAPHORES */

#define BUFFER_SIZE 5
#define ITEMS_TO_PRODUCE 10

int buffer[BUFFER_SIZE];
int in = 0, out = 0;

sem_t empty_slots;   /* counts empty slots in buffer */
sem_t full_slots;    /* counts filled slots in buffer */
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void *producer(void *arg) {
    for (int i = 1; i <= ITEMS_TO_PRODUCE; i++) {
        sem_wait(&empty_slots);          /* wait for empty slot */
        pthread_mutex_lock(&buffer_mutex);

        buffer[in] = i;
        printf("[Producer] produced item %d at slot %d\n", i, in);
        in = (in + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&full_slots);           /* signal item available */
        usleep(10000);
    }
    return NULL;
}

void *consumer(void *arg) {
    for (int i = 1; i <= ITEMS_TO_PRODUCE; i++) {
        sem_wait(&full_slots);           /* wait for filled slot */
        pthread_mutex_lock(&buffer_mutex);

        int item = buffer[out];
        printf("[Consumer] consumed item %d from slot %d\n", item, out);
        out = (out + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&empty_slots);          /* signal empty slot free */
        usleep(15000);
    }
    return NULL;
}

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

/* ROUND-ROBIN SCHEDULER SIMULATION */

typedef struct {
    int pid;
    int burst_time;
    int remaining_time;
    int arrival_time;
    int waiting_time;
    int turnaround_time;
    int completed;
} Process;

void run_round_robin_demo() {
    printf("\n===== PART 3: ROUND-ROBIN SCHEDULER SIMULATION =====\n");

    int quantum = 2;
    Process processes[] = {
        {1, 5, 5, 0, 0, 0, 0},
        {2, 3, 3, 0, 0, 0, 0},
        {3, 8, 8, 0, 0, 0, 0},
        {4, 6, 6, 0, 0, 0, 0}
    };
    int n = sizeof(processes) / sizeof(processes[0]);
    int time = 0, completed_count = 0;

    printf("Quantum = %d\n", quantum);
    printf("Gantt chart order:\n");

    while (completed_count < n) {
        int idle = 1;
        for (int i = 0; i < n; i++) {
            if (!processes[i].completed && processes[i].remaining_time > 0) {
                idle = 0;
                int slice = (processes[i].remaining_time < quantum)
                                ? processes[i].remaining_time
                                : quantum;

                printf("  | P%d (runs %d) ", processes[i].pid, slice);

                time += slice;
                processes[i].remaining_time -= slice;

                if (processes[i].remaining_time == 0) {
                    processes[i].completed = 1;
                    processes[i].turnaround_time = time - processes[i].arrival_time;
                    processes[i].waiting_time =
                        processes[i].turnaround_time - processes[i].burst_time;
                    completed_count++;
                }
            }
        }
        if (idle) break;
    }
    printf("|\n\n");

    float total_wait = 0, total_turnaround = 0;
    printf("PID\tBurst\tWaiting\tTurnaround\n");
    for (int i = 0; i < n; i++) {
        printf("P%d\t%d\t%d\t%d\n", processes[i].pid, processes[i].burst_time,
               processes[i].waiting_time, processes[i].turnaround_time);
        total_wait += processes[i].waiting_time;
        total_turnaround += processes[i].turnaround_time;
    }
    printf("\nAverage Waiting Time    : %.2f\n", total_wait / n);
    printf("Average Turnaround Time : %.2f\n", total_turnaround / n);
}

/* DEADLOCK PREVENTION DEMO */

pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

void *thread_A(void *arg) {
    printf("[Thread A] trying to lock lock1 then lock2 (ordered)\n");
    pthread_mutex_lock(&lock1);
    printf("[Thread A] acquired lock1\n");
    usleep(50000); /* force interleaving so a real deadlock WOULD occur without ordering */
    pthread_mutex_lock(&lock2);
    printf("[Thread A] acquired lock2 -> doing work\n");

    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);
    printf("[Thread A] released both locks\n");
    return NULL;
}

void *thread_B(void *arg) {
    printf("[Thread B] trying to lock lock1 then lock2 (ordered - SAME order as Thread A)\n");
    /* NOTE: prevention = thread B also locks lock1 first, lock2 second.
       (An unsafe version would lock lock2 first, lock1 second, which
       could deadlock against Thread A - left as a comment for the report.) */
    pthread_mutex_lock(&lock1);
    printf("[Thread B] acquired lock1\n");
    usleep(50000);
    pthread_mutex_lock(&lock2);
    printf("[Thread B] acquired lock2 -> doing work\n");

    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);
    printf("[Thread B] released both locks\n");
    return NULL;
}

void run_deadlock_prevention_demo() {
    printf("\n===== PART 4: DEADLOCK PREVENTION DEMO =====\n");
    pthread_t tA, tB;

    pthread_create(&tA, NULL, thread_A, NULL);
    pthread_create(&tB, NULL, thread_B, NULL);

    pthread_join(tA, NULL);
    pthread_join(tB, NULL);

    printf("No deadlock occurred because both threads acquire locks in the SAME order.\n");
}

/* MAIN */
int main() {
    printf("###############################################\n");
    printf("# TASK 1: PROCESS MANAGEMENT AND THREADING     #\n");
    printf("###############################################\n");

    run_race_condition_demo();
    run_producer_consumer_demo();
    run_round_robin_demo();
    run_deadlock_prevention_demo();

    printf("\nAll demonstrations completed successfully.\n");
    return 0;
}

