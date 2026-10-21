#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "process.h"
#include "logger.h"

// Cria um processo e inicializa variáveis
PCB* create_process(int pid, int dur, int prio, int nthreads, int start) {
    PCB* pcb = (PCB*) malloc(sizeof(PCB));
    if (!pcb) {
        perror("Erro ao alocar PCB");
        exit(1);
    }

    pcb->pid = pid;
    pcb->process_len = dur;
    pcb->remaining_time = dur;
    pcb->priority = prio;
    pcb->num_threads = nthreads;
    pcb->start_time = start;
    pcb->state = READY;

    pthread_mutex_init(&pcb->mutex, NULL);
    pthread_cond_init(&pcb->cv, NULL);

    pcb->thread_ids = (pthread_t*) malloc(sizeof(pthread_t) * nthreads);
    if (!pcb->thread_ids) {
        perror("Erro ao alocar threads do PCB");
        exit(1);
    }

    return pcb;
}

// Função que cada thread do processo executa
void* thread_func(void* arg) {
    TCB* tcb = (TCB*)arg;
    PCB* pcb = tcb->pcb;

    while (1) {
        pthread_mutex_lock(&pcb->mutex);

        // Espera até ser escalonado ou terminar
        while (pcb->state != RUNNING && pcb->state != FINISHED) {
            pthread_cond_wait(&pcb->cv, &pcb->mutex);
        }

        if (pcb->state == FINISHED) {
            pthread_mutex_unlock(&pcb->mutex);
            break;
        }

        pthread_mutex_unlock(&pcb->mutex);

        // Executa apenas enquanto estiver RUNNING
        while (1) {
            pthread_mutex_lock(&pcb->mutex);
            
            // Para se não estiver mais RUNNING ou já terminou
            if (pcb->state != RUNNING || pcb->remaining_time <= 0) {
                if (pcb->remaining_time <= 0) {
                    pcb->remaining_time = 0;
                    pcb->state = FINISHED;
                    pthread_cond_broadcast(&pcb->cv);
                }
                pthread_mutex_unlock(&pcb->mutex);
                break;
            }
            
            pthread_mutex_unlock(&pcb->mutex);

            // Simula 50ms de execução
            usleep(50 * 1000);

            // Decrementa tempo
            pthread_mutex_lock(&pcb->mutex);
            if (pcb->state == RUNNING) {
                pcb->remaining_time -= 50;
                if (pcb->remaining_time <= 0) {
                    pcb->remaining_time = 0;
                    pcb->state = FINISHED;
                    pthread_cond_broadcast(&pcb->cv);
                }
            }
            pthread_mutex_unlock(&pcb->mutex);
        }
    }

    free(tcb);
    return NULL;
}

// Cria as threads do processo (mas elas ficam esperando)
void start_process_threads(PCB* pcb) {
    for (int i = 0; i < pcb->num_threads; i++) {
        TCB* tcb = (TCB*) malloc(sizeof(TCB));
        tcb->pcb = pcb;
        tcb->thread_index = i;

        if (pthread_create(&pcb->thread_ids[i], NULL, thread_func, tcb) != 0) {
            perror("Erro ao criar thread do processo");
            exit(1);
        }
    }
}