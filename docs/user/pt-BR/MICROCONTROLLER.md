<!-- source: MICROCONTROLLER.md sha256 475a36e948dab9702b279673c9e94e6f0ceabb5019df9a3719510e70a6744350 -->
# Microcontrolador: o que é simulado

O SimNodus executa o firmware da **Blue Pill (STM32F103C8)** em seu próprio emulador
de STM32, junto com o circuito. Esta página lista o que o chip simulado suporta e
onde ele difere de uma placa real (MCU-09); o [guia do usuário](USER_GUIDE.md)
cobre o restante do SimNodus.

## Motor

O firmware roda no emulador de STM32 do próprio SimNodus (ADR 0089, 0090, 0094): ele
inicia imediatamente e dá a cada mudança de pino o seu instante exato. Mudanças no
circuito chegam ao firmware em até 0,1 ms (uma janela de acoplamento de 100 µs), e
duas execuções com o mesmo circuito dão a mesma temporização do firmware.

## Suportado

| Parte | Situação | Observações |
|---|---|---|
| Núcleo Cortex-M3, 64 KiB de flash, 20 KiB de SRAM | sim | Arquivos ELF gerados para o STM32F103C8 (STM32CubeIDE, arm-none-eabi-gcc) |
| Clock (RCC) | sim | Parte no HSI de 8 MHz. HSE (o cristal de 8 MHz da placa), PLL, LSE e LSI indicam pronto assim que são ligados, e o SYSCLK segue o SW, de modo que o `SystemClock_Config()` gerado pelo STM32CubeIDE funciona (72 MHz com PLL x9). As instruções levam os ciclos do Cortex-M3 em HCLK (veja *Temporização das instruções* abaixo) e o SysTick conta em HCLK; o RESET volta a 8 MHz. O exemplo 12 usa a HAL a 72 MHz |
| Controle de acesso à flash, PWR | só registradores | `FLASH_ACR` (wait states) e `PWR_CR` guardam o que é escrito, para que a HAL possa conferi-los; a gravação da flash e os modos de baixo consumo não são simulados |
| SysTick | sim | Conta em HCLK; período de RVR + 1 ciclos, como no chip |
| GPIO portas A, B, C | sim | Modos por CRL/CRH, ODR/BSRR/BRR/IDR; push-pull, dreno aberto, entradas com pull-up/pull-down |
| Saídas GPIO para o circuito | sim | 3,3 V ou 0 V atrás de 40 Ω; o dreno aberto só puxa para nível baixo |
| Entradas GPIO a partir do circuito | sim | Schmitt trigger: 1 acima de 1,83 V, 0 abaixo de 1,16 V; entradas flutuantes leem 0 |
| EXTI 0–15 e NVIC | sim | As linhas chegam às IRQs 6–10, 23 (5–9) e 40 (10–15), como no chip |
| USART1–3 | sim | Texto na aba UART do Analisador de sinais. Cada byte sai no pino TX bit a bit na taxa configurada (start, 8 ou 9 bits de dados com paridade, 0,5–2 stop bits); TXE, TC e o tempo do byte seguem o quadro, então a saída serial demora tanto quanto na placa. O TX de uma placa ligado ao RX de outra repassa os bytes |
| DMA1 | sim | Sete canais: endereços de periférico e de memória, incrementos, tamanhos de 8/16/32 bits, modo circular, memória para memória, flags e interrupções de meia transferência, transferência completa e erro de transferência. Requisições do ADC1 (canal 1) e de TX/RX das USART1–3, de modo que `HAL_ADC_Start_DMA`, `HAL_UART_Transmit_DMA` e `HAL_UART_Receive_DMA` funcionam. Uma transferência não leva tempo. Exemplo 15 |
| ADC1 e ADC2 | sim | 12 bits sobre 0–3,3 V em IN0–IN9 (PA0–PA7, PB0, PB1), o sensor de temperatura (1,43 V, como a 25 °C) e o VREFINT (1,20 V); conversão única, contínua e por varredura (scan), canais injetados com offsets, tempos de amostragem por canal e o tempo de conversão a partir de PCLK2 e ADCPRE, calibração, alinhamento à esquerda/direita, watchdog analógico, interrupção de fim de conversão. A tensão do pino é lida do circuito 0,1–0,2 ms antes do instante de amostragem (veja abaixo). Exemplo 14 |
| Timers TIM1–TIM4 | sim | Contagem crescente e decrescente com prescaler e auto-reload (com preload), modo de um pulso (one-pulse), contador de repetição do TIM1; output compare (toggle, ativo/inativo na igualdade, níveis forçados), modos PWM 1 e 2 com preload do CCR e polaridade, habilitação da saída principal do TIM1 e saídas complementares; input capture de bordas de subida ou de descida com o prescaler de captura; interrupções de update e de capture/compare; pinos dos canais com os remapeamentos do AFIO. As bordas são exatas no clock do timer. Exemplo 13 |
| SPI1, SPI2 | sim | Modo mestre: 8 ou 16 bits, MSB ou LSB primeiro, os quatro modos de clock, PCLK / 2..256 com cada borda de SCK no instante exato; MOSI e MISO nos pinos, de modo que os chips nas redes da placa respondem dentro da transferência; quadros consecutivos, TXE, RXNE, BSY, OVR, interrupções, DMA (SPI1 nos canais 2/3, SPI2 nos 4/5), saída NSS (SSOE), remapeamento do SPI1. `HAL_SPI_Transmit` funciona sem alterações. Exemplo 26 |
| I2C1, I2C2 | sim | Modo mestre, endereços de 7 bits, modo padrão (100 kHz) e rápido com os dois duty cycles; SCL e SDA são dreno aberto e precisam de resistores de pull-up, como numa placa; o clock espera enquanto um escravo mantém SCL em nível baixo, e fica em nível baixo sem pull-up (uma mensagem avisa); START, START repetido, STOP, acknowledge e AF, TXE/RXNE/BTF com ACK e POS do jeito que `HAL_I2C_Master_Transmit/Receive`, `HAL_I2C_Mem_Write/Read` e `HAL_I2C_IsDeviceReady` os usam; interrupções, DMA (I2C1 nos canais 6/7, I2C2 nos 4/5), remapeamento do I2C1. Exemplo 27 |
| CRC | sim | CRC-32 (polinômio 0x04C11DB7) sobre palavras de 32 bits, como `HAL_CRC_Calculate` o usa; IDR; RESET |
| Watchdog independente (IWDG) | sim | Registrador de chave, prescaler e reload; o chip reinicia quando não é recarregado a tempo (RCC_CSR IWDGRSTF, uma mensagem no painel Mensagens). Conta em LSI = 40 kHz exatos |
| Watchdog de janela (WWDG) | sim | Contador de 7 bits em PCLK1 / 4096 / 2^WDGTB; reset quando T6 zera, quando é recarregado acima da janela ou quando T6 é escrito como 0; interrupção de early wakeup em 0x40 |
| RTC e registradores de backup | sim | Prescaler e contador de 32 bits em LSE (32,768 kHz), LSI (40 kHz) ou HSE / 128; flags e interrupções de alarme, de segundo e de overflow, o alarme na linha 17 do EXTI; BKP_DR1–DR10. O domínio de backup mantém a hora e os registradores através de resets, é apagado ao ligar a alimentação e pelo BDRST, e exige PWR_CR.DBP para escritas, como no chip. `HAL_RTC_*` e `HAL_RTCEx_BKUP*` funcionam sem alterações. Exemplo 24 |
| LED da placa (PC13, aceso em nível baixo) e botão RESET | sim | Clicáveis durante a simulação |

