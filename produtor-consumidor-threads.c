#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h> 
#include <unistd.h>
#include <semaphore.h>
#include <string.h>

/* ============ PRIMEIRA PARTE: Relógios Vetoriais com MPI ============ */

#define N_PROC 3

typedef struct {
    int v[N_PROC];
} VectorClock;

void localEvent(int pid, VectorClock *clock) {
    clock->v[pid]++;
}

void sendMsg(int from, int to, VectorClock *clock) {
    clock->v[from]++;

    int msg[N_PROC];
    for (int i = 0; i < N_PROC; i++)
        msg[i] = clock->v[i];

    MPI_Send(msg, N_PROC, MPI_INT, to, 0, MPI_COMM_WORLD);

    printf("P%d -> P%d | Envio | Clock: (%d, %d, %d)\n",
           from, to, clock->v[0], clock->v[1], clock->v[2]);
}

void recvMsg(int from, int self, VectorClock *clock) {
    int recvVec[N_PROC];
    MPI_Recv(recvVec, N_PROC, MPI_INT, from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    clock->v[self]++;

    for (int i = 0; i < N_PROC; i++) {
        if (recvVec[i] > clock->v[i])
            clock->v[i] = recvVec[i];
    }

    printf("P%d <- P%d | Recebimento | Clock: (%d, %d, %d)\n",
           self, from, clock->v[0], clock->v[1], clock->v[2]);
}

void processo0() {
    VectorClock c = {{0, 0, 0}};
    printf("P0 inicial | Clock: (%d, %d, %d)\n", c.v[0], c.v[1], c.v[2]);

    localEvent(0, &c);
    printf("P0 evento interno | Clock: (%d, %d, %d)\n", c.v[0], c.v[1], c.v[2]);

    sendMsg(0, 1, &c);
    recvMsg(1, 0, &c);
    sendMsg(0, 2, &c);
    recvMsg(2, 0, &c);
    sendMsg(0, 1, &c);

    localEvent(0, &c);
    printf("P0 evento interno final | Clock: (%d, %d, %d)\n", c.v[0], c.v[1], c.v[2]);
}

void processo1() {
    VectorClock c = {{0, 0, 0}};
    printf("P1 inicial | Clock: (%d, %d, %d)\n", c.v[0], c.v[1], c.v[2]);

    sendMsg(1, 0, &c);
    recvMsg(0, 1, &c);
    recvMsg(0, 1, &c);
}

void processo2() {
    VectorClock c = {{0, 0, 0}};
    printf("P2 inicial | Clock: (%d, %d, %d)\n", c.v[0], c.v[1], c.v[2]);

    localEvent(2, &c);
    printf("P2 evento interno | Clock: (%d, %d, %d)\n", c.v[0], c.v[1], c.v[2]);

    sendMsg(2, 0, &c);
    recvMsg(0, 2, &c);
}

int main_mpi(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    switch (rank) {
        case 0: processo0(); break;
        case 1: processo1(); break;
        case 2: processo2(); break;
    }

    MPI_Finalize();
    return 0;
}

/* ============ SEGUNDA PARTE: Produtor/Consumidor com Threads ============ */

#define PROD_THREADS 3
#define CONS_THREADS 3
#define BUFFER_SIZE 5

typedef struct Task {
    VectorClock clock;
    int prod_id;
    int task_id;
    char event_type[20];
    int to_pid;
} Task;

Task taskQueue[BUFFER_SIZE];
int taskCount = 0;
int tasks_produced = 0;
int tasks_consumed = 0;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueueNotEmpty;
pthread_cond_t condQueueNotFull;

int scenario = 1;
int running = 1;

Task getTask(int consumer_id) {
    pthread_mutex_lock(&mutexQueue);
    
    while (taskCount == 0 && running) {
        printf("Consumidor %d: FILA VAZIA - esperando...\n", consumer_id);
        pthread_cond_wait(&condQueueNotEmpty, &mutexQueue);
    }
    
    if (!running && taskCount == 0) {
        pthread_mutex_unlock(&mutexQueue);
        Task emptyTask = {{{0, 0, 0}}, -1, -1, "", -1};
        return emptyTask;
    }
    
    Task task = taskQueue[0];
    for (int i = 0; i < taskCount - 1; i++) {
        taskQueue[i] = taskQueue[i+1];
    }
    taskCount--;
    tasks_consumed++;
    
    printf("Consumidor %d: removeu tarefa %d (Produtor %d). Fila: %d/%d\n",
           consumer_id, task.task_id, task.prod_id, taskCount, BUFFER_SIZE);
    
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueueNotFull);
    
    return task;
}

void submitTask(Task task, int producer_id) {
    pthread_mutex_lock(&mutexQueue);
    
    while (taskCount == BUFFER_SIZE && running) {
        printf("Produtor %d: FILA CHEIA (%d/%d) - esperando...\n",
               producer_id, taskCount, BUFFER_SIZE);
        pthread_cond_wait(&condQueueNotFull, &mutexQueue);
    }
    
    if (!running) {
        pthread_mutex_unlock(&mutexQueue);
        return;
    }
    
    taskQueue[taskCount] = task;
    taskCount++;
    tasks_produced++;
    
    printf("Produtor %d: adicionou tarefa %d. Fila: %d/%d\n",
           producer_id, task.task_id, taskCount, BUFFER_SIZE);
    
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueueNotEmpty);
}

