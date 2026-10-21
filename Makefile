CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = trabSO

# Objetos comuns
COMMON_OBJS = process.o queue.o logger.o

# Objetos específicos para cada versão
MONO_OBJS = $(COMMON_OBJS) mainMono.o schedulerMono.o
MULTI_OBJS = $(COMMON_OBJS) mainMulti.o schedulerMulti.o

# Target para monoprocessador
monoprocessador: $(MONO_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(MONO_OBJS)

# Target para multiprocessador
multiprocessador: $(MULTI_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(MULTI_OBJS)

# Compilação dos objetos comuns
process.o: process.c process.h logger.h
	$(CC) $(CFLAGS) -c process.c

queue.o: queue.c queue.h process.h
	$(CC) $(CFLAGS) -c queue.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) -c logger.c

# Compilação dos objetos específicos do monoprocessador
main.o: mainMono.c process.h schedulerMono.h queue.h logger.h
	$(CC) $(CFLAGS) -c mainMono.c

scheduler.o: schedulerMono.c schedulerMono.h logger.h process.h queue.h
	$(CC) $(CFLAGS) -c schedulerMono.c

# Compilação dos objetos específicos do multiprocessador
mainMulti.o: mainMulti.c process.h schedulerMulti.h queue.h logger.h
	$(CC) $(CFLAGS) -c mainMulti.c

schedulerMulti.o: schedulerMulti.c schedulerMulti.h logger.h process.h queue.h
	$(CC) $(CFLAGS) -c schedulerMulti.c

clean:
	rm -f *.o $(TARGET)

.PHONY: monoprocessador multiprocessador clean