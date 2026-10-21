# Mini-Kernel Scheduler 🧠⚙️

Simulador de escalonamento de processos em C, feito para a disciplina de Sistemas Operacionais (UFES). Cada processo é representado por um conjunto de threads POSIX (`pthread`), e um escalonador decide qual processo executa a cada momento, em versão **monoprocessador** e **multiprocessador (2 CPUs)**.

## O que ele faz

- Lê de um arquivo a lista de processos (duração, prioridade, número de threads e instante de chegada) e a política de escalonamento.
- Gera os processos no instante de chegada, mantém uma fila de prontos e escalona conforme a política escolhida.
- Registra cada decisão do escalonador em um log (`log_execucao_minikernel.txt`).

### Políticas de escalonamento

| Código | Política | Descrição |
|---|---|---|
| 1 | FCFS | Primeiro a chegar, primeiro a executar, sem preempção |
| 2 | Round Robin | Quantum fixo de 500 ms, com preempção |
| 3 | Prioridade | Preemptiva; **menor número = maior prioridade** |

### Conceitos usados

- PCB (bloco de controle de processo) e TCB (thread), com estados `READY`, `RUNNING` e `FINISHED`
- Sincronização com `pthread_mutex` e variáveis de condição (`pthread_cond`)
- Fila de prontos protegida por mutex, com inserção ordenada por prioridade
- Uma thread de escalonador por CPU na versão multiprocessador, com compartilhamento de processos multithread entre as CPUs

## Estrutura

```
process.c / process.h          PCB, TCB e threads dos processos
queue.c / queue.h              fila de prontos (FIFO e por prioridade)
logger.c / logger.h            log thread-safe dos eventos
schedulerMono.c / .h           escalonador monoprocessador
schedulerMulti.c / .h          escalonador multiprocessador (2 CPUs)
mainMono.c / mainMulti.c       leitura da entrada e gerador de processos
exemplos/                      arquivos de entrada de exemplo
```

## Como compilar e executar

Requer `gcc` e `make` (Linux).

```bash
# monoprocessador
make monoprocessador
./trabSO exemplos/rr.txt

# multiprocessador
make clean
make multiprocessador
./trabSO exemplos/rr.txt
```

O resultado fica em `log_execucao_minikernel.txt`.

## Formato do arquivo de entrada

```
<n>                               número de processos
<duração_ms> <prioridade> <threads> <chegada_ms>     (uma linha por processo)
...
<política>                        1 = FCFS, 2 = RR, 3 = Prioridade
```

Exemplo (`exemplos/rr.txt`): 3 processos, política Round Robin.

```
3
2000 1 2 0
1500 2 1 100
1000 3 1 200
2
```

## Limitações conhecidas

- Na versão multiprocessador, a ordem das mensagens no log pode variar entre execuções, por causa da concorrência entre as threads das CPUs.
- O limite é de 128 processos na fila de prontos e 100 processos no controle de finalização.

## Autores

- Daniel Olimpio Sbrocco