## Chips para microcontroladores

Os componentes da categoria *Chips para microcontroladores* (Registrador de
deslocamento 74HC595, Driver de display de LED MAX7219, EEPROM 24C02, Sensor de
temperatura LM75, Expansor de E/S PCF8574, BMP280, MPU6050, DS3231, TCS34725,
MCP3208, MCP41010, W25Q32, cartão microSD, HX711) e
o LCD de caracteres e o OLED SSD1306 (em *Displays*) funcionam em conjunto com
uma placa (ADR 0102): a lógica deles roda
em sincronia com o firmware, de modo que as redes entre a placa e o chip
acompanham cada borda imediatamente, e as saídas do chip acionam o circuito como
os pinos da placa (LEDs acendem, o osciloscópio e a aba Lógico os mostram, como
faixas `U2.Q0`).

- Ligue o pino de alimentação do chip (VCC) a uma fonte CC ou ao 3V3 da placa e
  o GND ao terra; sem eles o chip não faz nada e o painel Mensagens avisa.
- Um chip que compartilha uma rede com um pino da placa, diretamente ou através
  de outro chip, roda com essa placa. Um chip sem ligação a pino de placa roda
  sozinho (uma ROM endereçada por um contador, um 74HC595 com clock dado por um
  botão): suas entradas vêm do circuito com até 0,1 ms de atraso, então clocks de
  até alguns kHz funcionam.
