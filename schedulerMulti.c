#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "schedulerMulti.h"
#include "logger.h"

// array pra marcar quais processos ja finalizaram
static int process_finish_reported[100] = {0}; // no maximo 100 processos
static pthread_mutex_t report_mutex = PTHREAD_MUTEX_INITIALIZER;


// inicializa o escalonador
void scheduler_multi_init(SchedulerMulti* s, int policy, Queue* q, int total) {
    s->policy = policy;
    s->ready_queue = q;
    s->finished_processes = 0;
    s->total_processes = total;
    s->all_processes_arrived = 0;
    pthread_mutex_init(&s->mutex, NULL);
    
    // zera o array
    for (int i = 0; i < 100; i++) {
        process_finish_reported[i] = 0;
    }
    
    // inicializa as cpus
    for (int i = 0; i < NUM_CPUS; i++) {
        s->cpus[i].cpu_id = i;
        s->cpus[i].current_process = NULL;
        pthread_mutex_init(&s->cpus[i].cpu_mutex, NULL);
    }
}

// ve se todos os processos terminaram
int scheduler_multi_is_done(SchedulerMulti* s) {
    pthread_mutex_lock(&s->mutex);
    int done = (s->finished_processes == s->total_processes);
    pthread_mutex_unlock(&s->mutex);
    return done;
}

