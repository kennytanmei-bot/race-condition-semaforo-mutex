/*
 * CENARIO 1 (CORRIGIDO) - Contador compartilhado protegido por MUTEX
 *
 * Mesmo programa do codigo original, mas a secao critica (LOAD/ADD/STORE)
 * agora e protegida por um k_mutex. Somente uma thread por vez executa o
 * trecho; a outra bloqueia em k_mutex_lock() ate o mutex ser liberado.
 *
 * Por que MUTEX aqui: o recurso (variavel) tem "dono" durante o acesso -
 * a mesma thread que trava e quem destrava, e o mutex do Zephyr ainda tem
 * heranca de prioridade, evitando inversao de prioridade.
 *
 * A mesma deteccao do codigo original foi mantida (agora DENTRO do mutex):
 * se a correcao funciona, nenhuma ocorrencia deve ser detectada.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define N_ITER      20000U
#define STACK_SIZE  1024
#define PRIORIDADE  5
#define MAX_LOG     5U

struct evento {
	uint32_t iter;
	uint32_t lido;
	uint32_t atual;
};

static volatile uint32_t contador;

static struct evento log_ev[2][MAX_LOG];
static uint32_t n_ev[2];
static uint32_t perdidos[2];

static K_MUTEX_DEFINE(mtx_contador);

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
		k_mutex_lock(&mtx_contador, K_FOREVER);   /* entra na secao critica */

		uint32_t lido = contador;                 /* LOAD  */
		uint32_t tmp = lido;

		for (volatile int d = 0; d < 5; d++) {
			/* mesma espera do original: agora e inofensiva */
		}

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

		tmp = tmp + 1U;                           /* ADD   */
		contador = tmp;                           /* STORE */

		k_mutex_unlock(&mtx_contador);            /* sai da secao critica */
	}

	/* sem printk aqui: duas threads imprimindo juntas misturam a saida serial */
	k_sem_give(&fim);
}

int main(void)
{
	printk("\n=== Cenario 1 CORRIGIDO: contador protegido por MUTEX ===\n");

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