- Entradas ligadas ao terra ou à alimentação são lidas como constantes; uma
  entrada aberta lê nível baixo (uma mensagem a identifica). Um pino de chip numa
  rede sem pino de placa (um botão ou um LED num pino do PCF8574) é lido do
  circuito, como uma entrada da placa, sempre que nenhuma saída aciona a rede.
- Os chips I2C respondem no seu endereço, com A0-A2 como bits menos
  significativos; a 24C02 grava uma página escrita no STOP e depois não dá
  acknowledge durante seu ciclo de escrita de 5 ms (consulte-a com
  `HAL_I2C_IsDeviceReady`); sua propriedade *Arquivo de conteúdo* (`.bin` ou Intel
  `.hex`) define o que ela contém no início de cada simulação, caso contrário ela
  começa apagada (0xFF); o LM75 informa a temperatura definida em suas
  propriedades, que pode ser alterada durante a simulação; os pinos do PCF8574 são
  saídas e entradas ao mesmo tempo (um 1 é um pull-up fraco).
- Os sensores, o relógio e o display I2C (SN-223) respondem em endereços
  fixos: BMP280 em 0x76 (0x77 com SDO em alto), com suas palavras de
  calibração e os modos forçado e normal, para que as bibliotecas usuais
  calculem a pressão e a temperatura definidas em suas propriedades; MPU6050
  em 0x68 (0x69 com AD0 em alto), dormindo até que PWR_MGMT_1 seja zerado,
  com acelerações e velocidades de rotação escaladas pelas faixas
  configuradas; DS3231 em 0x68, um relógio que anda com a simulação a partir
  da sua *Hora inicial* (`now` ou uma data), dois alarmes e a saída SQW de
  onda quadrada ou de alarme; TCS34725 em 0x29, contagens a partir da cor e
  da luz em suas propriedades, do ganho e do tempo de integração; OLED SSD1306
  em 0x3C ou 0x3D (uma propriedade), cuja imagem aparece no esquemático
  durante a simulação. Os valores medidos são propriedades que podem mudar
  durante a simulação. O exemplo 38 mostra o OLED com um BMP280 e um DS3231.
- Os chips SPI (SN-224) funcionam nos modos 0 e 3 com seu chip select: o
  MCP3208 converte a tensão do circuito em CH0-CH7 em relação a VREF (12 bits,
  simples ou diferencial); o MCP41010 define uma resistência real entre PA0,
  PW0 e PB0 (10, 50 ou 100 kΩ, 256 passos); o W25Q32 é uma flash de 4 MB com os
  comandos e tempos de ocupado da folha de dados (seu conteúdo no Visualizador
  de memória, atualizado no máximo cinco vezes por segundo); o cartão microSD
  responde aos comandos do modo SPI que os drivers do FatFs usam, com tamanho
  de 1 a 64 MB e um arquivo de imagem opcional. O amplificador de célula de
  carga HX711 lê as tensões da ponte pelo seu próprio protocolo de dois fios
  (10 ou 80 conversões por segundo). O exemplo 39 usa todos, menos o cartão.
- A ROM e a RAM paralelas (*Memórias*) têm A0-A15, D0-D7 e CE, OE e (na RAM) WE
  ativos em nível baixo; o tamanho, de 16 bytes a 64 KB, é uma propriedade, e os
  pinos de endereço além dele são ignorados. Com CE e OE em nível baixo elas
  acionam D0-D7; a RAM grava D0-D7 quando WE sobe com CE em nível baixo. O
  conteúdo da ROM vem do seu *Arquivo de conteúdo* (`.bin` ou Intel `.hex`, senão
  0xFF); a RAM começa com zeros ou com o seu arquivo. A temporização é ideal (sem
  tempo de acesso).
- O LCD de caracteres (HD44780) recebe RS, RW, E e D0-D7 ou D4-D7 dos pinos da
  placa ou de um PCF8574 (como nos módulos adaptadores I2C); as linhas e colunas
  são propriedades, e o texto aparece no esquemático durante a simulação. As
  instruções levam 37 µs (clear e home, 1,52 ms); um byte escrito enquanto o LCD
  está ocupado é ignorado, como no chip, com uma mensagem. Caracteres
  personalizados (CGRAM) aparecem como blocos.
