#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>




#define USE_MUTEX   1     /* 0 = com race condition | 1 = corrigido com mutex */
#define ITERACOES   10
#define STACK_SIZE  1024




/* LEDs da FRDM-KL25Z (ajuste os aliases conforme o teste de cores) */
static const struct gpio_dt_spec led_verde    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_azul     = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);




static int contador = 0;                 /* recurso compartilhado */
static atomic_t total_races = ATOMIC_INIT(0);




K_MUTEX_DEFINE(meu_mutex);
K_SEM_DEFINE(fim_sem, 0, 2);




void tarefa(void *p1, void *p2, void *p3)
{
    const char *nome = (const char *)p1;   /* "A" ou "B" */




    for (int i = 1; i <= ITERACOES; i++) {




#if USE_MUTEX
        k_mutex_lock(&meu_mutex, K_FOREVER);
        gpio_pin_set_dt(&led_vermelho, 1);     /* vermelho = ocupado */
#endif




        /* ---- região crítica: ler -> esperar -> escrever ---- */
        int lido = contador;
        k_msleep(100);




        int atual = contador;                  /* valor no momento de gravar */
        contador = lido + 1;




        if (atual != lido) {
            atomic_inc(&total_races);
            printk("| %2d |  %s  | leu %2d | gravou %2d | RACE: outra thread ja tinha gravado %2d (soma perdida) |\n",
                   i, nome, lido, contador, atual);
        } else {
            printk("| %2d |  %s  | leu %2d | gravou %2d | OK |\n",
                   i, nome, lido, contador);
        }
        /* ---------------------------------------------------- */




#if USE_MUTEX
        gpio_pin_set_dt(&led_vermelho, 0);
        k_mutex_unlock(&meu_mutex);
#endif




        k_msleep(50);
    }
    k_sem_give(&fim_sem);
}




K_THREAD_DEFINE(thread_a, STACK_SIZE, tarefa, "A", NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(thread_b, STACK_SIZE, tarefa, "B", NULL, NULL, 5, 0, 0);




int main(void)
{
    gpio_pin_configure_dt(&led_verde,    GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_azul,     GPIO_OUTPUT_INACTIVE);




    printk("\n=== Modo: %s ===\n", USE_MUTEX ? "COM MUTEX" : "SEM PROTECAO");
    printk("| It | Thr | Leitura | Gravacao  | Situacao |\n");




    k_sem_take(&fim_sem, K_FOREVER);
    k_sem_take(&fim_sem, K_FOREVER);




    printk("\n=== RESUMO ===\n");
    printk("Esperado: %d | Obtido: %d\n", 2 * ITERACOES, contador);
    printk("Iteracoes com race condition: %d\n", (int)atomic_get(&total_races));




    if (contador == 2 * ITERACOES) {
        printk("Resultado: CORRETO\n");
        gpio_pin_set_dt(&led_verde, 1);
    } else {
        printk("Resultado: RACE CONDITION DETECTADA\n");
        gpio_pin_set_dt(&led_azul, 1);
    }
    return 0;
}