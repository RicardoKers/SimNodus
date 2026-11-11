<!-- source: MICROCONTROLLER.md sha256 7e374792431925480b36f9ec9ce7fdacda7a4798e5d5161409dd62c4742badc7 -->
# Microcontrolador: o que é simulado

O SimNodus executa o firmware da **Blue Pill (STM32F103C8)** em seu próprio emulador
de STM32, junto com o circuito. Esta página lista o que o chip simulado suporta e
onde ele difere de uma placa real (MCU-09); o [guia do usuário](USER_GUIDE.md)
cobre o restante do SimNodus. A placa **ESP32-C3-DevKitM-1** é nova e
experimental: veja [Placa ESP32-C3 (experimental)](#placa-esp32-c3-experimental).
O mesmo vale para o ATmega328P, o [ATmega32](#atmega32-e-atmega32a-experimental) e o [Arduino UNO](#arduino-uno-experimental).

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
| Pinos ligados a nada | sim | Não fazem parte do circuito, o que deixa a simulação mais rápida: uma entrada lê o pull-up ou pull-down do próprio chip (0 sem nenhum, 0 V no ADC). Para ver a tensão de um pino assim, coloque nele uma ponta de prova ou um rótulo de rede; vale para todas as placas e chips |
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
MCP3208, MCP41010, W25Q32, cartão microSD, HX711, DS18B20, DHT22, HC-SR04) e
o LCD de caracteres, o OLED SSD1306 e a fita WS2812B (em *Displays*) funcionam em conjunto com
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
- Os chips de um fio temporizados (SN-225) seguem os tempos das bordas que o
  firmware produz (atrasos de microssegundos, por exemplo com o contador de
  ciclos DWT): o DS18B20 responde a resets, comandos de ROM (vários sensores
  dividem um barramento, cada um com seu código) e conversões de 94 a 750 ms,
  lendo +85 °C antes da primeira; o DHT22 ou DHT11 envia seus 40 bits depois
  do sinal de início; o HC-SR04 mantém ECHO em alto 58 µs por centímetro; a
  fita WS2812B decodifica pulsos de 0,4 e 0,8 µs e mostra suas cores no
  esquemático. O exemplo 40 usa os quatro.
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

## Placa ESP32-C3 (experimental)

O componente **ESP32-C3-DevKitM-1** (Microcontroladores) executa firmware de
ESP32-C3 no próprio emulador RISC-V do SimNodus (ADR 0106). Ele é
**experimental**: o mesmo firmware já rodou sem modificações (exemplos do
ESP-IDF, um sketch do Arduino, a imagem oficial do MicroPython), mas ainda não
foi comparado com uma placa real, então pequenas diferenças podem existir.

- **Firmware**: uma imagem da flash (`.bin`) gravada a partir do endereço 0,
  como o `esptool write-flash 0x0` grava: a saída do `idf.py merge-bin` do
  ESP-IDF, o `.merged.bin` da IDE do Arduino (Sketch > Export Compiled Binary)
  ou o `.bin` do firmware `ESP32_GENERIC_C3` do MicroPython. Uma imagem de
  aplicação também funciona (o `build/app.bin` do ESP-IDF ou do
  Espressif-IDE, o `.ino.bin` da IDE do Arduino) quando o arquivo
  `flash_args` da compilação está ao lado dela, como a compilação o deixa: o
  SimNodus coloca o bootloader, a tabela de partições e a aplicação nos seus
  endereços, como o `esptool write-flash @flash_args` faz. Arquivos ELF não
  são aceitos. O exemplo 45 pisca um LED no GPIO8 com o exemplo blink do
  ESP-IDF.
- **ROM**: a ROM do chip é reconstruída no início a partir do arquivo de ROM
  da Espressif, que acompanha o SimNodus (`roms/esp32c3_rev3_rom.elf`,
  Apache-2.0).
- **Pinos**: GPIO0-GPIO10 e GPIO18-GPIO21, como nos conectores da placa; pinos
  EN, 3V3, 5V e G. As saídas fornecem 3,3 V ou 0 V através de 33 Ω; as
  entradas leem 1 acima de 2,475 V e 0 abaixo de 0,825 V (0,75 e 0,25 x VDD do
  datasheet); pull-ups/pull-downs internos de 45 kΩ. O EN e o GPIO9 (BOOT) da
  placa têm pull-ups de 10 kΩ. O LED RGB (um WS2812 no GPIO8) não é desenhado.
- **Portas seriais** no terminal: **UART0** (o conector USB da placa; GPIO21
  TX, GPIO20 RX; o log do ESP-IDF e do Arduino) e **USB Serial/JTAG** (o USB
  do próprio chip, nos GPIO18/GPIO19; o prompt `>>>` do MicroPython). A UART0
  e a UART1 enviam e recebem na taxa de transmissão, bit a bit nos seus
  pinos, então o log de boot leva o mesmo tempo que na placa, e o componente
  Porta Serial ou outra placa ligada aos GPIO21/GPIO20 conversa com elas. Veja
  [O USB da placa no computador](#o-usb-da-placa-no-computador) para o
  Arduino IDE, o Thonny, o mpremote e o esptool.
- **Simulado**: o núcleo RV32IMC, 4 MB de flash, SRAM, caches e mapeamento da
  flash, interrupções, temporizador do sistema, grupos de timers e watchdogs,
  RTC, light sleep e deep sleep, GPIO com interrupções, UART0/UART1, USB
  Serial/JTAG, LEDC (PWM), o ADC nos GPIO0-GPIO4 (aproximado: linear, sem
  ruído), SHA, números aleatórios e eFuse (MAC 34:85:18:00:C3:01).
- **ADC**: leituras avulsas e conversões contínuas com DMA (o driver
  `adc_continuous` do ESP-IDF, até 83 kHz), cada uma com a tensão do circuito
  no seu próprio instante, de modo que uma forma de onda é amostrada como na
  placa. As leituras calibradas ficam a cerca de 2 % (1,7 % abaixo perto de
  2,4 V com 12 dB). O exemplo 49 amostra uma senoide de 50 Hz e um
  potenciômetro a 20 kHz.
- **Sensor de temperatura**: mede a propriedade **Temperatura do chip** da
  placa (25 °C por padrão; mude-a durante a simulação). O driver
  `temperature_sensor` do ESP-IDF, o `esp32.mcu_temperature()` do MicroPython
  e o `temperatureRead()` do Arduino a leem; o ESP-IDF arredonda para baixo
  em graus inteiros, então uma leitura pode ficar até 1 °C abaixo.
- **I2C**: o I2C0 como mestre em qualquer GPIO, pela matriz de GPIO, com os
  chips I2C do SimNodus (os mesmos usados com a Blue Pill) no nível dos pinos:
  o driver de I2C do ESP-IDF, o `Wire` do Arduino e o `machine.I2C` do
  MicroPython funcionam sem mudanças, com clock stretching e um chip ausente
  informado como NACK. O barramento precisa de pull-ups: resistores no
  esquema, ou os pull-ups internos de 45 kΩ dos pinos, que os drivers ligam.
  O exemplo 46 lê um LM75 e grava uma 24C02. O modo escravo não é simulado.
- **SPI**: o SPI2 como mestre (modos 0-3, até 80 MHz, MSB ou LSB primeiro, CS0-CS5
  por hardware, fases de comando, endereço e dummy) em qualquer GPIO, pela
  matriz de GPIO, ou nos seus próprios pinos (SCLK GPIO6, MOSI GPIO7, MISO
  GPIO2, CS GPIO10), com ou sem DMA, com os chips SPI do SimNodus no nível
  dos pinos: o driver SPI master do ESP-IDF, o `SPI` do Arduino e o
  `machine.SPI` do MicroPython funcionam sem mudanças. O exemplo 47 lê um
  MCP3208 e grava uma W25Q32. Linhas duplas e quádruplas e o modo escravo não
  são simulados.
- **RMT**: os dois canais de transmissão em qualquer GPIO, com os símbolos
  na memória do RMT, reabastecimento circular, repetições e o nível de
  repouso, de modo que os trens de pulsos saem com o tempo exato: fitas
  WS2812B acendem pelo driver RMT do ESP-IDF, pelo `rgbLedWrite` do Arduino e
  pelo `neopixel` do MicroPython. O exemplo 48 percorre uma fita com uma luz.
  Os canais de recepção e a portadora não são simulados.
- **Wi-Fi** (experimental): a placa se conecta ao ponto de acesso do
  SimNodus; veja [Wi-Fi](#wi-fi). **Ainda não**: Bluetooth.
- **Depuração** com o `riscv32-esp-elf-gdb`: veja
  [Depuração com STM32CubeIDE ou GDB](#depuração-com-stm32cubeide-ou-gdb).
- **Tempo**: uma instrução por ciclo de clock da CPU (160 MHz com a
  configuração padrão); faltas de cache e esperas da flash não são modeladas,
  então o código roda um pouco mais rápido que no chip. Os timers contam nas
  suas taxas reais.
- **Pressionar RESET** reinicia o chip como o botão EN; a flash guarda o que o
  firmware gravou (os arquivos do MicroPython), e a próxima execução também
  (veja [Arquivos do MicroPython](#arquivos-do-micropython)).

### O USB da placa no computador

As duas portas seriais da placa podem chegar a programas do computador, de
modo que o Arduino IDE (Monitor Serial e Carregar), o Thonny, o mpremote, o
esptool ou um terminal funcionam com a placa simulada como com uma real. Nas
propriedades da placa, **UART0 para o computador** é o conector USB da DevKit
(a UART0 atrás da sua ponte USB-serial); **USB Serial/JTAG para o
computador** é o USB do próprio chip (o REPL do MicroPython). Cada uma aceita:

- **Uma porta serial** do computador: uma ponta de um par de portas virtuais
  (o com0com, ou um programa parecido, cria duas portas ligadas entre si, por
  exemplo COM10 e COM11). Escolha COM10 no SimNodus e COM11 no Arduino IDE ou
  no Thonny. Esses dois programas só listam portas COM, por isso precisam do
  par.
- **tcp:4000** (qualquer número de porta livre): o SimNodus escuta só neste
  computador. Programas feitos com o pyserial a abrem como
  `rfc2217://localhost:4000`: `esptool --port rfc2217://localhost:4000 ...` e
  o mpremote; nenhum driver é necessário. Um terminal TCP cru
  (`socket://localhost:4000`, o modo Raw do PuTTY) também recebe os bytes.
- **Vazio**: sem ligação (só o terminal).

A ligação abre quando a simulação começa; o painel Mensagens diz onde cada
porta está ligada, ou por que não pôde ser. O terminal continua mostrando o
que a placa envia.

**DTR e RTS** funcionam como na placa. Na UART0 eles chegam ao EN e ao GPIO9
pelos dois transistores da DevKit, então o reset automático do esptool e do
Arduino IDE põe o chip no modo de download, e o carregador da própria ROM
grava a flash (Carregar no Arduino IDE, `esptool write-flash`). Na porta USB
Serial/JTAG eles seguem a lógica de reset do chip (o `--before usb_reset` do
esptool, ou a mesma sequência da UART0). O painel Mensagens avisa quando um
firmware novo está sendo gravado; quando a simulação para, ele é salvo como
`projeto-U1-flash.bin` (o nome do projeto e o designador da placa) ao lado do
projeto e passa a ser o firmware da placa, como uma placa real guarda o que
foi gravado; o projeto precisa ter sido salvo uma vez.

Uma placa nova não precisa de firmware para começar: sem ele a flash fica
apagada, como num módulo em branco, e a ROM mostra `invalid header:
0xffffffff` na UART0 até o Arduino IDE ou o esptool gravar um firmware, que a
placa então guarda.

Qual porta o `Serial` de um sketch do Arduino usa é escolha do sketch, como
numa placa real: a UART0 por padrão, a porta USB Serial/JTAG com **Ferramentas
> USB CDC On Boot: Enabled** no Arduino IDE (aí o `Serial0` é a UART0). O REPL
do MicroPython fica na porta USB Serial/JTAG.

As gravações levam o tempo simulado de que precisam. Pela UART0 os bytes
trafegam na velocidade de gravação, como pela ponte USB-serial do DevKit: em
velocidade de tempo real, o Upload da IDE do Arduino de um sketch de 300 KB
leva uns 5 segundos a 921600 baud (o padrão) e uns 20 segundos a 115200. A
porta USB Serial/JTAG não tem taxa de transmissão, então a mesma gravação leva
uns 2 segundos por ela. Por `tcp:PORTA`, o cliente RFC 2217 do esptool soma
uns 0,2 s a cada comando (meio minuto para a mesma gravação); um par de portas
virtuais não. O Thonny e o mpremote podem reiniciar a placa ao conectar, como
fazem com uma real.

### Arquivos do MicroPython

O MicroPython guarda seus arquivos (`main.py`, `boot.py`, módulos, dados) num
sistema de arquivos na flash. Copie-os para a placa e de volta como numa
real: no Thonny, **View > Files** com a porta da placa escolhida, depois
**Upload to /** ou **Download**; com o mpremote, `mpremote connect
rfc2217://localhost:4000 fs cp main.py :main.py` (e `fs cp :data.txt .` para
o caminho de volta).

O que o firmware grava na flash (os arquivos do MicroPython, o NVS do
ESP-IDF, as `Preferences` do Arduino) é guardado como as gravações pelo USB:
quando a simulação para, a flash é salva como `projeto-U1-flash.bin` ao lado do
projeto e passa a ser o firmware da placa, então a próxima execução começa
com os arquivos e roda o `main.py` ao iniciar. O projeto precisa ter sido
salvo uma vez: um exemplo aberto por **Arquivo > Exemplos** não guarda nada
até ser salvo com um nome próprio. Para recomeçar de uma imagem limpa,
escolha de novo o `.bin` original na propriedade **Firmware** da placa (ou
desfaça a mudança), ou apague a flash com `esptool erase-flash` e grave a
imagem de novo.

### Wi-Fi

O Wi-Fi é experimental: o fabricante não documenta o hardware de rádio da
placa, então ele foi modelado a partir do comportamento das próprias
bibliotecas de Wi-Fi da Espressif, e os resultados ainda não foram
comparados com uma placa real.

As placas ESP32-C3 de um projeto veem uma rede Wi-Fi: o **ponto de acesso
do SimNodus**. O firmware se conecta a ela como a uma rede real, sem
mudanças: o `esp_wifi` do ESP-IDF, o `WiFi.begin()` do Arduino e o
`network.WLAN` do MicroPython.

- **Nome e senha**: `SimNodus` e `simnodus-wifi` (WPA2-PSK, canal 6), a
  menos que o projeto os mude em **Simular > Rede Wi-Fi...**. A rede também
  pode ser aberta, sem senha.
- **Endereços**: o ponto de acesso é 192.168.4.1. Ele dá às placas
  192.168.4.2, .3 em diante por DHCP e é o gateway e o servidor DNS delas.
- **Várias placas**: placas na mesma rede se alcançam por esses endereços,
  como numa rede real. Cada placa tem seu próprio endereço MAC
  (34:85:18:00:c3:01, :05, :09...).
- **As buscas** (scans) listam a rede com RSSI de -40 dBm. Placas que nunca
  ligam o Wi-Fi rodam tão rápido quanto antes.

Um firmware escrito para um ponto de acesso real roda aqui sem mudanças se
usar o mesmo nome e a mesma senha. O exemplo 60 é um servidor web: a página
mostra o LED e a temperatura do chip, e os links dela ligam e desligam o
LED.

**O gateway para este computador.** A rede fica dentro da simulação, a
menos que o projeto ligue **Gateway para a rede deste computador** em
**Simular > Rede Wi-Fi...**. Com o gateway:

- O firmware das placas alcança este computador como `host.simnodus`
  (192.168.4.254), a rede local e a internet, como um aparelho na rede deste
  computador: páginas web, brokers MQTT, a hora por NTP e o ping (este
  computador envia o ping e devolve a resposta).
- **Portas deste computador redirecionadas às placas** deixam o servidor de
  uma placa acessível deste computador: com a porta 8080 redirecionada a
  192.168.4.2:80, um navegador abre `http://127.0.0.1:8080/` enquanto a
  simulação roda. As portas redirecionadas escutam só em 127.0.0.1, então
  outros computadores não as alcançam.
- Na primeira vez que um projeto com o gateway roda num computador, o
  SimNodus pergunta se pode permitir. A resposta fica guardada nesse
  computador, não no projeto, então um projeto recebido de outra pessoa
  nunca chega à rede sem a pergunta. Recusar mantém a rede das placas dentro
  da simulação.

Ligue o gateway só para firmware em que você confia. Execuções pelo
`simnodus-cli` ou por um agente de IA o deixam desligado, a menos que o
comando peça com `"allow_network": true`.

**Limites.**

- O tempo no ar é aproximado: todo quadro vai a 1 Mbit/s, e não há colisões,
  níveis de sinal nem interferência. Downloads são mais lentos que numa
  placa real.
- Não há modo ponto de acesso na própria placa (SoftAP), nem ESP-NOW, nem
  WPA3, nem Bluetooth.
- Sem o gateway, um ping de uma placa só alcança o ponto de acesso
  (192.168.4.1). No Linux, pings pelo gateway precisam que
  `net.ipv4.ping_group_range` inclua o usuário, como a maioria das
  distribuições configura.
- Duas execuções sem o gateway dão os mesmos resultados. Com o gateway, a
  rede do computador decide o tempo.

## ATmega8 e ATmega8A

As peças **ATmega8-PU** e **ATmega8A-PU** (Microcontroladores) são os chips
AVR avulsos em PDIP-28, como na protoboard. Elas rodam no emulador AVR do
próprio SimNodus (experimento E-10, ADR 0108).
- Cada instrução foi conferida com o AVR Instruction Set Manual, e os
  periféricos com o datasheet.
- Os exemplos 50, 51, 52 e 54 se comportaram num ATmega8 real como na
  simulação: porta serial, PWM, clock a cristal, EEPROM e sleep.
- O ADC, o comparador analógico e os níveis de brown-out são aproximados.
  Eles ainda não foram medidos num chip.

- **Firmware**: um arquivo Intel HEX, como os que o Microchip Studio, o
  MPLAB X, o `avr-objcopy` ou o Sketch > Exportar Binário Compilado da IDE
  do Arduino gravam. Um arquivo ELF do avr-gcc também funciona. Os fusíveis
  dele (a macro `FUSES`) e os dados da EEPROM valem como um gravador de
  produção os grava. Quando a simulação começa, os fusíveis do ELF que
  diferem dos da peça viram as propriedades da peça, num passo que Desfazer
  volta, e as mensagens avisam.
- **Alimentação**: ligue VCC, AVCC e os dois pinos GND. O chip lê a sua
  alimentação do circuito:
  - Abaixo de cerca de 1,4 V no VCC ele fica desligado e avisa; quando o
    VCC volta, parte com um reset de ligação (power-on).
  - Com o **BODEN**, fica em reset abaixo do nível de brown-out (2,6 V, ou
    3,8 V com o BODLEVEL programado), e volta com o BORF.
  - Sem o BODEN, uma alimentação abaixo do mínimo do datasheet para o
    clock (2,7 V; 4,5 V acima de 8 MHz ou para o ATmega8) gera um aviso.
  - Os pinos acionam o circuito como a 5 V, seja qual for o VCC.
- **Fusíveis**: as propriedades **Fusível low**, **Fusível high** e **Bits de trava (lock)**,
  em hexadecimal, com os bits programados em 0. Elas valem quando a
  simulação começa, como ao ligar o chip. **Fusíveis...**, logo abaixo,
  mostra os mesmos bits campo a campo e mantém as duas vistas iguais: a
  fonte de clock e o tempo de partida, o brown-out, CKOPT, RSTDISBL, WDTON,
  EESAVE, a seção de boot e os modos de trava. Uma caixa marcada é um bit
  programado. **Valores de fábrica** volta a E1, D9 e FF.
  - Os valores de fábrica (E1, D9) escolhem o oscilador RC interno a 1 MHz,
    com 65 ms de partida, como num chip novo. Um código compilado para
    16 MHz roda 16 vezes mais devagar até os fusíveis mudarem.
  - Para um cristal, use o low fuse EF e o high fuse C9 (CKOPT), e coloque
    uma peça **Cristal** (Passivos) entre XTAL1 (PB6) e XTAL2 (PB7). A
    frequência do cristal é o clock do chip; a oscilação dele não é
    simulada.
  - Com um cristal escolhido e nenhum ligado, o chip não parte, e o log
    explica por quê.
  - O RSTDISBL faz do PC6 um pino de E/S; sem ele, o PC6 é o RESET, com o
    seu pull-up, e levá-lo a nível baixo reinicia o chip. EESAVE, WDTON,
    BOOTSZ e BOOTRST agem como no chip.
- **O que roda**:
  - as portas, INT0 e INT1;
  - Timer0, Timer1 e Timer2 em todos os modos, exatos em ciclos (PWM em
    OC1A, OC1B e OC2, captura de entrada);
  - a USART, bit a bit em PD1 e PD0, no terminal ou pela propriedade
    **USART para o computador**;
  - a EEPROM, o watchdog, os resets e a autoprogramação (SPM);
  - o ADC: ADC0-ADC5 em PC0-PC5, o bandgap e o GND; a referência AVCC,
    AREF ou a interna de 2,56 V, com AVCC e AREF tirados do circuito;
    conversões únicas e contínuas com os seus tempos (13 clocks do ADC, 25
    na primeira). Cada uma lê a tensão do circuito no instante da
    amostragem;
  - o comparador analógico: AIN0 ou o bandgap contra AIN1 ou uma entrada
    do ADC, com a sua interrupção e a captura de entrada do Timer1. Ele
    amostra o circuito a cada 10 µs, então uma borda mais curta que isso
    pode passar despercebida;
  - o SPI como mestre, bit a bit em SCK (PB5), MOSI (PB3) e MISO (PB4) nos
    quatro modos, até F_CPU / 2. O SS (PB2) levado a nível baixo como
    entrada encerra o modo mestre, como no chip;
  - o TWI (I2C) como mestre em SDA (PC4) e SCL (PC5), em dreno aberto, com
    os códigos de estado do TWSR e o clock stretching. Ele conversa com os
    chips I2C da biblioteca, com os pull-ups no circuito;
  - os modos de sleep:
    - Idle, ADC Noise Reduction, Power-down, Power-save e Standby, com as
      fontes de despertar do datasheet: do Power-down só uma interrupção
      por nível em INT0/INT1 ou o watchdog acorda o chip;
    - os temporizadores param enquanto o clock de E/S para;
    - acordar do Power-down leva o tempo de partida do oscilador;
  - o Timer2 assíncrono (AS2) num **Cristal** de 32,768 kHz entre TOSC1
    (PB6) e TOSC2 (PB7), com o clock principal no RC interno. Ele continua
    rodando no Power-save, e as flags de atualização pendente funcionam
    como no chip.
- **Ainda não**: SPI e TWI como escravos. O log avisa uma vez quando o
  firmware usa um deles.
- **EEPROM**: o que o firmware grava nela fica guardado ao lado de um
  projeto salvo como `<projeto>-<designador>-eeprom.bin` (projetos na mesma
  pasta guardam cada um o seu), que passa a ser a
  propriedade **EEPROM** do chip. A próxima simulação começa com ele, como o
  chip guarda a EEPROM desligado.
  - Sem EEPROM guardada, o chip começa apagado, ou com os dados da EEPROM
    do ELF.
  - Esvazie a propriedade para começar apagado de novo.
  - Um exemplo de **Arquivo > Exemplos** é uma cópia sem arquivo: o que uma
    simulação grava na EEPROM fica guardado quando o projeto é salvo (as
    mensagens avisam depois da simulação).
  - **Ver...**, ao lado da propriedade, abre o **Visualizador de memória**
    na EEPROM do chip: os bytes como o chip em execução os tem, ou como a
    última simulação os deixou. **Carregar...** ali (com a simulação
    parada) faz de um arquivo `.bin` ou `.hex` a EEPROM do chip.
- **Exemplos**: 50 (a USART a 9600 baud, com um contador guardado na
  EEPROM), 51 (um pisca a 16 MHz com cristal) e 52 (um potenciômetro no
  ADC ajustando um PWM, sincronizados por um tick de 1 ms do timer) e 53
  (um LM75 e uma 24C02 por TWI, um 74HC595 por SPI) e 54 (um relógio de
  tempo real com cristal de relógio, dormindo em Power-save). Os fontes dos firmwares
  estão em `examples/firmware/avr_*`.
- **Depuração** com o `avr-gdb` (todos os chips AVR e o UNO): veja
  [Depuração com STM32CubeIDE ou GDB](#depuração-com-stm32cubeide-ou-gdb).

## ATmega328 e ATmega328P (experimental)

As peças **ATmega328P-PU** (o chip do Arduino UNO) e **ATmega328-PU**
(Microcontroladores) têm a pinagem do ATmega8 e rodam no mesmo emulador
(SN-260). São **experimentais** até serem comparadas com um chip real (ADR
0108). As duas diferem só na assinatura (1E 95 0F e 1E 95 14) e no BODS do
328P. Tudo o que a seção do ATmega8 acima diz vale, com estas diferenças:

- **Memórias**: 32 KB de flash, 2 KB de SRAM, 1 KB de EEPROM. Os vetores
  de interrupção ocupam duas palavras (JMP).
- **Fusíveis**: um terceiro byte, o **Extended fuse**, guarda o BODLEVEL.
  O diálogo **Fusíveis...** mostra os campos do 328.
  - Os valores de fábrica (62, D9, FF) escolhem o oscilador RC interno de
    8 MHz dividido por 8 (CKDIV8): o chip começa a 1 MHz. O firmware pode
    mudar a divisão durante a execução pelo CLKPR (`clock_prescale_set` da
    avr-libc), e o clock da simulação acompanha.
  - Os fusíveis do Arduino UNO são FF, DE e FD: um cristal de 16 MHz
    (coloque um **Crystal** entre PB6 e PB7), uma seção de boot de 256
    palavras com o vetor de reset nela, brown-out em 2,7 V.
  - O oscilador de cristal de baixo consumo avisa quando o cristal passa da
    faixa que o CKSEL3:1 escolhe. CKOUT e DWEN são anotados no log.
- **Brown-out**: BODLEVEL de 1,8, 2,7 ou 4,3 V, do extended fuse.
- **O que roda**, além do que o ATmega8 tem:
  - Timer0 e Timer2 com duas saídas de comparação cada (OC0A PD6, OC0B
    PD5, OC2A PB3, OC2B PD3) nos oito modos de forma de onda; o TCCR1C do
    Timer1;
  - as interrupções de mudança de pino PCINT0-23 nas portas B, C e D. Elas
    acordam o chip de todos os modos de sleep;
  - INT0 e INT1 pelo EICRA e pelo EIMSK; escrever 1 num bit de PINx
    inverte o pino;
  - o modo de interrupção do watchdog (WDIE), seu oscilador de 128 kHz
    (16 ms a 8 s), e o reset que o WDRF mantém ligado até o firmware
    limpar o MCUSR, como no chip;
  - a referência de 1,1 V do ADC, o sensor de temperatura (ADC8, lido como
    a 25 °C: cerca de 352; chips reais precisam de calibração) e o disparo
    automático pelo ADTS (contínuo, o comparador, INT0, eventos do Timer0
    e do Timer1);
  - o UCSR0C e o UBRR0H próprios da USART0, os modos só apagar e só
    escrever da EEPROM (EEPM), DIDR0 e DIDR1, GPIOR0-2, o sleep extended
    standby;
  - a leitura da assinatura e dos fusíveis pelo SIGRD e pelo BLBSET do SPM.
- **Ainda não**: o modo mestre SPI da USART, o PRR parando um módulo (ele
  é aceito; o módulo continua rodando) e o clock externo do Timer2 (EXCLK).
- **Exemplos**: 55 (serial a 38400 baud depois do CLKPR, o sensor de
  temperatura, um contador na EEPROM), 56 (as quatro saídas PWM de 8 bits e
  um ADC disparado pelo overflow do Timer0) e 57 (power-down acordado pelo
  watchdog, por uma mudança de pino e pelo INT0). Os fontes dos firmwares
  estão em `examples/firmware/avr328_*`.

## ATmega32 e ATmega32A (experimental)

As peças **ATmega32-PU** e **ATmega32A-PU** (Microcontroladores) são os
chips em PDIP-40 (SN-263), no mesmo emulador. São experimentais até serem
comparadas com um chip real. Seguem a seção do ATmega8 acima (os mesmos
registradores, fusíveis e fontes de clock), com estas diferenças:

- **Pinos**: quatro portas, A a D (PA0-PA7 são ADC0-ADC7). **RESET**,
  **XTAL1** e **XTAL2** são pinos próprios; um **Crystal** vai entre XTAL1 e
  XTAL2, e o cristal de 32,768 kHz do Timer2 entre TOSC1 (PC6) e TOSC2
  (PC7).
- **JTAG**: o fusível alto de fábrica (99) tem o **JTAGEN** programado,
  então PC2-PC5 são os pinos do JTAG e não seguem DDRC e PORTC, como num
  chip novo. O firmware os libera escrevendo JTD no MCUCSR duas vezes em
  quatro ciclos (`MCUCSR = _BV(JTD); MCUCSR = _BV(JTD);`), ou o fusível alto
  D9 faz isso. O log avisa quando o firmware escreve nesses pinos enquanto o
  JTAG os ocupa.
- **Memórias**: 32 KB de flash, 2 KB de SRAM, 1 KB de EEPROM; vetores de
  duas palavras.
- **O que roda**, além do que o ATmega8 tem:
  - o Timer0 com uma saída de comparação no OC0 (PB3) nos seus quatro
    modos; as saídas do Timer1 no OC1A (PD5) e no OC1B (PD4), a captura no
    ICP1 (PD6); o OC2 do Timer2 no PD7;
  - o INT2 no PB2 (uma borda escolhida pelo ISC2; ele acorda o chip de
    todos os modos de sleep);
  - as oito entradas do ADC, os canais diferenciais com ganho de 1, 10 ou
    200 (os valores ideais do datasheet: erros de offset e de ganho não são
    simulados), o bandgap de 1,22 V, o disparo automático pelo ADTS do
    SFIOR;
  - SPI em PB4-PB7, TWI em PC0 (SCL) e PC1 (SDA), o comparador em PB2
    (AIN0) e PB3 (AIN1); o sleep extended standby.
- **Ainda não**: o JTAG e a depuração no chip em si (o OCDEN não tem
  efeito); o `avr-gdb` funciona pelo próprio servidor GDB do SimNodus.
- **Exemplo**: 59 (JTD e uma contagem em PC2-PC5, um PWM no OC0 pelo
  potenciômetro, o INT2, o ADC diferencial). O fonte do firmware está em
  `examples/firmware/avr32_demo`.

## Arduino UNO (experimental)

A peça **Arduino UNO R3** (Microcontroladores) é a placa como sai da
fábrica (SN-261): um ATmega328P (veja a seção acima) a 16 MHz com o
**Optiboot** na seção de boot e os fusíveis do Arduino (FF, DE, FD; lock
0F), então a IDE do Arduino e o avrdude gravam nela sem mudanças. Os pinos
têm os nomes dos conectores: **D0**-**D13**, **A0**-**A5**, **RESET**,
**AREF**, e os **5V**, **3V3**, **IOREF**, **VIN** e **GND** da placa. Ela é
experimental até ser comparada com uma placa real.

- **Alimentação**: a placa é alimentada pelo conector USB. **5V** e **3V3**
  são saídas para o circuito; ligue um **GND** ao terra do circuito.
- **O LED L** no D13 acende na placa, e o **RESET** da placa a reinicia (o
  Optiboot então espera cerca de um segundo por uma gravação, como numa
  placa real).
- **Firmware**: o `.hex` de um sketch (**Sketch > Exportar Binário
  Compilado** da IDE do Arduino, com ou sem o bootloader) ou um `.elf` do
  avr-gcc. Vazio: a placa tem só o bootloader e espera uma gravação.
- **USB para o computador**: a USART (D1 TX, D0 RX) pelo conector USB da
  placa, como o **UART0 para o computador** da ESP32-C3 acima: um par de
  portas virtuais (com0com ou parecido) ou `tcp:PORTA`. O **DTR** reinicia
  a placa pelo seu capacitor de 100 nF, como a IDE do Arduino faz antes de
  gravar. Alguns pares de portas virtuais (o da Eterlogic, com o avrdude da
  IDE do Arduino) não repassam esse DTR, então a placa também reinicia no
  Optiboot quando o avrdude pede o bootloader duas vezes a um sketch em
  execução (as primeiras tentativas "not in sync" são normais); o painel
  de Mensagens avisa:
  1. Ponha em **USB para o computador** uma ponta do par (COM10) e inicie a
     simulação.
  2. Na IDE do Arduino escolha **Ferramentas > Placa > Arduino UNO** e a
     outra ponta (COM11), e então **Carregar**. O avrdude reinicia a placa e
     o Optiboot grava o sketch; o **Monitor Serial** (na velocidade do
     sketch) funciona na mesma porta.
  3. Quando a simulação para, a flash é salva como `projeto-U1-flash.bin`
     ao lado do projeto e vira o firmware da placa, e o que o sketch
     escreveu na EEPROM como `projeto-U1-eeprom.bin`. O projeto precisa ter
     sido salvo uma vez.
- **Não simulados**: a ponte USB (o ATmega16U2) em si, os resistores de
  1 kΩ dela para o D0 e o D1, os LEDs TX e RX, o regulador do VIN.
- **Exemplo**: 58 (o exemplo serial da seção *ATmega328 e ATmega328P*
  compilado para 16 MHz; `l` inverte o LED L).

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

**ESP32-C3** (SN-249): o mesmo menu funciona com o `riscv32-esp-elf-gdb`
(o ESP-IDF o instala com as suas ferramentas). A placa mantém o seu
**Firmware** (a imagem `.bin`, ou o que o esptool gravou); o gdb lê do ELF
da aplicação só os símbolos, e o `load` não é suportado. A simulação começa
com o firmware parado no vetor de reset da ROM (0x40000000), então o
`continue` passa pela ROM e pelo bootloader até a aplicação:

```text
riscv32-esp-elf-gdb build/app.elf
(gdb) target remote :3333
(gdb) break app_main
(gdb) continue
(gdb) bt
(gdb) next
(gdb) watch s_led_state
(gdb) detach
```

Registradores: x0-x31 (pelos nomes da ABI: ra, sp, a0, ...), pc e os CSRs
de máquina mstatus, misa, mie, mtvec, mscratch, mepc, mcause, mtval e mip.
Breakpoints, watchpoints, step, Ctrl-C, `monitor reset halt` e detach
funcionam como na Blue Pill; com um debugger conectado, um `ebreak` no
firmware o para em vez de gerar uma exceção. Testado com o
`riscv32-esp-elf-gdb` 16.3 do ESP-IDF v5.5.2 no exemplo blink do ESP-IDF
(breakpoints, um backtrace pelo FreeRTOS, `next`, um watchpoint, detach).

A parada em 0x40000000 é na ROM, que não tem informação de depuração no ELF
da aplicação: uma IDE mostra "no debug information available" ali, e o gdb
mostra `?? ()`. Isso é esperado; coloque um breakpoint em `app_main` e
continue. Para ver nomes na ROM, carregue também os símbolos dela:
`add-symbol-file roms/esp32c3_rev3_rom.elf` (o arquivo fica na pasta do
SimNodus).

**Espressif-IDE**: numa configuração *GDB Hardware Debugging*, escolha o
ELF do projeto, o `riscv32-esp-elf-gdb` como debugger e o alvo remoto
`localhost`, porta `3333`; na aba **Startup**, desmarque *Load image* e
coloque um breakpoint em `app_main`. Testado com o Espressif-IDE e o
exemplo blink do ESP-IDF v6.1. Uma nova conexão substitui a que está
conectada, então reiniciar a sessão de depuração enquanto a anterior ainda
está conectada funciona.

**ATmega8/8A, ATmega328/328P, ATmega32/32A e o Arduino UNO** (SN-262): o
mesmo menu funciona com o `avr-gdb`. A IDE do Arduino não o instala; ele vem
com o Microchip Studio (`toolchain\avr8\avr8-gnu-toolchain\bin\avr-gdb.exe`
na sua pasta), com o AVR 8-bit toolchain da Microchip e com o MPLAB XC8
(`avr\bin\avr-gdb.exe`); no Linux ele é o pacote `gdb-avr`. O gdb lê os
símbolos do ELF do firmware, então compile com `-g` (o avr-gcc faz isso por
padrão na configuração Debug do Microchip Studio; o
`arduino-cli compile --export-binaries` copia o `.elf` de um sketch para a
pasta `build` dele).

```text
avr-gdb firmware.elf
(gdb) target remote :3333
(gdb) break main
(gdb) continue
(gdb) next
(gdb) info registers
(gdb) x/8xb 0x800100
(gdb) watch counter
(gdb) detach
```

- **Registradores**: r0-r31, SREG, SP e PC (um endereço em bytes, como no
  ELF).
- **Endereços** como o avr-gdb os conta: a flash a partir de 0, o espaço de
  dados a partir de 0x800000 (os registradores, os registradores de I/O nos
  seus endereços de dados e depois a SRAM: `x/8xb 0x800100` mostra o início
  da SRAM do ATmega328P), a EEPROM a partir de 0x810000. Um breakpoint num
  endereço de código puro precisa de um cast, `break *(void(*)())0x2ea`: o
  avr-gdb entende `*0x2ea` como um endereço de dados (o servidor recusa
  breakpoints fora da flash).
- **A execução começa** com o firmware parado no vetor de reset; o do UNO é
  o início do Optiboot (0x7E00), e o Optiboot inicia o sketch logo depois
  desse reset de power-on, então `break main` e `continue` funcionam.
- **`load`** grava as seções de flash e de EEPROM do ELF, como um
  gravador, e o próximo passo ou continue começa de um reset; o **Firmware**
  da placa pode ficar vazio, como na Blue Pill. Os fusíveis do ELF são
  aceitos, mas ficam os da peça (ajuste-os em **Fuses...**). A flash que o
  `load` grava vale para a execução; ela não fica guardada no projeto.
- **`monitor reset`** reinicia o chip como num power-on (o MCUSR mostra o
  PORF).
- **Watchpoints** na SRAM e nos registradores de I/O param depois da
  instrução (`watch` no `UDR0` para a cada byte enviado); um watchpoint na
  EEPROM é recusado, e então o gdb vigia executando passo a passo
  (lentamente).
- **Ler um registrador de I/O** no depurador não tem efeitos colaterais (o
  `UDR` mantém o seu byte, ler o `ADCL` não trava o `ADCH`); escrever um faz
  o que a escrita do firmware faz.
- Com um depurador conectado, uma instrução `BREAK` no firmware
  (ATmega328/328P, ATmega32/32A) para o firmware depois do `BREAK`.

Testado com o `avr-gdb` 7.8 do Microchip Studio 7 nos firmwares de
`examples/firmware` (avr_blink no ATmega8, avr328_serial no ATmega328P,
avr32_demo no ATmega32A) e num sketch blink do Arduino compilado pelo
`arduino-cli` (com otimização no link): breakpoints, `next`, `stepi`, um
backtrace, `watch`, `rwatch` (parando na interrupção da USART), `load`,
`monitor reset` e detach. O depurador do próprio Microchip Studio não se
conecta a um servidor GDB; IDEs que rodam o gdb com um alvo remoto (o *GDB
Hardware Debugging* do Eclipse, o VS Code) devem funcionar, mas não foram
testadas.

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
