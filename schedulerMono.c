#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "schedulerMono.h"
#include "logger.h"

// Inicializa escalonador
void scheduler_init(Scheduler* s, int policy, Queue* q, int total) {
    s->policy = policy;
    s->ready_queue = q;
    s->finished_processes = 0;
    s->total_processes = total;
    pthread_mutex_init(&s->mutex, NULL);
}

// Retorna se todos processos terminaram
int scheduler_is_done(Scheduler* s) {
    pthread_mutex_lock(&s->mutex);
    int done = (s->finished_processes == s->total_processes);
    pthread_mutex_unlock(&s->mutex);
    return done;
}

// Função da thread escalonadora
void* scheduler_run(void* arg) {
    Scheduler* sched = (Scheduler*) arg;
    // PCB* current_running = NULL; // processo atualmente executando

    while (!scheduler_is_done(sched)) {
        PCB* pcb = dequeue(sched->ready_queue);
        if (pcb == NULL) {
            usleep(1000); // espera 1ms se não há processos
            continue;
        }

        pthread_mutex_lock(&pcb->mutex);

        // Verifica se já terminou
        if (pcb->state == FINISHED) {
            pthread_mutex_unlock(&pcb->mutex);
            continue;
        }

        char msg[128];

        // =========================
        // POLÍTICA: FCFS
        // =========================
        if (sched->policy == FCFS) {
            sprintf(msg, "[FCFS] Executando processo PID %d", pcb->pid);
            log_event(msg);

            // Marca como RUNNING e acorda threads
            pcb->state = RUNNING;
            pthread_cond_broadcast(&pcb->cv);
            pthread_mutex_unlock(&pcb->mutex);

            // Espera até o processo terminar
            while (1) {
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED || pcb->remaining_time <= 0) {
                    pcb->state = FINISHED;
                    pthread_mutex_lock(&sched->mutex);
                    sched->finished_processes++;
                    pthread_mutex_unlock(&sched->mutex);
                    sprintf(msg, "[FCFS] Processo PID %d finalizado", pcb->pid);
                    log_event(msg);
                    pthread_mutex_unlock(&pcb->mutex);
                    break;
                }
                pthread_mutex_unlock(&pcb->mutex);
                usleep(10000); // espera 10ms antes de verificar novamente
            }
        }

        // =========================
        // POLÍTICA: Round Robin
        // =========================
        else if (sched->policy == RR) {
            sprintf(msg, "[RR] Executando processo PID %d com quantum %dms", pcb->pid, QUANTUM);
            log_event(msg);

            // Marca como RUNNING e acorda threads
            pcb->state = RUNNING;

            pthread_cond_broadcast(&pcb->cv);
            pthread_mutex_unlock(&pcb->mutex);

            // Monitora execução durante o quantum
            int quantum_used = 0;
            int process_finished = 0;
            
            while (quantum_used <= QUANTUM && !process_finished) {
                usleep(50 * 1000); // verifica a cada 50ms
                quantum_used += 50;

                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED || pcb->remaining_time <= 0) {
                    pcb->state = FINISHED;
                    pthread_mutex_lock(&sched->mutex);
                    sched->finished_processes++;
                    pthread_mutex_unlock(&sched->mutex);
                    sprintf(msg, "[RR] Processo PID %d finalizado", pcb->pid);
                    log_event(msg);
                    process_finished = 1;
                }
                pthread_mutex_unlock(&pcb->mutex);
            }

            // Se não terminou, faz preempção
            if (!process_finished) {
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == RUNNING && pcb->remaining_time > 0) {
                    pcb->state = READY;
                    pthread_cond_broadcast(&pcb->cv); // sinaliza threads para pararem
                    enqueue(sched->ready_queue, pcb);
                }
                pthread_mutex_unlock(&pcb->mutex);
            }
        }

        // =========================
        // POLÍTICA: Prioridade
        // =========================
        else if (sched->policy == PRIORITY) {
            sprintf(msg, "[PRIORITY] Executando processo PID %d prioridade %d ", pcb->pid, pcb->priority);
            log_event(msg);

            // Marca como RUNNING e acorda threads
            pcb->state = RUNNING;

            pthread_cond_broadcast(&pcb->cv);
            pthread_mutex_unlock(&pcb->mutex);

            // Monitora execução (verificando preempção continuamente)
            int process_finished = 0;
            int preempted = 0;
            
            while (!process_finished && !preempted) {
                usleep(50 * 1000); // verifica a cada 50ms

                pthread_mutex_lock(&pcb->mutex);
                
                // Verifica se terminou
                if (pcb->state == FINISHED || pcb->remaining_time <= 0) {
                    pcb->state = FINISHED;
                    pthread_mutex_lock(&sched->mutex);
                    sched->finished_processes++;
                    pthread_mutex_unlock(&sched->mutex);
                    sprintf(msg, "[RRIORITY] Processo PID %d finalizado", pcb->pid);
                    log_event(msg);
                    process_finished = 1;
                    // current_running = NULL;
                    pthread_mutex_unlock(&pcb->mutex);
                    continue;
                }
                pthread_mutex_unlock(&pcb->mutex);

                // PREEMPÇÃO: Verifica se há processo de maior prioridade na fila
                pthread_mutex_lock(&sched->ready_queue->mutex);
                if (sched->ready_queue->count > 0) {
                    // Procura o processo de maior prioridade na fila
                    int highest_priority = 999;
                    for (int i = 0; i < sched->ready_queue->count; i++) {
                        int idx = (sched->ready_queue->front + i) % MAX_QUEUE;
                        PCB* waiting_pcb = sched->ready_queue->items[idx];
                        if (waiting_pcb && waiting_pcb->priority < highest_priority) {
                            highest_priority = waiting_pcb->priority;
                        }
                    }
                    
                    // Se há processo de prioridade maior que o atual, marca para preempção
                    if (highest_priority < pcb->priority) {
                        preempted = 1;
                    }
                }
                pthread_mutex_unlock(&sched->ready_queue->mutex);
            }

            // Trata o resultado: se foi preemptado, reinsere na fila
            if (preempted && !process_finished) {
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == RUNNING && pcb->remaining_time > 0) {
                    pcb->state = READY;
                    pthread_cond_broadcast(&pcb->cv); // para as threads
                    enqueue_priority(sched->ready_queue, pcb);
                }
                pthread_mutex_unlock(&pcb->mutex);
            }
        }
    }

    log_event("Escalonador terminou execução de todos processos");
    return NULL;
}