- Numa rede, um nível baixo acionado prevalece, depois um nível alto acionado,
  depois um pull-up ou pull-down: o interno do pino da placa ou um resistor para
  uma alimentação ou para o terra. Duas saídas acionando níveis opostos são
  indicadas como curto-circuito.
- Os níveis são digitais: um chip de 5 V aceita os níveis altos de 3,3 V da placa,
  o que um chip real pode não aceitar.

### Visualizador de memória

**Janela > Visualizador de memória** (Ctrl+Shift+M), ou **Ver...** ao lado do
*Arquivo de conteúdo* de um chip de memória (24C02, ROM, RAM), mostra os bytes do
chip em hexadecimal e como texto. Durante a simulação a tabela acompanha o chip e
destaca os bytes que acabaram de mudar; depois de uma simulação ela mantém o
último conteúdo. **Carregar...** coloca um arquivo `.bin` ou Intel `.hex` no chip
em execução ou, quando nada está rodando, torna-o o arquivo de conteúdo do chip.
**Salvar como...** grava o que o chip contém num arquivo `.bin` ou `.hex`, por
exemplo para iniciar a próxima simulação com o que o firmware gravou.

O exemplo 32 usa uma ROM sem placa como tabela de consulta de um display de 7
segmentos; no exemplo 33 uma Blue Pill escreve e lê uma RAM paralela.

O exemplo 25 desloca um byte bit a bit para um 74HC595 que aciona oito LEDs.
O exemplo 27 usa os três chips I2C com a HAL; o exemplo 28 aciona um LCD 16x2
através de um módulo adaptador PCF8574. O exemplo 26 envia palavras ao MAX7219 pela SPI1 com a HAL; o chip faz a varredura de uma
matriz de LEDs 8x8 (um display de 7 segmentos de 4 dígitos funciona do mesmo
jeito). LEDs multiplexados brilham com a corrente média em 20 ms, como o olho os
vê. Com 64 LEDs o circuito é pesado: o exemplo 26 roda a cerca de 0,8x do tempo
real num notebook de 2026.

## Ainda não simulado

| Parte | Previsão |
|---|---|
| Recursos do ADC: conversões disparadas por timer ou EXTI, modos descontínuo e duplo; requisições de DMA de timers, SPI e I2C | Mais tarde; o firmware recebe uma mensagem no painel Mensagens quando usa um deles |
| Recursos dos timers: contagem center-aligned, encoder e outros modos escravos, filtros de entrada, dead time, entrada de break, requisições de DMA dos timers | Mais tarde; o firmware recebe uma mensagem no painel Mensagens quando usa um deles |
| SPI escravo, modos só recepção e bidirecional, CRC, I2S | Ainda não; uma mensagem avisa |
| I2C modo escravo, endereços de 10 bits, SMBus, PEC, bit LAST do DMA | Ainda não; uma mensagem avisa |
| CAN, USB, pino de tamper do RTC e saída de calibração | Ainda sem previsão |
| Modo transitório com microcontrolador | Ainda não: use o modo interativo |

## Diferenças em relação à placa real

- **Temporização entre firmware e circuito.** O firmware roda no máximo uma
  janela de acoplamento (100 µs) à frente do circuito. As mudanças nas saídas
  chegam ao circuito no instante exato do firmware. As mudanças no circuito chegam
  ao firmware com menos de 0,1 ms de atraso, em ordem e com o seu espaçamento. A
  dica (tooltip) da barra de status descreve essas aproximações.
- **Clock.** Os osciladores e a PLL ficam prontos imediatamente (um HSE real leva
  cerca de 1–2 ms). As habilitações de clock dos periféricos
  (`RCC_APB2ENR` …) não são verificadas: um periférico funciona mesmo que seu clock
  nunca tenha sido ligado, o que falharia na placa. O clock security system e o
  MCO não são simulados.