void processTask(Task* task, int consumer_id) {
    printf("\n=== Consumidor %d processando ===\n", consumer_id);
    printf("Tarefa %d do Produtor P%d\n", task->task_id, task->prod_id);
    printf("Evento: %s", task->event_type);
    if (task->to_pid != -1) {
        printf(" para P%d", task->to_pid);
    }
    printf("\n");
    printf("Relógio Vetorial: (%d, %d, %d)\n",
           task->clock.v[0], task->clock.v[1], task->clock.v[2]);
    printf("=== Fim processamento ===\n\n");
}

void* producerThread(void* args) {
    int prod_id = (long)args;
    VectorClock clock = {{0, 0, 0}};
    int task_counter = 0;
    
    printf("Produtor P%d iniciado\n", prod_id);
    
    while (running && task_counter < 10) {
        Task task;
        task.prod_id = prod_id;
        task.task_id = task_counter++;
        
        switch (prod_id) {
            case 0:
                localEvent(0, &clock);
                strcpy(task.event_type, "Evento interno");
                task.to_pid = -1;
                break;
                
            case 1:
                if (task_counter % 2 == 0) {
                    sendMsg(1, 0, &clock);
                    strcpy(task.event_type, "Envio");
                    task.to_pid = 0;
                } else {
                    strcpy(task.event_type, "Evento interno");
                    task.to_pid = -1;
                    localEvent(1, &clock);
                }
                break;
                
            case 2:
                if (task_counter % 3 == 0) {
                    sendMsg(2, 0, &clock);
                    strcpy(task.event_type, "Envio");
                    task.to_pid = 0;
                } else {
                    strcpy(task.event_type, "Evento interno");
                    task.to_pid = -1;
                    localEvent(2, &clock);
                }
                break;
        }
        
        for (int i = 0; i < N_PROC; i++) {
            task.clock.v[i] = clock.v[i];
        }
        
        submitTask(task, prod_id);
        
        if (scenario == 1) {
            sleep(1);
        } else {
            sleep(2);
        }
    }
    
    printf("Produtor P%d terminou\n", prod_id);
    return NULL;
}

void* consumerThread(void* args) {
    int cons_id = (long)args;
    
    printf("Consumidor %d iniciado\n", cons_id);
    
    while (running) {
        Task task = getTask(cons_id);
        
        if (task.prod_id == -1) {
            break;
        }
        
        processTask(&task, cons_id);
        
        if (scenario == 1) {
            sleep(2);
        } else {
            sleep(1);
        }
    }
    
    printf("Consumidor %d terminou\n", cons_id);
    return NULL;
}

int main_pthread(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <cenario>\n", argv[0]);
        printf("  cenario 1: FILA CHEIA\n");
        printf("  cenario 2: FILA VAZIA\n");
        return 1;
    }
    
    scenario = atoi(argv[1]);
    if (scenario != 1 && scenario != 2) {
        printf("Cenário deve ser 1 ou 2\n");
        return 1;
    }
    
    printf("\n=== SISTEMA PRODUTOR/CONSUMIDOR ===\n");
    printf("Cenário %d: %s\n", scenario,
           scenario == 1 ? "FILA CHEIA" : "FILA VAZIA");
    printf("Buffer: %d | Produtores: %d | Consumidores: %d\n\n", 
           BUFFER_SIZE, PROD_THREADS, CONS_THREADS);
    
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueueNotEmpty, NULL);
    pthread_cond_init(&condQueueNotFull, NULL);
    
    pthread_t producers[PROD_THREADS];
    pthread_t consumers[CONS_THREADS];
    
    srand(time(NULL));
    
    for (long i = 0; i < PROD_THREADS; i++) {
        if (pthread_create(&producers[i], NULL, &producerThread, (void*)i) != 0) {
            perror("Falha ao criar thread produtora");
            return 1;
        }
    }
    
    for (long i = 0; i < CONS_THREADS; i++) {
        if (pthread_create(&consumers[i], NULL, &consumerThread, (void*)i) != 0) {
            perror("Falha ao criar thread consumidora");
            return 1;
        }
    }
    
    sleep(15);
    
    running = 0;
    printf("\n=== FINALIZANDO... ===\n");
    
    pthread_cond_broadcast(&condQueueNotEmpty);
    pthread_cond_broadcast(&condQueueNotFull);
    
    for (int i = 0; i < PROD_THREADS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    pthread_cond_broadcast(&condQueueNotEmpty);
    
    for (int i = 0; i < CONS_THREADS; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    printf("\n=== ESTATÍSTICAS ===\n");
    printf("Tarefas produzidas: %d\n", tasks_produced);
    printf("Tarefas consumidas: %d\n", tasks_consumed);
    printf("Tarefas na fila: %d\n", taskCount);
    
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueueNotEmpty);
    pthread_cond_destroy(&condQueueNotFull);
    
    return 0;
}

/* ============ FUNÇÃO MAIN UNIFICADA ============ */

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("ESCOLHA O MODO DE EXECUÇÃO:\n");
    printf("1. MPI (Relógios Vetoriais)\n");
    printf("2. Produtor/Consumidor com Threads\n");
    printf("========================================\n");
    
    if (argc < 2) {
        printf("Uso:\n");
        printf("  Para MPI: mpirun -np 3 %s mpi\n", argv[0]);
        printf("  Para Threads: %s <cenario>\n", argv[0]);
        printf("  cenario = 1 (fila cheia) ou 2 (fila vazia)\n");
        return 1;
    }
    
    if (strcmp(argv[1], "mpi") == 0) {
        printf("\n=== EXECUTANDO MPI ===\n");
        return main_mpi(argc, argv);
    } else {
        printf("\n=== EXECUTANDO THREADS ===\n");
        char* new_argv[2];
        new_argv[0] = argv[0];
        new_argv[1] = (argc > 2) ? argv[2] : "1";
        return main_pthread(2, new_argv);
    }
}
