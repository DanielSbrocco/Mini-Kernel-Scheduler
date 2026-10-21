#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>

// Inicializa o logger
void logger_init();

// Adiciona uma mensagem ao log
void log_event(const char* msg);

// Salva o log em arquivo
void logger_save(const char* filename);

// Libera recursos
void logger_destroy();

#endif
