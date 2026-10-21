#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "logger.h"

#define MAX_LOG 10000
#define MAX_MSG 256

static char log_buffer[MAX_LOG][MAX_MSG];
static int log_count = 0;
static pthread_mutex_t log_mutex;

// Inicializa o logger
void logger_init() {
    log_count = 0;
    pthread_mutex_init(&log_mutex, NULL);
}

// Adiciona mensagem no buffer
void log_event(const char* msg) {
    pthread_mutex_lock(&log_mutex);

    if (log_count < MAX_LOG) {
        strncpy(log_buffer[log_count], msg, MAX_MSG - 1);
        log_buffer[log_count][MAX_MSG - 1] = '\0'; // segurança
        log_count++;
    } else {
        fprintf(stderr, "Logger cheio, mensagem descartada: %s\n", msg);
    }

    pthread_mutex_unlock(&log_mutex);
}

// Salva log em arquivo
void logger_save(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("Erro ao abrir arquivo de log");
        return;
    }

    for (int i = 0; i < log_count; i++) {
        fprintf(f, "%s\n", log_buffer[i]);
    }

    fclose(f);
}

// Destroi logger
void logger_destroy() {
    pthread_mutex_destroy(&log_mutex);
}
