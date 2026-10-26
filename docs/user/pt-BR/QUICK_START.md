<!-- source: QUICK_START.md sha256 ea1cda6f515037c1227212b212ad2641c36d3bb92a453b1a09576bf0d6570286 -->
# Início rápido do SimNodus

Cinco minutos da instalação até um circuito funcionando. Todo o resto está no
[guia do usuário](USER_GUIDE.md) (Ajuda > Guia do usuário), incluindo suas
[limitações conhecidas](USER_GUIDE.md#limitações-conhecidas).

## 1. Instalar

Execute `SimNodus-<version>-windows-x64-setup.exe` (só para você, sem
direitos de administrador, ou para todos os usuários), ou descompacte o pacote
portátil e execute `simnodus.exe`. As duas formas funcionam sem internet.

## 2. Abrir um exemplo

Arquivo > Exemplos > **01 LED and push button** e pressione **F5**. Mantenha o
botão do mouse pressionado sobre o botão do circuito: o LED acende. Para parar,
use Shift+F5.

## 3. Desenhar seu próprio circuito

Um LED alimentado por uma fonte de 5 V:

1. Em **Componentes** (à esquerda), procure *Fonte de tensão CC* e clique no
   esquemático para colocá-la; Esc encerra a inserção. Faça o mesmo com um
   **Resistor** (tecla R), um **LED** e um **Terra** (tecla G).
2. Selecione o resistor, digite `330` em **Propriedades** > Resistência e
   pressione Enter. Os valores aceitam `4k7`, `100n`, `2.2u`, `1meg`.
3. Pressione **W** e clique de pino em pino: o + da fonte ao resistor, o
   resistor ao anodo do LED, o catodo do LED e o − da fonte ao terra.
   Um círculo vermelho marca um pino que ainda está desconectado.
4. Pressione **F5**. O LED acende; selecione-o para ver sua corrente em
   Propriedades, ou passe o mouse sobre um fio para ver sua tensão.

## 4. Observar os sinais

Pressione **P** e clique num fio: é uma ponta de prova de tensão. Janela >
Analisador de sinais (Ctrl+Shift+A) mostra o sinal no osciloscópio; experimente
um gerador de pulsos no lugar da fonte CC (exemplo 02).

## 5. Executar um firmware

Arquivo > Exemplos > **07 Blue Pill blink** e depois F5: o LED da placa em PC13
e o LED em PA0 piscam. No seu próprio circuito, coloque uma
**Blue Pill (STM32F103C8)**, ligue um pino G ao terra e escolha seu `.elf` com
**Procurar...** em Propriedades. Veja a
[página do microcontrolador](MICROCONTROLLER.md).

## Teclas para lembrar

| Tecla | Ação |
|---|---|
| F1 | Todos os atalhos |
| F5 / F6 / Shift+F5 | Executar / pausar / parar |
| W, N, P, T | Fio, rótulo de rede, ponta de prova, nota de texto |
| R, C, L, D, G | Resistor, capacitor, indutor, diodo, terra |
| V, I, Shift+V, Shift+I | Fonte de tensão, fonte de corrente, voltímetro, amperímetro |
| Ctrl+R, Ctrl+E | Girar, espelhar |
| F | Enquadrar o circuito na janela |
