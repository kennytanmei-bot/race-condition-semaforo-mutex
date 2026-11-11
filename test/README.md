# PSI-Microcontroladores2-Aula06

Atividade: Resolução de Race Condition com Semáforo ou Mutex

Placa: **FRDM-KL25Z** · PlatformIO + **Zephyr 4.2** · VSCode

## Integrantes

| Integrante | Nome | Cenário |
|---|---|---|
| 1 | _(preencher)_ | Contador compartilhado (mutex) |
| 2 | _(colega preenche)_ | _(cenário do colega)_ |

## Estrutura do repositório

```
integrante1/
  codigo_original/    # race condition (contador compartilhado)
  codigo_corrigido/   # corrigido com mutex
integrante2/          # (colega adiciona aqui)
  codigo_original/
  codigo_corrigido/
evidencias/           # prints, logs e vídeos curtos
```

Cada pasta é um projeto PlatformIO independente (`pio run -t upload` e `pio device monitor`, 115200 baud).

## Cenário do Integrante 1: Contador compartilhado

Duas threads incrementam a mesma variável global 20.000 vezes cada. Valor esperado ao final: **40.000**.

### Race condition
O incremento é feito em três passos (LOAD → ADD → STORE). Com o time slicing do Zephyr (1 ms), uma thread pode ser interrompida entre o LOAD e o STORE; a outra incrementa o contador e, quando a primeira retoma, ela grava um valor antigo + 1, **perdendo incrementos** (lost update). Uma pequena espera entre o LOAD e o STORE alarga a janela do erro para torná-lo reproduzível.

### Solução
`k_mutex` envolvendo todo o LOAD/ADD/STORE. Escolha do mutex: o recurso tem "dono" durante o acesso (quem trava é quem destrava) e o mutex do Zephyr possui herança de prioridade.

## Casos de teste

> TODO (dupla, **sem IA generativa**): descrever 3 casos de teste para este cenário.

| Caso de Teste | Pré-condição | Etapas de Teste | Pós-condição Esperada |
|---|---|---|---|
| 1 | ... | ... | ... |
| 2 | ... | ... | ... |
| 3 | ... | ... | ... |

## Comportamento incorreto observado (código original do colega)

> TODO: registrar o que foi observado ao executar o código do colega e o momento em que o erro ocorre.

## Resultado após a correção

> TODO: reexecutar os mesmos casos de teste no código corrigido e registrar com evidências.

## Avaliação cruzada (Integrante 1 avalia o código do Integrante 2)

> TODO (**sem IA generativa**)

- O que estava errado antes:
- O que mudou com a correção:
- O comportamento agora é estável?

## Evidências

> TODO: prints, logs ou vídeos curtos em `evidencias/`.
