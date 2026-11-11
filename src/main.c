/*
 * CENARIO 2 (ORIGINAL) - Contador compartilhado com "lost update"
 *
 * Duas threads incrementam a mesma variavel global N_ITER vezes cada.
 * Valor esperado ao final: 2 * N_ITER.
 *
 * O incremento "contador++" NAO e atomico: ele e feito em 3 passos
 * (LOAD -> ADD -> STORE). Se o time slicing trocar de thread entre o LOAD e o
 * STORE, a outra thread escreve um valor que depois e sobrescrito com um valor
 * antigo -> incrementos sao perdidos (race condition).
 *
 * A pequena espera entre o LOAD e o STORE apenas AUMENTA a janela de tempo
 * em que a troca de contexto pode acontecer, tornando o erro reproduzivel.
 *
 * DETECCAO DO ERRO: logo antes do STORE, a thread le o contador de novo.
 * Se o valor mudou desde o LOAD, outra thread incrementou durante a janela e
 * este STORE vai sobrescrever o trabalho dela (lost update). A ocorrencia e
 * guardada em um log por thread (sem printk dentro do loop, para nao alterar
 * o timing) e impressa no final.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define N_ITER      1000U
#define STACK_SIZE  1024
#define PRIORIDADE  5
#define MAX_LOG     5U      /* quantas ocorrencias guardar por thread */

struct evento {
	uint32_t iter;      /* iteracao da thread em que ocorreu */
	uint32_t lido;      /* valor lido no LOAD */
	uint32_t atual;     /* valor do contador logo antes do STORE */
};

static volatile uint32_t contador;

static struct evento log_ev[2][MAX_LOG];
static uint32_t n_ev[2];        /* total de ocorrencias detectadas */
static uint32_t perdidos[2];    /* incrementos perdidos detectados */

static K_THREAD_STACK_DEFINE(stack_a, STACK_SIZE);
static K_THREAD_STACK_DEFINE(stack_b, STACK_SIZE);
static struct k_thread thread_a, thread_b;

static K_SEM_DEFINE(fim, 0, 2);

static void incrementa(void *id_ptr, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const int id = (int)(intptr_t)id_ptr;      /* 0 = A, 1 = B */

	for (uint32_t i = 0; i < N_ITER; i++) {
		/* ---- inicio da secao critica (SEM protecao) ---- */
		uint32_t lido = contador;           /* LOAD  */
		uint32_t tmp = lido;

		for (volatile int d = 0; d < 5; d++) {
			/* alarga a janela da race condition */
		}

		/* deteccao: o contador mudou entre o LOAD e o STORE? */
		uint32_t atual = contador;

		if (atual != lido) {
			if (n_ev[id] < MAX_LOG) {
				log_ev[id][n_ev[id]].iter = i;
				log_ev[id][n_ev[id]].lido = lido;
				log_ev[id][n_ev[id]].atual = atual;
			}
			n_ev[id]++;
			perdidos[id] += atual - lido;
		}

		tmp = tmp + 1U;                     /* ADD   */
		contador = tmp;                     /* STORE */
		/* ---- fim da secao critica ---- */
	}

	/* sem printk aqui: duas threads imprimindo juntas misturam a saida serial */
	k_sem_give(&fim);
}

int main(void)
{
	printk("\n=== Cenario 2 ORIGINAL: contador compartilhado (sem protecao) ===\n");

	k_thread_create(&thread_a, stack_a, K_THREAD_STACK_SIZEOF(stack_a),
			incrementa, (void *)(intptr_t)0, NULL, NULL, PRIORIDADE, 0, K_NO_WAIT);
	k_thread_create(&thread_b, stack_b, K_THREAD_STACK_SIZEOF(stack_b),
			incrementa, (void *)(intptr_t)1, NULL, NULL, PRIORIDADE, 0, K_NO_WAIT);

	k_sem_take(&fim, K_FOREVER);
	k_sem_take(&fim, K_FOREVER);

	printk("Threads A e B terminaram (%u iteracoes cada)\n", N_ITER);

	uint32_t esperado = 2U * N_ITER;
	uint32_t obtido = contador;

	printk("Esperado: %u | Obtido: %u | Incrementos perdidos: %u\n",
	       esperado, obtido, esperado - obtido);

	printk("\n--- Ocorrencias da race condition detectadas em tempo de execucao ---\n");
	printk("Thread A: %u ocorrencias | Thread B: %u ocorrencias\n", n_ev[0], n_ev[1]);

	for (int id = 0; id < 2; id++) {
		uint32_t n = (n_ev[id] < MAX_LOG) ? n_ev[id] : MAX_LOG;

		for (uint32_t k = 0; k < n; k++) {
			printk("[Thread %c] iteracao %u: leu %u, mas o contador ja era %u "
			       "antes do STORE -> %u incremento(s) da outra thread sobrescrito(s)\n",
			       'A' + id, log_ev[id][k].iter, log_ev[id][k].lido,
			       log_ev[id][k].atual, log_ev[id][k].atual - log_ev[id][k].lido);
		}
	}

	printk("Incrementos sobrescritos (soma das ocorrencias): A=%u | B=%u\n",
	       perdidos[0], perdidos[1]);
	printk("Resultado: %s\n", (obtido == esperado) ? "OK" : "RACE CONDITION DETECTADA");

	return 0;
}
