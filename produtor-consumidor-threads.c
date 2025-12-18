/*
 * Produtor/Consumidor com relógios vetoriais globais
 * Baseado em pth_pool.c
 *
 * Dois cenários:
 * 1 -> fila cheia
 * 2 -> fila vazia
 *
 * Para compilar:
 *   gcc -Wall -pthread produtor_consumidor_vetorial.c -o produtor_consumidor_vetorial
 *
 * Para executar o cenário 1:
 *   ./produtor_consumidor_vetorial 1
 *
 * Para executar o cenário 2:
 *   ./produtor_consumidor_vetorial 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define N_PROC        3
#define BUFFER_SIZE   5
#define TOTAL_CLOCKS 10

typedef struct {
    int pid;
    int v[N_PROC];
} VectorClock;

/* ---------- Fila ---------- */
VectorClock buffer[BUFFER_SIZE];
int count = 0;

/* ---------- Relógio vetorial global ---------- */
VectorClock globalClock = { -1, {0, 0, 0} };

pthread_mutex_t mutex;
pthread_cond_t condEmpty;
pthread_cond_t condFull;

int prod_delay;  // segundos
int cons_delay;  // segundos

/* ---------- Funções da fila ---------- */
void putClock(VectorClock c, int pid) {
    pthread_mutex_lock(&mutex);

    while (count == BUFFER_SIZE) {
        printf(">>> FILA CHEIA: Produtor P%d aguardando...\n", pid);
        pthread_cond_wait(&condFull, &mutex);
    }

    buffer[count++] = c;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condEmpty);
}

VectorClock getClock(int cid) {
    pthread_mutex_lock(&mutex);

    while (count == 0) {
        printf(">>> FILA VAZIA: Consumidor %d aguardando...\n", cid);
        pthread_cond_wait(&condEmpty, &mutex);
    }

    VectorClock c = buffer[0];

    for (int i = 0; i < count - 1; i++) {
        buffer[i] = buffer[i + 1];
    }

    count--;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condFull);

    return c;
}

/* ---------- Produtor ---------- */
void* produtor(void* arg) {
    int pid = (long)arg;
    VectorClock c = { pid, {0, 0, 0} };

    for (int i = 0; i < TOTAL_CLOCKS; i++) {
        /* Evento interno do produtor */
        c.v[pid]++;

        printf("[PRODUTOR P%d] Produziu clock (%d,%d,%d)\n",
               pid, c.v[0], c.v[1], c.v[2]);

        putClock(c, pid);
        sleep(prod_delay);
    }

    return NULL;
}

/* ---------- Consumidor ---------- */
void* consumidor(void* arg) {
    int cid = (long)arg;

    for (int i = 0; i < TOTAL_CLOCKS; i++) {
        VectorClock c = getClock(cid);

        /* Atualiza relógio global */
        pthread_mutex_lock(&mutex);
        for (int j = 0; j < N_PROC; j++) {
            if (c.v[j] > globalClock.v[j]) {
                globalClock.v[j] = c.v[j];
            }
        }
        pthread_mutex_unlock(&mutex);

        printf(" [CONSUMIDOR %d] Consumiu de P%d -> (%d,%d,%d) | "
               "Global (%d,%d,%d)\n",
               cid, c.pid,
               c.v[0], c.v[1], c.v[2],
               globalClock.v[0], globalClock.v[1], globalClock.v[2]);

        sleep(cons_delay);
    }

    return NULL;
}

/* ---------- Main ---------- */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <1|2>\n", argv[0]);
        printf("1 -> Fila cheia\n");
        printf("2 -> Fila vazia\n");
        return 1;
    }

    int scenario = atoi(argv[1]);

    if (scenario == 1) {
        prod_delay = 1;
        cons_delay = 2;
        printf("=== CENÁRIO: FILA CHEIA ===\n");
    } else {
        prod_delay = 2;
        cons_delay = 1;
        printf("=== CENÁRIO: FILA VAZIA ===\n");
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&condEmpty, NULL);
    pthread_cond_init(&condFull, NULL);

    pthread_t prod[N_PROC];
    pthread_t cons[N_PROC];

    for (long i = 0; i < N_PROC; i++) {
        pthread_create(&prod[i], NULL, produtor, (void*)i);
        pthread_create(&cons[i], NULL, consumidor, (void*)i);
    }

    for (int i = 0; i < N_PROC; i++) {
        pthread_join(prod[i], NULL);
        pthread_join(cons[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condEmpty);
    pthread_cond_destroy(&condFull);

    return 0;
}
