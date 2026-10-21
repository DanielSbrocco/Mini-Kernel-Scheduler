#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void queue_init(Queue* q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

int queue_is_empty(Queue* q) {
    return (q->count == 0);
}

void enqueue(Queue* q, PCB* p) {
    pthread_mutex_lock(&q->mutex);

    if (q->count < MAX_QUEUE) {
        q->rear = (q->rear + 1) % MAX_QUEUE;
        q->items[q->rear] = p;
        q->count++;
    } else {
        fprintf(stderr, "Erro: Fila cheia!\n");
    }

    pthread_mutex_unlock(&q->mutex);
}

PCB* dequeue(Queue* q) {
    pthread_mutex_lock(&q->mutex);

    PCB* p = NULL;
    if (q->count > 0) {
        p = q->items[q->front];
        q->front = (q->front + 1) % MAX_QUEUE;
        q->count--;
    }

    pthread_mutex_unlock(&q->mutex);
    return p;
}

// Insere processo respeitando prioridade (1 é mais alto)
void enqueue_priority(Queue* q, PCB* p) {
    pthread_mutex_lock(&q->mutex);

    if (q->count >= MAX_QUEUE) {
        fprintf(stderr, "Erro: Fila cheia!\n");
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    // IMPLEMENTAÇÃO SIMPLES: Reconstruir fila ordenada
    PCB* temp_array[MAX_QUEUE];
    int temp_count = 0;

    // 1. Remove todos elementos da fila para array temporário
    while (q->count > 0) {
        temp_array[temp_count] = q->items[q->front];
        q->front = (q->front + 1) % MAX_QUEUE;
        q->count--;
        temp_count++;
    }

    // 2. Adiciona novo processo ao array temporário
    temp_array[temp_count] = p;
    temp_count++;

    // 3. Ordena array por prioridade (bubble sort simples)
    for (int i = 0; i < temp_count - 1; i++) {
        for (int j = 0; j < temp_count - i - 1; j++) {
            if (temp_array[j]->priority > temp_array[j + 1]->priority) {
                PCB* temp = temp_array[j];
                temp_array[j] = temp_array[j + 1];
                temp_array[j + 1] = temp;
            }
        }
    }

    // 4. Reinsere todos elementos na fila (já ordenados)
    q->front = 0;
    q->rear = -1;
    q->count = 0;

    for (int i = 0; i < temp_count; i++) {
        q->rear = (q->rear + 1) % MAX_QUEUE;
        q->items[q->rear] = temp_array[i];
        q->count++;
    }

    pthread_mutex_unlock(&q->mutex);
}