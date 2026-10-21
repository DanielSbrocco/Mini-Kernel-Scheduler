#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "process.h"
#include "schedulerMulti.h"
#include "queue.h"
#include "logger.h"

// pega o tempo atual em ms
long current_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// struct pra passar args pro gerador
typedef struct {
    PCB** processes;
    int n;
    Queue* ready_q;
    long start_time;
} GeneratorArgs;

// funcao de comparacao pro qsort
int compare_arrival_time(const void* a, const void* b) {
    PCB* pcb_a = *(PCB**)a;
    PCB* pcb_b = *(PCB**)b;
    return pcb_a->start_time - pcb_b->start_time;
}

// thread que gera os processos no tempo certo
void* process_generator(void* arg) {
    GeneratorArgs* args = (GeneratorArgs*) arg;

    // ordena por tempo de chegada
    qsort(args->processes, args->n, sizeof(PCB*), compare_arrival_time);

    for (int i = 0; i < args->n; i++) {
        PCB* pcb = args->processes[i];

        // espera o tempo de chegada
        while (current_millis() - args->start_time < pcb->start_time) {
            usleep(1000);
        }

        // cria threads do processo
        start_process_threads(pcb);

        // coloca na fila
        enqueue(args->ready_q, pcb);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    logger_init();

    int n;
    fscanf(f, "%d", &n);

    PCB** processes = (PCB**) malloc(sizeof(PCB*) * n);

    // le os processos do arquivo
    for (int i = 0; i < n; i++) {
        int dur, prio, nthreads, start;
        fscanf(f, "%d", &dur);
        fscanf(f, "%d", &prio);
        fscanf(f, "%d", &nthreads);
        fscanf(f, "%d", &start);

        processes[i] = create_process(i + 1, dur, prio, nthreads, start);
    }

    int policy;
    fscanf(f, "%d", &policy);
    fclose(f);

    // inicializa fila
    Queue ready_q;
    queue_init(&ready_q);

    // prepara o gerador
    GeneratorArgs gen_args;
    gen_args.processes = processes;
    gen_args.n = n;
    gen_args.ready_q = &ready_q;
    gen_args.start_time = current_millis();

    pthread_t generator_thread;
    pthread_create(&generator_thread, NULL, process_generator, &gen_args);

    // inicializa escalonador
    SchedulerMulti sched;
    scheduler_multi_init(&sched, policy, &ready_q, n);
    
    // cria uma thread pra cada cpu
    SchedulerArgs sched_args[NUM_CPUS];
    for (int i = 0; i < NUM_CPUS; i++) {
        sched_args[i].scheduler = &sched;
        sched_args[i].cpu_id = i;
        pthread_create(&sched.cpus[i].scheduler_thread, NULL, scheduler_cpu_run, &sched_args[i]);
    }

    // espera gerador terminar
    pthread_join(generator_thread, NULL);

    // avisa que todos os processos chegaram
    pthread_mutex_lock(&sched.mutex);
    sched.all_processes_arrived = 1;
    pthread_mutex_unlock(&sched.mutex);

    // espera os escalonadores
    for (int i = 0; i < NUM_CPUS; i++) {
        pthread_join(sched.cpus[i].scheduler_thread, NULL);
    }

    // espera todas as threads dos processos
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < processes[i]->num_threads; j++) {
            pthread_join(processes[i]->thread_ids[j], NULL);
        }
    }

    // salva o log
    logger_save("log_execucao_minikernel.txt");
    logger_destroy();

    // libera memoria
    for (int i = 0; i < n; i++) {
        free(processes[i]->thread_ids);
        free(processes[i]);
    }
    free(processes);

    return 0;
}