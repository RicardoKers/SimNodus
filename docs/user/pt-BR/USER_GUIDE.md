<!-- source: USER_GUIDE.md sha256 c64f0d07ce80524f59c593a7fa285c9a09062411203fa455c088f95140f07d81 -->
# Guia do usuário do SimNodus

O SimNodus é um simulador para sala de aula: você desenha um circuito, aperta
F5 e ele continua rodando enquanto você pressiona botões, gira
potenciômetros e observa LEDs, medidores e o Analisador de sinais. Uma placa
Blue Pill (STM32F103C8) pode executar o seu próprio firmware dentro do
circuito. Este guia descreve a versão 0.9. Para
montar um primeiro circuito em cinco minutos, veja o
[início rápido](QUICK_START.md); para todos os detalhes do microcontrolador
simulado, veja [MICROCONTROLLER.md](MICROCONTROLLER.md).

Conteúdo: [Instalação](#instalação) · [A janela](#a-janela) ·
[Desenhando um circuito](#desenhando-um-circuito) · [Componentes](#componentes) ·
[Modelos](#modelos-de-dispositivos) · [Barramentos](#barramentos-e-distribuidores) ·
[Blocos](#blocos) · [Execução](#executando-uma-simulação) ·
[Analisador de sinais](#analisador-de-sinais) · [Microcontrolador](#microcontrolador) ·
[Arquivos](#arquivos-exemplos-e-aulas) · [Preferências](#preferências) ·
[Linha de comando](#linha-de-comando) · [Solução de problemas](#solução-de-problemas) ·
[Limitações conhecidas](#limitações-conhecidas)

## Instalação

O SimNodus roda no Windows 10 e 11 de 64 bits e funciona offline; nada é
baixado enquanto você trabalha.

- **Instalador:** `SimNodus-<version>-windows-x64-setup.exe` instala só para
  você (sem precisar de direitos de administrador) ou para todos os
  usuários. Ele cria um atalho no menu Iniciar, um ícone opcional na área de
  trabalho e faz os arquivos `.snx` abrirem com um clique duplo. Para
  desinstalar, use Configurações do Windows > Aplicativos.
- **Portátil:** descompacte `SimNodus-<version>-windows-x64.zip` em qualquer
  lugar (um pendrive serve) e execute `simnodus.exe`.

Os downloads estão na [página de versões](https://github.com/RicardoKers/SimNodus/releases).
Suas configurações (tema, idioma, pastas de modelos, arquivos recentes,
disposição da janela) são guardadas por usuário do Windows; os projetos são
arquivos comuns, onde quer que você os salve.

## A janela

| Área | O que faz |
|---|---|
| **Componentes** (esquerda, Ctrl+1) | A árvore de componentes por categoria, com uma caixa de busca. A prévia abaixo mostra o símbolo, os pinos e uma breve descrição do componente selecionado |
| **Esquemático** (centro) | O desenho. A roda do mouse aplica zoom na posição do cursor, o botão do meio ou Espaço+arrastar move a vista, F enquadra o circuito |
| **Propriedades** (direita, Ctrl+2) | Valores do componente selecionado; durante a execução, suas tensões, correntes e potência ao vivo, a tensão de um fio ou rótulo selecionado e a leitura de uma ponta de prova selecionada |
| **Mensagens** (embaixo, Ctrl+3) | Verificações do circuito, mensagens do motor e do firmware. Clique duas vezes numa mensagem para mostrar o seu componente; Ctrl+C ou o menu de contexto copia as mensagens selecionadas (Copiar tudo copia todas) |
| **Barra de ferramentas** | Novo, Abrir, Salvar, Desfazer, Refazer, Fio, Ponta de prova, Executar, Pausar, Parar e a caixa de velocidade |
| **Barra de status** | Estado da execução, tempo simulado e a velocidade real em relação ao tempo real; a dica de ferramenta lista as aproximações da execução atual |

F11 oculta todos os painéis (foco no esquemático). **F1** (Ajuda > Atalhos
de teclado) lista todos os atalhos e gestos do mouse; Ajuda > Guia do
usuário abre este guia.

## Desenhando um circuito

### Inserindo componentes

Escolha um componente na árvore (clique duplo, Enter ou arraste-o para o
esquemático) e clique para colocá-lo; cada clique coloca mais uma cópia, e
Esc termina. Ctrl+R gira e Ctrl+E espelha, inclusive antes de colocar.
Atalhos de uma tecla: **R** resistor, **C** capacitor, **L** indutor,
**D** diodo, **G** terra, **V** fonte de tensão, **I** fonte de corrente,
**Shift+V** voltímetro, **Shift+I** amperímetro.

### Fios, rótulos e notas

| Ferramenta | Tecla | Como usar |
|---|---|---|
| Fio | W | Clique num pino para começar, clique para fazer as dobras, clique num pino ou num fio para terminar. Espaço troca a direção da quina; um clique duplo termina num ponto livre; Esc cancela |
| Rótulo de rede | N | Clique num fio ou pino e digite um nome. Rótulos com o mesmo nome ficam ligados sem fio; `GND` ou `0` é o terra. Ctrl+E põe o nome do outro lado |
| Barramento | B | Veja [Barramentos e distribuidores](#barramentos-e-distribuidores) |
| Distribuidor | S | Veja [Barramentos e distribuidores](#barramentos-e-distribuidores) |
| Nota de texto | T | Clique onde o texto começa e digite, por exemplo, instruções para os alunos. Clique duas vezes numa nota para editá-la; um texto vazio a exclui |
| Ponta de prova | P | Veja [Analisador de sinais](#analisador-de-sinais) |

Um **círculo vermelho** marca um pino ou ponta de fio que não está ligado; um
**ponto** marca uma junção. Passar o mouse sobre um fio ou pino mostra o nome
da rede. Todo circuito precisa de um **terra**.

### Edição

| Ação | Como |
|---|---|
| Selecionar | Clique, Ctrl+clique para acrescentar, ou arraste um retângulo; Ctrl+A seleciona tudo |
| Mover | Arraste (os fios continuam ligados) ou use as setas (um passo da grade) |
| Girar, espelhar | Ctrl+R, Ctrl+E |
| Excluir | Del |
| Área de transferência | Ctrl+C, Ctrl+X, Ctrl+V |
| Desfazer, refazer | Ctrl+Z, Ctrl+Y (toda edição, inclusive mover pontas de prova) |
| Mover um rótulo | Arraste o texto do nome ou do valor de um componente |

### Valores

Os valores aceitam notação de engenharia: `4k7`, `100n`, `2.2u`, `1meg` (um
`M` maiúsculo também significa mega; `m` é mili). Pressione Enter para
aplicar. Um valor inválido fica contornado em vermelho e não é aplicado.

## Componentes

| Categoria | Componentes |
|---|---|
| **Passivos** | Resistor, capacitor, indutor (ideais); potenciômetro; transformador (primário P1-P2, secundário S1-S2) |
| **Fontes** | Fonte de tensão CC, bateria (com resistência interna), fonte de corrente CC (saindo pelo +), fontes de tensão e de corrente ajustáveis (giradas durante a execução entre um limite inferior e um superior), fontes de tensão e de corrente controladas por tensão e por corrente (VCVS, VCCS, CCVS, CCCS), gerador de pulsos, gerador senoidal, terra |
| **Chaves e controles** | Chave SPST, chave reversível SPDT, botões NA (fechado enquanto pressionado) e NF (aberto enquanto pressionado) com repique dos contatos opcional; teclado matricial 4x4 |
| **Instrumentos** | Voltímetro (entrada de 1 GΩ), amperímetro (ideal; corrente entrando pelo +) |
| **Semicondutores** | Diodo (silício), Schottky, germânio, Zener, LED (acende conforme a corrente, cor selecionável), transistores NPN e PNP, MOSFETs canal N e canal P (corpo ligado à fonte) |
| **Modelos simplificados** | Modelos didáticos: diodo com tensão direta fixa; NPN e PNP com Vbe fixo, um beta mostrado no símbolo e uma tensão de saturação |
| **Circuitos integrados** | Amplificador operacional (comportamental ou modelo de fabricante, alimentado por V+ e V-); regulador 78xx (ajuste Vout); regulador de 3,3 V de baixo dropout; LM317 (OUT 1,25 V acima de ADJ); referência shunt TL431 (2,495 V entre REF e A); temporizador 555 |
| **Portas lógicas** | NOT, AND, OR, NAND, NOR, XOR (tipo 74HC, com alimentação oculta) |
| **CIs lógicos (DIP)** | Chips 74HC e CD4000 reais, com cada pino na posição do encapsulamento e VCC/GND para ligar: 74HC00, 74HC02, 74HC04, 74HC08, 74HC14 (Schmitt), 74HC32, 74HC86, 74HC74, 74HC138, 74HC157, 74HC245, 74HC161, 74HC193, 74HC165, CD4017 (o 74HC595 está em Chips para microcontroladores) |
| **Flip-flops e contadores** | Latches D e SR; flip-flops D, JK e T (borda de subida); contador de 4 bits, contador de décadas, registrador de deslocamento de 4 bits. Q̅ e CLR podem ficar desligados |
| **Displays** | Displays de 7 segmentos (catodo comum, anodo comum) com a pinagem de um display real; um display de 7 segmentos com 4 dígitos e uma matriz de LEDs 8x8 para multiplexação; um LCD de caracteres (HD44780, 1-4 linhas de 8-40 caracteres) que mostra o texto no esquemático e funciona com uma placa; um OLED SSD1306 de 128x64 ou 128x32 (I2C) que mostra sua imagem; uma fita de LEDs RGB WS2812B; decodificadores 4511 (CMOS, catodo comum) e 7447 (TTL, anodo comum, coletor aberto) com todos os pinos do chip; decodificadores hexadecimais genéricos 0-F |
| **Eletromecânicos** | Lâmpada (acende conforme a corrente), relé (bobina e contato reversível; acrescente um diodo de roda livre), motor CC (mostra a velocidade) |
| **Memórias** | ROM paralela e RAM estática de 16 bytes a 64 KB (A0-A15, D0-D7, CE, OE, WE), com ou sem placa; conteúdo de arquivos `.bin` ou Intel `.hex` e no Visualizador de memória |
| **Conversores** | Conversores A/D e D/A ideais de 4, 8, 10 ou 12 bits; a referência é um pino (VREF), e os níveis lógicos seguem uma alimentação oculta, como nas portas |
| **Microcontroladores** | Blue Pill (STM32F103C8) executando o seu firmware ELF |
| **Chips para microcontroladores** | Registrador de deslocamento 74HC595, driver de display de LED MAX7219, EEPROM 24C02, sensor de temperatura LM75, expansor de E/S PCF8574, sensor de pressão BMP280, acelerômetro e giroscópio MPU6050, relógio de tempo real DS3231, sensor de cor TCS34725 (I2C); conversor A/D MCP3208, potenciômetro digital MCP41010, flash W25Q32, cartão microSD (SPI); amplificador de célula de carga HX711; sensores DS18B20 (1-Wire), DHT22/DHT11 e HC-SR04; funcionam com a placa a que estão ligados, ou sozinhos (veja [MICROCONTROLLER.md](MICROCONTROLLER.md#chips-para-microcontroladores)) |
| **Sensores** | Sensor de temperatura LM35, termistor NTC, PT100/PT1000, termopar tipo K, LDR, fotodiodo BPW34, sensor de corrente ACS712, célula de carga (ponte de extensômetros), chave Hall A3144, optoacoplador PC817, solenoide |
| **Blocos** | Porta de bloco, para transformar um circuito em bloco |
| **E/S externa** | Entradas e saídas de bit e de palavra que trocam sinais com outro programa, como uma planta virtual (veja abaixo) |

Observações sobre alguns componentes:

- **Potenciômetro:** a seta no corpo indica o cursor, e a porcentagem ao
  lado (0 % no pino 1). Ajuste em Propriedades > Posição ou, durante a
  execução, arraste o potenciômetro ou gire a roda do mouse sobre ele.
- **Fontes ajustáveis:** durante a execução, arraste-as para cima (ou gire a
  roda do mouse para cima) para aumentar o valor e para baixo para
  diminuí-lo; o valor vai do limite inferior (0 %) ao limite superior
  (100 %) e aparece no símbolo.
- **Sensores:** a grandeza que um sensor mede (temperatura, luz, campo
  magnético, carga) é definida em Propriedades e, durante a execução,
  alterada arrastando o sensor para cima ou para baixo ou girando a roda
  sobre ele, dentro da faixa do sensor (luz em escala logarítmica, de
  0,1 lux a 100 mil lux); o símbolo a mostra. Os modelos usam os valores
  típicos das folhas de dados: LM35 10 mV/°C (alimentação de 4 a 30 V; 0 V
  abaixo de 0 °C, como no circuito básico), NTC pelo modelo beta, PT100 pela
  IEC 60751, tipo K pelas tabelas do NIST com a junta fria nos terminais,
  LDR como o GL5528 (sem tempo de resposta), BPW34 com cerca de 50 µA por
  1000 lux, ACS712 com VCC/2 + sensibilidade × corrente, célula de carga com
  saída = excitação × sensibilidade × carga / capacidade, A3144 com coletor
  aberto e pontos de operação e liberação (precisa de pull-up), PC817 com
  razão de transferência de corrente. Autoaquecimento, ruído e tempos de
  resposta ficam de fora. O êmbolo do solenoide aparece recolhido acima de
  60 % da corrente nominal e solto abaixo de 25 %; sua corrente é mostrada.
  O exemplo 36 mostra quatro deles.
- **Conversores A/D e D/A:** ideais e contínuos: o código do ADC é
  floor(2^N × VIN / VREF) a todo instante (sem clock, sem tempo de
  amostragem), e D0 é o bit menos significativo; o DAC fornece
  OUT = VREF × código / 2^N atrás de 10 Ω. As entradas comutam em VDD/2. O
  exemplo 31 mostra a escada de 4 bits.
- **Teclado 4x4:** durante a execução, segure o mouse sobre uma tecla: ela
  liga a sua linha (R1-R4) à sua coluna (C1-C4) até você soltar. Uma tecla
  por vez. O exemplo 30 faz a varredura dele com uma Blue Pill.
- **Fontes controladas:** os pinos de controle C+ e C- ficam à esquerda. Uma
  fonte controlada por tensão apenas sente a tensão entre eles (1 GΩ); uma
  fonte controlada por corrente sente a corrente que flui de C+ para C-, por
  isso os seus pinos de controle são um curto-circuito, como um amperímetro:
  ligue-os em série.
- **Displays de 7 segmentos e decodificadores:** um resistor por segmento;
  vários displays lado a lado formam um número (qualquer um dos pinos COM
  pode ser usado). No 4511, ligue LT e BI em nível alto e LE em nível baixo
  e conecte VDD/VSS; no 7447, conecte VCC e GND; RBI/RBO apagam os zeros à
  esquerda. Cada segmento acende conforme a sua própria corrente, com brilho
  total a 10 mA.
- **Temporizador 555:** os pinos e o comportamento do NE555: TRIG abaixo de
  1/3 de VCC inicia uma saída em nível alto, THR acima de 2/3 de VCC (CTRL)
  a encerra e descarrega DIS; ligue RESET a VCC (ele pode ficar aberto) e
  ponha 10 nF de CTRL para GND, como na folha de dados. Circuitos astáveis e
  monoestáveis dão os tempos da folha de dados (exemplo 34). O nível alto da
  saída é VCC menos 1,7 V (Propriedades: 0 para um 555 CMOS).
- **Reguladores:** a corrente de entrada é a corrente da carga mais alguns
  mA; com tensão de entrada insuficiente, a saída cai (dropout).
- **CIs lógicos (DIP):** desenhados como o encapsulamento visto de cima, com
  o pino 1 no alto à esquerda, ao lado do chanfro, para que a ligação seja a
  mesma da protoboard. Sem VCC e GND o chip não faz nada; a corrente das
  saídas vem do pino VCC. As portas, o 74HC138, o 74HC157, o 74HC245 e o
  74HC74 são SPICE comportamental (as entradas comutam em VCC/2, o 74HC14 em
  cerca de 0,53 e 0,31 VCC, então um oscilador RC roda na frequência real;
  saídas de 50 Ω). O 74HC161, o 74HC193, o 74HC165 e o CD4017 funcionam como
  chips digitais, como os chips para microcontroladores: instantâneos com uma
  placa ou outro chip, mas as entradas vindas do resto do circuito chegam até
  0,1 ms atrasadas (clocks de até alguns kHz). Pinos de portas não usadas
  podem ficar abertos; suas entradas leem nível baixo. O exemplo 37 é um
  sequencial de LEDs com um 74HC14 e um CD4017.
- **Portas lógicas e flip-flops** funcionam com uma alimentação oculta, por
  isso você liga apenas os seus pinos lógicos. As saídas são tensões reais:
  um LED numa saída precisa de um resistor em série.
- **Tipos de diodo:** cada um tem o seu próprio símbolo e lista os modelos
  do seu tipo.
- **E/S externa:** entradas e saídas de bit e de palavra trocam seus sinais
  com outro programa enquanto a simulação roda, por exemplo uma planta
  industrial virtual. Cada componente tem um endereço, enviado exatamente
  como escrito (`%IX0.10`, `%QW2`). Uma entrada de bit aciona o seu pino com
  0 V ou com o seu nível alto (3,3 V); uma entrada de palavra aciona uma
  tensão escalonada a partir da sua faixa de palavras (0–1000 corresponde a
  0–3,3 V por padrão; Propriedades altera as duas faixas). Uma saída de bit
  envia se o seu pino está acima do limiar, e uma saída de palavra envia a
  tensão do seu pino escalonada para a faixa de palavras. A polaridade
  *invertida* troca verdadeiro e falso. As entradas podem ser clicadas ou
  arrastadas à mão enquanto nada estiver conectado. O circuito precisa de um
  terra. **Simular > E/S externa** (só aparece em projetos com esses
  componentes) define o host do servidor, a porta (127.0.0.1:8765 por
  padrão) e o período (20 ms). A cada período o SimNodus envia um objeto
  JSON por linha com todas as saídas (`{"%QX0.0": true, "%QW1": 512}`) e
  aplica as entradas com que o servidor responde. Pausar ou parar envia
  false e 0 para todas as saídas, porque esses programas mantêm o último
  comando. O link funciona em tempo real, por isso os resultados não são
  repetíveis; execuções de agentes sem a janela não o usam.

## Modelos de dispositivos

Selecione um transistor, MOSFET, diodo ou amplificador operacional:
**Propriedades > Modelo** lista os componentes internos (2N2222A, BC547B,
1N4007, 1N5819, IRF540N, ...) e os modelos das suas pastas de modelos. Os
modelos internos são aproximações do SimNodus de valores típicos de folhas
de dados, feitas para o ensino.

**Seus próprios modelos (Ferramentas > Pastas de modelos):** liste as pastas
que contêm arquivos `.lib`, por exemplo copiados do site de um fabricante. A
pasta padrão é `Documents/SimNodus/models`; é possível acrescentar quantas
pastas quiser.

- Linhas `.model` aparecem nos componentes do tipo correspondente (modelos
  de diodo marcados com `type=` aparecem no tipo de diodo correspondente;
  exemplo 19).
- Blocos `.SUBCKT` (por exemplo um modelo de LM358, TL072 ou TIP120)
  aparecem em amplificadores operacionais, transistores, MOSFETs e diodos.
  **Pinos...** define qual porta do subcircuito corresponde a qual pino; os
  comentários do arquivo costumam indicar a ordem.
- **Inserir > Subcircuito...** insere qualquer `.SUBCKT` das pastas como um
  componente próprio (um LM555, um ULN2003, um amplificador de áudio...): uma
  caixa com um pino por porta, a primeira metade descendo pelo lado esquerdo
  e o restante subindo pelo lado direito, como no chip, com os nomes do
  arquivo. O exemplo 35 usa um driver Darlington de quatro canais de
  `examples/models/darlington_array.lib`. Subcircuitos com chaves ou modelos
  digitais (XSPICE) podem não manter o seu estado interno entre os segmentos
  de 1 s de uma execução interativa; o painel Mensagens avisa, e uma
  execução transitória é exata.
- Um modelo escolhido é copiado para o projeto, então o projeto abre em
  outros computadores sem a pasta.
- Linhas ou parâmetros que o SimNodus não consegue usar são listados no
  painel Mensagens.

## Barramentos e distribuidores

Pressione **B** e desenhe um barramento como um fio; um clique duplo o
termina. Dê um nome a ele com um rótulo de rede, por exemplo `D`. O bit 3 do
barramento `D` é a rede `D3`.

Pressione **S** para inserir um distribuidor e informe as larguras dos seus
pinos de cada lado, como no simulador *Digital*: `8` à esquerda e `8*1` à
direita separa um barramento de 8 bits em fios; `1,1,1,1` e `4` juntam
quatro fios num barramento; `4` e `2,2` o dividem em dois barramentos.
Distribuidores do mesmo barramento ligam os bits iguais. Exemplo 18.

## Blocos

Um bloco é um circuito que você desenhou uma vez e insere muitas vezes,
mostrado como um retângulo com pinos.

1. Desenhe o estágio num projeto próprio e marque as suas conexões com
   componentes **Porta de bloco** (Blocos, na árvore): dê a cada uma um nome
   (IN, OUT, VCC, ...) e o lado do retângulo em que o pino fica. Salve-o.
2. Em outro projeto, escolha **Inserir > Bloco...** e selecione esse
   arquivo. Insira-o quantas vezes precisar.
3. O projeto guarda uma cópia do bloco. Clique duas vezes num bloco para
   alterar o seu arquivo; depois de editar o projeto do bloco, clique em
   **Atualizar** em Propriedades (ou reabra o projeto) para usar a nova
   versão.

O exemplo 20 usa `examples/blocks/RC_lowpass.snx` duas vezes.

## Executando uma simulação

| Tecla | Ação |
|---|---|
| **F5** | Executar |
| **F6** | Pausar e continuar (o botão Pausar fica pressionado enquanto está pausado) |
| **Shift+F5** | Parar |

Antes de uma execução, o SimNodus verifica o circuito (terra, pinos
desligados, valores inválidos) e lista os problemas no painel Mensagens.

Durante a execução:

- Clique numa **chave** para alterná-la; segure o mouse sobre um **botão**;
  arraste ou use a roda num **potenciômetro**.
- LEDs, lâmpadas e segmentos de displays acendem conforme a corrente, com a
  média dos últimos 20 ms, como o olho vê um display multiplexado, e com o
  brilho que o olho perceberia (um LED com poucos miliamperes parece
  claramente aceso); os medidores mostram a leitura; o contato do relé se
  move; o motor mostra a velocidade.
- Passe o mouse sobre um fio para ver a sua tensão. **Exibir > Mostrar
  tensões** colore os fios.
- Selecione um componente para ver as suas tensões, corrente e potência ao
  vivo em Propriedades.
- A **caixa de velocidade** define a velocidade em relação ao tempo real
  (0,001x a 1x, ou Máxima). A barra de status mostra a velocidade
  efetivamente alcançada.
- Alterar o circuito pede para parar antes; mudanças de valores valem na
  próxima execução.

### Configurações da simulação

**Simular > Configurações da simulação** escolhe o modo. Os dois modos
começam com capacitores descarregados e indutores sem corrente.

| Modo | Uso | Configurações |
|---|---|---|
| **Interativo** (padrão) | Execução contínua com a qual você pode interagir | Passo de tempo máximo: 100 µs (padrão), 10 µs (sinais rápidos, mais CPU) ou 1 ms (circuitos lentos). Duração do segmento de 0,5 a 2 s (o motor recomeça a cada segmento para limitar a memória). *Bordas exatas do microcontrolador* (veja abaixo) |
| **Transitório** | Um intervalo de tempo fixo, calculado por completo e depois aberto no analisador | Tempo final e passo de tempo máximo |

As bordas de chaves e geradores são resolvidas no passo de tempo. Com um
microcontrolador, **Bordas exatas do microcontrolador** dá ao circuito um
ponto de tempo em cada borda do firmware, para que PWM e outras ondas
quadradas fiquem exatas no osciloscópio; as execuções ficam mais lentas.

## Analisador de sinais

**Janela > Analisador de sinais** (Ctrl+Shift+A) o abre numa janela
separada, que pode ficar em outro monitor. Ele tem três abas: Osciloscópio,
Lógico e UART.

### Pontas de prova

Pressione **P** e clique:

| Clique | Mede |
|---|---|
| Num fio | A tensão dele |
| Num pino de componente | A corrente que entra nesse pino |
| Shift+clique num pino | A tensão do pino |
| Alt+clique numa segunda rede | A última ponta de tensão em relação a essa rede (diferencial) |

As pontas de prova são salvas com o projeto. Para mover uma, arraste o seu
marcador com a ferramenta de seleção até outro fio ou pino: ela mantém a cor
e o canal (uma ponta de corrente precisa de um pino). Ctrl+Z desfaz a
movimentação.

### Osciloscópio

- **Base de tempo**, modo **Rolagem** ou **Disparado**. O modo Disparado tem
  fonte, nível e borda, e varredura Automático, Normal ou Único. O ponto de
  disparo é marcado com uma bandeira T e uma linha pontilhada; a tela mostra
  *Disparado em ...*, *Aguardando disparo* ou *Automático: sem disparo,
  correndo livre*.
- **Vertical:** escala por canal ou **Escala automática**. A borda esquerda
  mostra os volts por divisão do canal selecionado e um triângulo colorido
  no 0 V de cada canal; arraste um triângulo (ou gire a roda sobre a escala)
  para deslocar o canal, e clique duas vezes nele para centralizar o 0 V de
  novo.
- Os **Cursores** A e B mostram Δt, 1/Δt e os valores. A linha abaixo do
  gráfico mostra mínimo, máximo, média, RMS, pico a pico, frequência e ciclo
  de trabalho do canal selecionado.
- **Resultados transitórios** abrem como um registro completo: a roda aplica
  zoom no eixo do tempo, e arrastar move a vista.
- **Exportar CSV** grava os canais visíveis (a primeira linha diz como os
  dados foram calculados); **Exportar PNG** salva o gráfico.

### Lógico

Os pinos do microcontrolador aparecem sozinhos como faixas quando mudam.
Marque ou desmarque qualquer pino ou canal de tensão na lista; os canais de
tensão leem 1 acima do **Limiar das pontas**. As faixas seguem a base de
tempo do osciloscópio, e os cursores A e B (compartilhados com o
osciloscópio) mostram as suas posições, a distância e a frequência. Passe o
mouse sobre uma borda para ver o instante exato; clique numa faixa para
listar as suas transições. **Exportar VCD...** grava as faixas marcadas para
o PulseView ou o GTKWave.

### UART

Escolha a porta (USART1 em PA9/PA10, USART2 em PA2/PA3, USART3 em
PB10/PB11) para ler o que o firmware imprime. Digite uma linha e pressione
Enter ou Enviar, com o fim de linha que preferir. **Marcas de tempo** começa
cada linha com o seu tempo simulado.

## Microcontrolador

1. Insira **Blue Pill (STM32F103C8)**, da categoria Microcontroladores, e
   ligue um dos seus pinos **G** ao terra.
2. Em Propriedades, **Procurar...** escolhe o firmware: um `.elf` compilado
   para o STM32F103C8, por exemplo pelo STM32CubeIDE (projetos HAL com a
   `SystemClock_Config()` gerada rodam sem alterações). O caminho é guardado
   em relação ao projeto. **Recarregar** usa um arquivo recompilado e
   reinicia uma simulação em execução.
3. **F5** inicia o firmware no emulador de STM32 do próprio SimNodus.

Durante a execução, cada pino GPIO mostra o seu estado: vermelho = saída em
nível alto, azul = saída em nível baixo, contorno pontilhado = função
alternativa, vazado = entrada (a borda fica vermelha quando lê 1). O LED de
PC13 acende quando PC13 está em nível baixo; clique em **RESET** para
reiniciar o firmware.

- As saídas acionam o circuito como pinos reais: push-pull de 3,3 V ou 0 V
  atrás de cerca de 40 Ω; open-drain só puxa para baixo; os pull-ups e
  pull-downs são de 40 kΩ.
- As entradas leem 1 acima de 1,83 V e 0 abaixo de 1,16 V (Schmitt
  trigger).
- Suportados: GPIO, EXTI, USART1-3, SPI1/2 e I2C1/2 (mestre), TIM1-TIM4
  (PWM, comparação de saída, captura de entrada), ADC1/2, DMA1, SysTick, a
  árvore de clock até 72 MHz, os watchdogs independente e de janela, o RTC
  com os seus registradores de backup e o CRC. Duas placas podem se
  comunicar pelos seus pinos seriais.
- **Depuração:** marque **Simular > Depurar firmware com GDB (porta 3333)**,
  pressione F5 e conecte o STM32CubeIDE ou o arm-none-eabi-gdb a
  `localhost:3333`. Enquanto o firmware está parado num breakpoint, o
  circuito fica parado.

**Janela > Visualizador de memória** (Ctrl+Shift+M) mostra, carrega e salva
o conteúdo dos chips de memória (a EEPROM 24C02, ROM e RAM), ao vivo durante
a execução ([detalhes](MICROCONTROLLER.md#visualizador-de-memória)).

Instruções passo a passo, a lista completa de periféricos e todas as
diferenças em relação a uma placa real estão em
[MICROCONTROLLER.md](MICROCONTROLLER.md).

## Arquivos, exemplos e aulas

- Os projetos são arquivos `.snx`: Arquivo > Salvar (Ctrl+S), Salvar como
  (Ctrl+Shift+S), Abrir (Ctrl+O), Abrir recente. Ao salvar, a versão
  anterior é mantida como `.snx.bak`.
- **Arquivo > Salvar com firmware...** também copia os arquivos que o
  projeto usa (o `.elf` de cada placa, arquivos de conteúdo de memórias)
  para uma pasta `<projeto>_files` ao lado do `.snx` e faz as peças
  apontarem para as cópias. Copie o projeto junto com essa pasta e ele abre
  e roda em outro computador (o projeto de um aluno no PC do professor).
- Alterações não salvas são gravadas num **arquivo de recuperação** a cada
  minuto. Se o SimNodus fechar inesperadamente, a próxima abertura oferece
  recuperá-las.
- **Arquivo > Exemplos** abre os circuitos incluídos como cópias sem título,
  então os originais ficam inalterados. A
  [lista de exemplos](../../../examples/README.md) diz o que cada um mostra e
  o que experimentar: circuitos analógicos básicos (01-06), firmware para a
  Blue Pill (07-09, 12-15), lógica e barramentos (17, 18), tipos de diodo
  (19), blocos (20), um contador (21), reguladores (22), um contador de 7
  segmentos (23), o RTC com um watchdog (24), um registrador de deslocamento
  74HC595 (25), uma matriz de LEDs 8x8 num MAX7219 via SPI (26), três chips
  I2C (27) e um LCD 16x2 atrás de um PCF8574 (28).
- As **aulas** 1-3 (exemplos 10, 11 e 16) vêm com notas em
  `examples/lessons/`: objetivos, passos e as observações esperadas. Os
  fontes dos seus firmwares estão em `examples/firmware/`.
- Projetos que contêm blocos são salvos no formato versão 2, que o SimNodus
  0.3 não consegue abrir; os demais projetos continuam legíveis pela 0.3.

## Preferências

| Menu | Configuração |
|---|---|
| Exibir > Tema | Sistema (segue o Windows, padrão), Claro ou Escuro; aplicado na hora |
| Ferramentas > Idioma | Idioma do sistema (padrão), English ou Português (Brasil); vale a partir da próxima abertura |
| Ferramentas > Pastas de modelos... | Pastas com os seus arquivos de modelo `.lib` |
| Ferramentas > Verificar motor de simulação | Carrega o motor analógico e resolve um circuito de referência |
| Ferramentas > Permitir controle externo | Agentes de IA e scripts neste computador podem editar e executar o circuito nesta janela (`simnodus-cli connect`, veja AUTOMATION.md); desligado a cada abertura |
| Exibir > Componentes / Propriedades / Mensagens | Mostra ou oculta os painéis (Ctrl+1/2/3); a disposição é lembrada |

## Linha de comando

| Comando | Efeito |
|---|---|
| `simnodus.exe FILE.snx` | Abre um projeto |
| `simnodus.exe FILE.snx --run` | Abre e inicia a simulação |
| `simnodus.exe --control` | Inicia com o controle externo ligado (Ferramentas > Permitir controle externo) |
| `simnodus-cli.exe mcp [--window]` | Servidor MCP para agentes de IA como o Claude Code ou o Codex (veja AUTOMATION.md) |
| `simnodus.exe --check-engine` | Carrega o ngspice e resolve um circuito de referência |
| `simnodus.exe --check-mcu-engine` | Executa `blink.elf` no emulador do microcontrolador |
| `simnodus.exe --smoke-test` | Abre as janelas, executa as verificações internas e sai |
| `simnodus.exe --settings FILE` | Usa FILE em vez das configurações do usuário |
| `simnodus-cli.exe ...` | O SimNodus sem janela, para agentes de IA e scripts: monta, verifica, simula e mede circuitos com comandos JSON (veja AUTOMATION.md) |

## Solução de problemas

| Problema | O que fazer |
|---|---|
| A execução não começa | Leia o painel Mensagens: normalmente falta o terra, há um pino desligado ou um valor inválido. Clique duas vezes na mensagem para encontrar o componente |
| O motor informa que o circuito não convergiu | Procure nós flutuantes (um capacitor sozinho entre duas redes, uma porta sem caminho para o terra), fontes ideais em paralelo ou uma chave pondo uma fonte em curto. Experimente um passo de tempo máximo menor |
| A barra de status mostra uma velocidade abaixo de 1x | O circuito exige mais cálculo do que o tempo real permite. Use um passo de 100 µs ou maior, desligue *Bordas exatas do microcontrolador* se não precisar dela, ou reduza a velocidade |
| Um PWM parece uma rampa ou um quadro UART não aparece no osciloscópio | As bordas são calculadas pela média no passo de tempo: ligue *Bordas exatas do microcontrolador*, use um passo de 10 µs ou olhe a aba Lógico |
| O firmware não faz nada | Verifique se um pino G está no terra, se o ELF foi compilado para o STM32F103C8 e se o painel Mensagens aponta periféricos não suportados |
| A placa fica esperando para sempre | *Depurar firmware com GDB* está marcado: conecte um depurador ou desmarque a opção |
| Falta um modelo da minha pasta | Verifique Ferramentas > Pastas de modelos e o painel Mensagens; o modelo precisa corresponder ao tipo do componente (um modelo de diodo num diodo) |
| O motor não é encontrado ou "parou inesperadamente" | Ferramentas > Verificar motor de simulação; reinstale se `simnodus-engine.exe` ou a pasta `ngspice` ao lado de `simnodus.exe` estiver faltando. Se um circuito para o motor toda vez, por favor relate o problema enviando o arquivo do projeto |

## Limitações conhecidas

O SimNodus é uma ferramenta de ensino. Estes limites são conhecidos e
declarados para que os resultados sejam lidos corretamente.

### Simulação de circuitos

- Os modelos de dispositivos são aproximações de valores típicos, não
  reproduções exatas das folhas de dados. Chaves abertas valem 10^15 Ω, o
  voltímetro tem entrada de 1 GΩ e o amperímetro é ideal; os LEDs não têm
  limite de corrente, então um LED sem resistor consome uma corrente
  irreal.
- Execuções interativas são calculadas em segmentos (1 s por padrão). A
  carga das junções dos semicondutores não passa de um segmento para o
  seguinte, então um pequeno degrau pode aparecer uma vez por segmento em
  circuitos que dependem dela. As tensões dos capacitores e as correntes dos
  indutores são preservadas.
- Eventos (um clique, uma chave, uma borda do firmware) são resolvidos no
  passo de tempo máximo, a menos que *Bordas exatas do microcontrolador*
  esteja ligada.
- No Windows, o acompanhamento do tempo real pode atrasar até cerca de 25 ms
  (granularidade do temporizador); o tempo simulado em si não é afetado.
- Não há análise CA (varredura em frequência), de ruído nem FFT; use os
  modos transitório e interativo.
- Portas lógicas, flip-flops e contadores são modelos comportamentais com
  alimentação oculta, não modelos em nível de transistor de um chip 74HC.

### Microcontrolador

- Só a Blue Pill (STM32F103C8) está disponível.
- Ainda não simulados: modos escravo de SPI e I2C, CAN, USB; modos encoder
  e alinhado ao centro dos timers, tempo morto e break; conversões do ADC
  disparadas por timer ou EXTI, modos dual e descontínuo. O painel
  Mensagens avisa quando o firmware usa um dos recursos com suporte parcial.
- Uma instrução por ciclo de clock: laços de espera ocupada rodam mais
  rápido que na placa; código temporizado com SysTick, timers ou
  `HAL_Delay()` fica correto. Os periféricos funcionam mesmo que o
  respectivo clock enable nunca tenha sido ligado.
- Firmware e circuito são acoplados a cada 100 µs: mudanças no circuito
  chegam ao firmware com até 0,1 ms de atraso, e o ADC lê a tensão do pino
  de 0,1–0,2 ms antes do instante da amostra.
- Bordas exatas do microcontrolador custam velocidade (cerca de 0,8x–2,9x o
  tempo real com passo de 100 µs; cerca de 0,2x a 10 µs com um PWM de
  10 kHz, num notebook de 2026).
- Na velocidade máxima, num notebook de 2026, a maioria dos exemplos roda
  de 1,5x a 50x o tempo real, mas quatro ficam perto ou abaixo dele: 26
  (matriz de LEDs, 64 LEDs multiplexados) cerca de 0,4x, 24 (RTC e watchdog)
  e 15 (ADC + DMA com HAL) cerca de 1x (firmware ocupado a 72 MHz), 23
  (contador de 7 segmentos) 1,2x. Em PCs mais lentos eles mostram uma
  velocidade abaixo de 1x: os resultados continuam certos, só mais lentos.
- A recepção (RX) da USART pelo pino precisa de bordas exatas ou de um bit
  com pelo menos quatro passos de tempo (2400 baud ou menos a 100 µs); um
  byte recebido chega ao firmware cerca de 0,4 ms depois do seu stop bit.
- O modo transitório não executa microcontroladores: use o modo interativo.
- Chips para microcontroladores só funcionam junto com uma placa e leem os
  níveis digitalmente (um chip de 5 V aceita níveis altos de 3,3 V); os seus
  pinos em redes sem pino de placa leem o circuito com até 0,1 ms de atraso.

### Aplicativo

- Por enquanto, só Windows; um pacote para Linux está planejado (o código
  compila no Linux).
- A simulação roda num programa separado (`simnodus-engine`): se ele falhar,
  a execução para com uma mensagem, e o SimNodus e o seu projeto continuam
  abertos; pressione F5 para executar de novo.
- Os resistores usam o retângulo IEC; uma variante em zigue-zague ANSI e um
  editor de símbolos ainda não estão disponíveis.
