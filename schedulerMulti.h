#ifndef SCHEDULER_MULTI_H
#define SCHEDULER_MULTI_H

#include <pthread.h>
#include "process.h"
#include "queue.h"

#define NUM_CPUS 2
#define QUANTUM 500

// Políticas de escalonamento
enum {
    FCFS = 1,
    RR = 2,
    PRIORITY = 3
};

// Estrutura para representar uma CPU
typedef struct {
    int cpu_id;
    PCB* current_process;
    pthread_t scheduler_thread;
    pthread_mutex_t cpu_mutex;
} CPU;

// Estrutura principal do escalonador multiprocessador
typedef struct {
    int policy;
    Queue* ready_queue;
    int finished_processes;
    int total_processes;
    int all_processes_arrived;    
    pthread_mutex_t mutex;
    CPU cpus[NUM_CPUS];
} SchedulerMulti;

// Estrutura para passar argumentos para thread do escalonador
typedef struct {
    SchedulerMulti* scheduler;
    int cpu_id;
} SchedulerArgs;

// Funções
void scheduler_multi_init(SchedulerMulti* s, int policy, Queue* q, int total);
int scheduler_multi_is_done(SchedulerMulti* s);
void* scheduler_cpu_run(void* arg);

#endif