- **Temporização das instruções.** Com *Tempo de instrução do Cortex-M3* ligado
  (Simular > Configurações da simulação, ligado por padrão), cada instrução leva os
  ciclos do núcleo do STM32F103: 2 para um load, de 2 a 12 para uma divisão, 1 + o
  número de registradores para LDM/STM e PUSH/POP, um reabastecimento do pipeline
  após um desvio tomado, 12 ciclos para entrar numa interrupção e 10 para retornar,
  e os wait states da flash definidos em `FLASH_ACR` (dois a 72 MHz) através do
  buffer de prefetch. É uma aproximação calibrada numa Blue Pill real com o
  `CYCCNT` do DWT: as 30 medições de `examples/firmware/cycle_loops` (15 testes a 8
  e a 72 MHz) concordam dentro de 5 %, exceto oito multiplicações seguidas a 72 MHz
  (7 % mais rápido). Laços de espera ativa e cálculos, portanto, levam
  aproximadamente o mesmo tempo que na placa. Desligado: uma instrução por ciclo,
  mais rápido que o chip (como nas versões até a 0.8). Código temporizado com
  SysTick, timers ou `HAL_Delay()` fica correto de qualquer forma. O DMA e a CPU
  não atrasam um ao outro.
- **Velocidade.** A 72 MHz o emulador executa nove vezes mais instruções por
  segundo simulado do que a 8 MHz; a barra de status mostra a velocidade real.
- **Temporização do ADC.** O firmware roda até 0,1 ms à frente do circuito, então
  uma conversão lê a tensão que o pino tinha no circuito 0,1–0,2 ms antes do
  instante de amostragem (ADR 0092). Sinais lentos (potenciômetros, sensores, PWM
  filtrado) dão o código da sua tensão; uma forma de onda rápida mostra esse atraso.
  Duas execuções com o mesmo circuito dão os mesmos códigos.
- **Bordas no circuito.** O emulador temporiza cada borda com exatidão. O circuito
  usa passos de tempo de até 0,1 ms (Configurações da simulação, passo de tempo
  máximo) e recebe, a cada passo, a saída do pino com a média sobre esse passo
  (ADR 0091). Assim, a média de um PWM é exata em qualquer frequência (exemplo 13:
  0,825 V a partir de 25 % de 3,3 V), mas uma borda se espalha pelo passo que a
  contém: o osciloscópio mostra um ponto entre os níveis, e sinais mais rápidos que
  o passo (os bits de um quadro serial) desaparecem na média ali. As faixas lógicas
  mostram as bordas exatas. **Simular > Configurações da simulação > Bordas exatas
  do microcontrolador** dá ao circuito, em vez disso, um ponto de tempo em cada
  borda do firmware (ADR 0093): ondas quadradas continuam quadradas com o passo de
  100 µs e as respostas RC batem com as fórmulas, mas as simulações ficam mais
  lentas (lição 1: 1,2x em vez de 2,9x do tempo real num notebook de 2026; um passo
  de 10 µs dá as mesmas formas de onda a cerca de 0,25x).
- **UART.** As faixas lógicas mostram os bits exatos no pino TX. No osciloscópio, um
  bit a 115200 baud (8,7 µs) é mais curto que o passo de 0,1 ms do circuito: ligue
  *Bordas exatas do microcontrolador* com um passo de 10 µs para ver o quadro. O
  pino RX é decodificado a partir do circuito: qualquer fonte no RX, por
  exemplo o TX de outra placa através de um resistor ou de portas lógicas, envia
  quadros que a USART lê no centro dos bits, com as flags de erro de
  enquadramento (FE) e de paridade (PE). O circuito precisa mostrar os bits: ligue
  *Bordas exatas do microcontrolador* (qualquer taxa quando uma placa aciona a
  linha), ou mantenha cada bit com pelo menos quatro passos de tempo (2400 baud ou
  menos com o passo de 0,1 ms); caso contrário, o painel Mensagens avisa que o pino
  não é decodificado. Um byte chega ao DR cerca de 0,4 ms após o seu stop bit (as
  entradas chegam ao firmware atrasadas em até uma janela de acoplamento). Bytes
  também chegam do terminal, e o TX de uma placa na mesma rede que o RX de outra
  placa é repassado no nível de byte (com um quadro de defasagem) com qualquer
  passo.
- **Velocidade e corrente dos pinos.** A velocidade das bordas (MODE 10/2/50 MHz) e
  os limites de corrente não são simulados; um LED sem resistor consome uma corrente
  irreal.
