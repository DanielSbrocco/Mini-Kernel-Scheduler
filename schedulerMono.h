#ifndef SCHEDULER_MONO_H
#define SCHEDULER_MONO_H

#include <pthread.h>
#include "process.h"
#include "queue.h"

// Tipos de escalonamento
#define FCFS 1
#define RR 2
#define PRIORITY 3

// Quantum fixo (500ms)
#define QUANTUM 500

// Estrutura global do escalonador
typedef struct {
    int policy;              // 1=FCFS, 2=RR, 3=Prioridade
    Queue* ready_queue;      // Fila de prontos
    int finished_processes;  // Contador de processos finalizados
    int total_processes;     // Total de processos no sistema
    pthread_mutex_t mutex;   // Protege variáveis globais
} Scheduler;

void* scheduler_run(void* arg); // Função da thread escalonadora
void scheduler_init(Scheduler* s, int policy, Queue* q, int total);
int scheduler_is_done(Scheduler* s);

#endif
