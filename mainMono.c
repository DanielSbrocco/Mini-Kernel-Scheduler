#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "process.h"
#include "schedulerMono.h"
#include "queue.h"
#include "logger.h"

// Utilitário para tempo em ms
long current_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// Estrutura para thread geradora
typedef struct {
    PCB** processes;
    int n;
    Queue* ready_q;
    long start_time; 
} GeneratorArgs;

// Função de comparação para ordenar por tempo de chegada
int compare_arrival_time(const void* a, const void* b) {
    PCB* pcb_a = *(PCB**)a;
    PCB* pcb_b = *(PCB**)b;
    return pcb_a->start_time - pcb_b->start_time;
}

// Thread geradora de processos
void* process_generator(void* arg) {
    GeneratorArgs* args = (GeneratorArgs*) arg;

    // Ordena processos por tempo de chegada
    qsort(args->processes, args->n, sizeof(PCB*), compare_arrival_time);

    for (int i = 0; i < args->n; i++) {
        PCB* pcb = args->processes[i];

        // Espera até o tempo de chegada
        while (current_millis() - args->start_time < pcb->start_time) {
            usleep(1000); // espera ativa leve (1ms)
        }

        // Cria as threads do processo (ficam bloqueadas no cond var)
        start_process_threads(pcb);

        // Insere na fila de prontos
        enqueue(args->ready_q, pcb);
    }

    return NULL;
}


// Função principal
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        perror("Erro ao abrir arquivo de entrada");
        return 1;
    }

    logger_init();

    int n;
    fscanf(f, "%d", &n); // número de processos

    PCB** processes = (PCB**) malloc(sizeof(PCB*) * n);

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

    // Inicializa fila de prontos
    Queue ready_q;
    queue_init(&ready_q);

    // Inicia gerador de processos
    GeneratorArgs gen_args;
    gen_args.processes = processes;
    gen_args.n = n;
    gen_args.ready_q = &ready_q;
    gen_args.start_time = current_millis();

    pthread_t generator_thread;
    pthread_create(&generator_thread, NULL, process_generator, &gen_args);

    // Inicializa escalonador
    Scheduler sched;
    scheduler_init(&sched, policy, &ready_q, n);

    // Cria thread do escalonador
    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler_run, &sched);

    // Aguarda o fim do gerador e do escalonador
    pthread_join(generator_thread, NULL);
    pthread_join(sched_thread, NULL);

    // Aguarda fim de todas as threads dos processos
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < processes[i]->num_threads; j++) {
            pthread_join(processes[i]->thread_ids[j], NULL);
        }
    }

    // Salva log em arquivo
    logger_save("log_execucao_minikernel.txt");
    logger_destroy();

    // Libera memória
    for (int i = 0; i < n; i++) {
        free(processes[i]->thread_ids);
        free(processes[i]);
    }
    free(processes);

    return 0;
}