- **Watchdogs e RTC.** O LSI roda a exatos 40 kHz (num chip real: 30–60 kHz), então
  os timeouts do IWDG e um RTC com clock do LSI são exatos aqui e variam nas placas.
  O LSE parte imediatamente (um cristal real precisa de até alguns segundos). As
  escritas no RTC em modo de configuração têm efeito imediato (RTOFF sempre lê 1).
  Enquanto o depurador mantém o firmware parado, os watchdogs e o RTC também ficam
  parados (o tempo para), como com os bits de stop do DBGMCU ativados.
- **Alimentação.** A placa é alimentada pelo seu regulador de 3,3 V (uma fonte
  ideal de 3,3 V atrás de 0,1 Ω); os pinos de 5 V e VBAT não são ligados
  internamente.

## Depuração com STM32CubeIDE ou GDB

O emulador do SimNodus contém um servidor GDB (SN-180), então o firmware pode ser
depurado enquanto aciona o circuito: breakpoints, execução passo a passo,
variáveis, registradores e memória. Enquanto o firmware está parado, **o circuito
também fica parado**; quando ele volta a rodar, ambos continuam a partir do mesmo
instante.

1. No SimNodus, marque **Simular > Depurar firmware com GDB (porta 3333)** e
   inicie a simulação (F5). A barra de status mostra *Aguardando o depurador (porta
   GDB 3333)*: o firmware espera no seu reset handler e o circuito em 0 s. Uma
   segunda placa usa a porta 3334, e assim por diante.
2. O debugger carrega o firmware, como com um ST-LINK: o campo Firmware da placa
   pode ficar vazio e, se contiver um ELF, o download feito pelo debugger o
   substitui. Após um download o chip parte do reset.
3. **STM32CubeIDE:** Run > Debug Configurations…, selecione a configuração
   *STM32 C/C++ Application* do projeto. Na aba **Debugger** escolha
   *Connect to remote GDB server*, host `localhost`, porta `3333`, e clique em
   **Debug** (mantenha *Download* marcado na aba **Startup**). Depois use
   breakpoints, Step Over/Into, Resume, Suspend e as visões Variables, Registers
   e Expressions como com uma placa. Testado com o STM32CubeIDE 2.1.1.
4. **arm-none-eabi-gdb** (vem com o STM32CubeIDE, na pasta GNU Tools for
   STM32): `arm-none-eabi-gdb Debug/project.elf`, depois
   `target extended-remote :3333`, `break main`, `continue`, `next`,
   `print variable`, `monitor reset`, `detach`.
5. Encerrar o debugger (detach ou terminate) deixa o firmware continuar rodando;
   a simulação em si continua até você pará-la no SimNodus.

Suportado: qualquer número de breakpoints e watchpoints (parar quando uma variável
é escrita, lida ou ambos: *Add Watchpoint* do STM32CubeIDE, `watch`, `rwatch` e
`awatch` do gdb; o firmware roda mais devagar enquanto houver um definido), Step,
Resume, Suspend (Ctrl-C),
registradores (r0–r15, xPSR, MSP, PSP, PRIMASK, CONTROL), memória e
registradores de periféricos, `monitor reset` e `monitor reset halt`, `load`.
Ler um registrador de periférico no debugger tem os
mesmos efeitos colaterais que no chip (ler `USART_DR` limpa RXNE). O servidor GDB
escuta apenas em 127.0.0.1. Após `monitor reset`, o gdb pode continuar
mostrando o PC antigo até o próximo step ou continue.

## Compilação do firmware

Qualquer projeto para STM32F103C8 funciona se gerar um ELF, inclusive projetos do
STM32CubeIDE com a HAL e o `SystemClock_Config()` gerado por ele. A maioria dos
exemplos em `examples/firmware/` não usa bibliotecas; `hal_clock` (exemplo 12) e
`hal_adc_dma` (exemplo 15, ADC com DMA, PWM de timer, UART com DMA) são escritos
como projetos do STM32CubeIDE e usam a HAL STM32CubeF1. Todos são compilados com
`python tools/build_firmware.py` (encontra o compilador de uma instalação do
STM32CubeIDE ou o `arm-none-eabi-gcc` no PATH, e o pacote STM32CubeF1
que o STM32CubeIDE baixa em `~/STM32Cube/Repository`). Os projetos guardam o
caminho do ELF relativo ao arquivo do projeto; **Recarregar** em Propriedades
carrega um ELF recompilado.

Base técnica: ADR 0083–0094 em `docs/decisions/`.
