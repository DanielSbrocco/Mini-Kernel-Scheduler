#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "process.h"

#define MAX_QUEUE 128  // limite de processos na fila

typedef struct {
    PCB* items[MAX_QUEUE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
} Queue;

// Funções principais
void queue_init(Queue* q);
void enqueue(Queue* q, PCB* p);
PCB* dequeue(Queue* q);

// Função para prioridade 
void enqueue_priority(Queue* q, PCB* p);

// Função utilitária
int queue_is_empty(Queue* q);

#endif