// verifica se pode parar o escalonador
int should_terminate(SchedulerMulti* s) {
    pthread_mutex_lock(&s->mutex);
    
    if (s->finished_processes >= s->total_processes) {
        pthread_mutex_unlock(&s->mutex);
        return 1;
    }
    
    if (!s->all_processes_arrived) {
        pthread_mutex_unlock(&s->mutex);
        return 0;
    }
    
    pthread_mutex_lock(&s->ready_queue->mutex);
    int queue_empty = (s->ready_queue->count == 0);
    pthread_mutex_unlock(&s->ready_queue->mutex);
    
    if (!queue_empty) {
        pthread_mutex_unlock(&s->mutex);
        return 0;
    }
    
    // ve se alguma cpu ainda ta rodando algo
    for (int i = 0; i < NUM_CPUS; i++) {
        pthread_mutex_lock(&s->cpus[i].cpu_mutex);
        int has_process = (s->cpus[i].current_process != NULL);
        pthread_mutex_unlock(&s->cpus[i].cpu_mutex);
        if (has_process) {
            pthread_mutex_unlock(&s->mutex);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&s->mutex);
    return 1;
}

// funcao auxiliar pra ver se o processo ta rodando em outra cpu
static int is_shared_on_other_cpu(SchedulerMulti *sched, PCB *pcb, int my_cpu_id) {
    for (int j = 0; j < 2; ++j) {
        if (j == my_cpu_id) continue;
        pthread_mutex_lock(&sched->cpus[j].cpu_mutex);
        PCB *other = sched->cpus[j].current_process;
        pthread_mutex_unlock(&sched->cpus[j].cpu_mutex);
        if (other == pcb) return 1;
    }
    return 0;
}


// thread principal de cada cpu
void* scheduler_cpu_run(void* arg) {
    SchedulerArgs* args = (SchedulerArgs*) arg;
    SchedulerMulti* sched = args->scheduler;
    int cpu_id = args->cpu_id;
    CPU* my_cpu = &sched->cpus[cpu_id];
    char msg[128];

    
    // cpu 1 espera um pouco pra cpu 0 pegar primeiro processo
    if (cpu_id == 1) {
        usleep(10000); // 10ms
    }

    while (!should_terminate(sched)) {
        PCB* pcb = NULL;
        
        // tenta pegar da fila
        pcb = dequeue(sched->ready_queue);
        
        if (pcb == NULL) {
            // se nao tem nada na fila, ve se pode compartilhar processo
            for (int i = 0; i < NUM_CPUS; i++) {
                if (i != cpu_id) {
                    pthread_mutex_lock(&sched->cpus[i].cpu_mutex);
                    PCB* other_process = sched->cpus[i].current_process;
                    pthread_mutex_unlock(&sched->cpus[i].cpu_mutex);
                    
                    if (other_process != NULL) {
                        pthread_mutex_lock(&other_process->mutex);
                        // so compartilha se tem mais de uma thread
                        if (other_process->num_threads > 1 && 
                            other_process->state == RUNNING &&
                            other_process->remaining_time > 0) {
                            pcb = other_process;
                            pthread_mutex_unlock(&other_process->mutex);
                            break;
                        }
                        pthread_mutex_unlock(&other_process->mutex);
                    }
                }
            }
            
            if (pcb == NULL) {
                usleep(10000); // espera 10ms
                continue;
            }
        }

        pthread_mutex_lock(&pcb->mutex);

        // se ja terminou, pula
        if (pcb->state == FINISHED || pcb->remaining_time <= 0) {
            pthread_mutex_unlock(&pcb->mutex);
            continue;
        }

        // atualiza qual processo ta nessa cpu
        pthread_mutex_lock(&my_cpu->cpu_mutex);
        my_cpu->current_process = pcb;
        pthread_mutex_unlock(&my_cpu->cpu_mutex);

        /* tentativa de fazer cpu 0 sempre imprimir primeiro (nao funcionou muito bem) */
        pthread_mutex_lock(&sched->mutex);

        // ve se cpu 0 ta livre
        int cpu0_idle = 0;
        pthread_mutex_lock(&sched->cpus[0].cpu_mutex);
        if(sched->cpus[0].current_process == NULL){
            cpu0_idle = 1;
        }
        pthread_mutex_unlock(&sched->cpus[0].cpu_mutex);

        // tem processo esperando?
        int has_ready = 0;
        pthread_mutex_lock(&sched->ready_queue->mutex);
        // has_ready = (sched->ready_queue->count > 0);  //testei assim tbm
        if(sched->ready_queue->count > 0){
            has_ready = 1;
        }
        pthread_mutex_unlock(&sched->ready_queue->mutex);
        
        // cpu 1 da uma pausa pra cpu 0 logar primeiro
        if (cpu_id == 1 && cpu0_idle && has_ready) {
            pthread_mutex_unlock(&sched->mutex);
            usleep(2000);  // 2ms de delay
            pthread_mutex_lock(&sched->mutex);
        }

        pthread_mutex_unlock(&sched->mutex);

        // marca como rodando e acorda as threads
        if (pcb->state != RUNNING) {
            pcb->state = RUNNING;
            pthread_cond_broadcast(&pcb->cv);
        }
        pthread_mutex_unlock(&pcb->mutex);

        // ROUND ROBIN
        if (sched->policy == RR) {
            sprintf(msg, "[RR] Executando processo PID %d com quantum %dms // processador %d", pcb->pid, QUANTUM, cpu_id);
            log_event(msg);
            
            int elapsed = 0;
            int finished = 0;

            // roda ate acabar ou quantum estourar
            while (elapsed < QUANTUM) {
                usleep(5000);  // 5ms
                elapsed += 5;

                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED || pcb->remaining_time <= 0) {
                    finished = 1;
                    pthread_mutex_unlock(&pcb->mutex);
                    break;
                }
                pthread_mutex_unlock(&pcb->mutex);
            }

            if (finished) {
                // so imprime se ninguem ainda imprimiu
                int is_sharing = is_shared_on_other_cpu(sched, pcb, cpu_id);
                if (!is_sharing || (is_sharing && cpu_id == 0)) {
                    pthread_mutex_lock(&sched->mutex);
                    sched->finished_processes++;
                    pthread_mutex_unlock(&sched->mutex);

                    sprintf(msg, "[RR] Processo PID %d finalizado", pcb->pid);
                    log_event(msg);
                }
            } else {
                // acabou o quantum
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == RUNNING) {
                    pcb->state = READY;  
                    pthread_cond_broadcast(&pcb->cv);   
                }
                pthread_mutex_unlock(&pcb->mutex);

                // so coloca na fila se nao ta rodando em outra cpu
                int running_elsewhere = 0;
                for (int j = 0; j < NUM_CPUS; ++j) {
                    if (j == cpu_id) continue;
                    pthread_mutex_lock(&sched->cpus[j].cpu_mutex);
                    running_elsewhere |= (sched->cpus[j].current_process == pcb);
                    pthread_mutex_unlock(&sched->cpus[j].cpu_mutex);
                    if (running_elsewhere) break;
                }

                if (!running_elsewhere) {
                    enqueue(sched->ready_queue, pcb);
                }
            }

            // libera cpu
            pthread_mutex_lock(&my_cpu->cpu_mutex);
            my_cpu->current_process = NULL;
            pthread_mutex_unlock(&my_cpu->cpu_mutex);
            continue;   

        } 

        // FCFS
        else if(sched->policy == FCFS){
            sprintf(msg, "[FCFS] Executando processo PID %d // processador %d", pcb->pid, cpu_id);
            log_event(msg);
            
            // fica esperando terminar
            while (1) {
                pthread_mutex_lock(&pcb->mutex);
                
                if (pcb->state == FINISHED) {
                    int pid = pcb->pid;
                    pthread_mutex_unlock(&pcb->mutex);
                    
                    // so uma cpu imprime que terminou
                    pthread_mutex_lock(&report_mutex);
                    if (!process_finish_reported[pid]) {
                        process_finish_reported[pid] = 1;
                        
                        pthread_mutex_lock(&sched->mutex);
                        sched->finished_processes++;
                        pthread_mutex_unlock(&sched->mutex);
                        
                        sprintf(msg, "[FCFS] Processo PID %d finalizado", pid);
                        log_event(msg);
                        
                        // isso aqui era pra imprimir o que ta rodando nas outras cpus (???)
                        for (int j = 0; j < 2; ++j) {
                            if (j == cpu_id) continue; 
                            PCB *r = sched->cpus[j].current_process;
                            if (r && r->state == RUNNING) {
                                sprintf(msg, "[FCFS] Executando processo PID %d // processador %d", r->pid, j);
                                log_event(msg);
                            }
                        }
                    }
                    pthread_mutex_unlock(&report_mutex);
                    
                    break;
                }
                pthread_mutex_unlock(&pcb->mutex);
                usleep(10000); 
            }
        }

        // PRIORIDADE
        else if (sched->policy == PRIORITY) {
            sprintf(msg, "[PRIORITY] Executando processo PID %d prioridade %d // processador %d", 
                    pcb->pid, pcb->priority, cpu_id);
            log_event(msg);
            
            int elapsed = 0;
            int finished = 0;
            int preempted = 0;

            // roda ate terminar ou ser preemptado
            while (!finished && !preempted) {
                usleep(5000);  
                elapsed += 5;

                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED || pcb->remaining_time <= 0) {
                    finished = 1;
                    pthread_mutex_unlock(&pcb->mutex);
                    break;
                }
                pthread_mutex_unlock(&pcb->mutex);

                // depois do quantum ve se tem alguem com prioridade maior
                if (elapsed >= QUANTUM) {
                    pthread_mutex_lock(&sched->ready_queue->mutex);
                    
                    // procura o de maior prioridade
                    int highest_priority = 999;
                    for (int i = 0; i < sched->ready_queue->count; i++) {
                        int idx = (sched->ready_queue->front + i) % MAX_QUEUE;
                        PCB* waiting_pcb = sched->ready_queue->items[idx];
                        if (waiting_pcb && waiting_pcb->priority < highest_priority) {
                            highest_priority = waiting_pcb->priority;
                        }
                    }
                    
                    // menor numero = maior prioridade
                    if (highest_priority < pcb->priority) {
                        preempted = 1;
                    }
                    pthread_mutex_unlock(&sched->ready_queue->mutex);
                    
                    // se nao foi preemptado, reseta o tempo
                    if (!preempted) {
                        elapsed = 0;
                    }
                }
            }

            if (finished) {
                // evita imprimir duas vezes
                int is_sharing = is_shared_on_other_cpu(sched, pcb, cpu_id);
                if (!is_sharing || (is_sharing && cpu_id == 0)) {
                    pthread_mutex_lock(&sched->mutex);
                    sched->finished_processes++;
                    pthread_mutex_unlock(&sched->mutex);

                    sprintf(msg, "[PRIORITY] Processo PID %d finalizado", 
                            pcb->pid);
                    log_event(msg);
                }
            } else if (preempted) {
                // foi preemptado
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == RUNNING) {
                    pcb->state = READY;
                    pthread_cond_broadcast(&pcb->cv);
                }
                pthread_mutex_unlock(&pcb->mutex);

                // ve se nao ta em outra cpu
                int running_elsewhere = 0;
                for (int j = 0; j < NUM_CPUS; ++j) {
                    if (j == cpu_id) continue;
                    pthread_mutex_lock(&sched->cpus[j].cpu_mutex);
                    running_elsewhere |= (sched->cpus[j].current_process == pcb);
                    pthread_mutex_unlock(&sched->cpus[j].cpu_mutex);
                    if (running_elsewhere) break;
                }

                if (!running_elsewhere) {
                    enqueue_priority(sched->ready_queue, pcb);  // volta pra fila com prioridade
                }
            }

        }
        // limpa a cpu
        pthread_mutex_lock(&my_cpu->cpu_mutex);
        my_cpu->current_process = NULL;
        pthread_mutex_unlock(&my_cpu->cpu_mutex);
    }
    
    // so cpu 0 imprime msg final
    if (cpu_id == 0) {
        // espera todo mundo terminar
        while (1) {
            pthread_mutex_lock(&sched->mutex);
            int all_done = (sched->finished_processes >= sched->total_processes);
            pthread_mutex_unlock(&sched->mutex);
            
            if (all_done) {
                break;
            }
            usleep(50000); 
        }
        
        log_event("Escalonador terminou execução de todos processos");
    }

    return NULL;
}