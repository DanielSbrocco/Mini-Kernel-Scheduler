#ifndef PROCESS_H
#define PROCESS_H

#include <pthread.h>

// Estados de processo
typedef enum { READY, RUNNING, FINISHED } ProcessState;

// Bloco de Controle de Processo (PCB)
typedef struct {
    int pid;
    int process_len;     
    int remaining_time;  
    int priority;       
    int num_threads;
    int start_time;      
    
    ProcessState state;

    pthread_mutex_t mutex;
    pthread_cond_t cv;
    pthread_t* thread_ids;
} PCB;

// Bloco de Controle de Thread (TCB)
typedef struct {
    PCB* pcb;
    int thread_index;
} TCB;

// Funções
PCB* create_process(int pid, int dur, int prio, int nthreads, int start);
void start_process_threads(PCB* pcb);
void* thread_func(void* arg);

#endif