# Robô Tic-Tac-Toe (Project 2)

***

## Base do Jogo

### Visão computacional (Nicla Vision)
- Câmara captura a grelha a partir do topo (posição fixa e repetível)
- Grelha é **preto e branco** → deteção por luminosidade
  - **Threshold adaptativo** (preferencial): ajusta automaticamente ao nível de iluminação ambiente
  - **Fallback para threshold fixo** se o adaptativo não for viável na Nicla Vision / OpenMV
- **Capturar imagem apenas com o braço em posição levantada** — evitar sombras do end-effector sobre a grelha
- Detetar os contornos da grelha e dividi-la nas 9 secções
- Descobrir o centro de cada secção → coordenadas usadas na lookup table
- Reconhecer o estado de cada célula:
  - Deteção de contornos (círculo = O, cruz = X, vazio = fundo branco)
  - Limiar de confiança para evitar falsos positivos

### Lógica de jogo — Opção A (Nicla Vision)
- **Min-max corre inteiramente na Nicla Vision** (MicroPython/OpenMV)
- Nicla processa imagem → determina estado da grelha → corre min-max → envia índice da célula ao Nano via BLE
- Nano é exclusivamente responsável pelo controlo dos servos
- Separação clara de responsabilidades: visão + IA na Nicla, atuação no Nano

### Comunicação BLE
- Nicla envia ao Nano o **índice da célula** escolhida (0–8)
- Protocolo simples: 1 byte por mensagem, acknowledge de volta
- O Nano faz o mapeamento célula → ângulos de servo (lookup table hardcoded)

***

## Calibração Inicial (interativa com o utilizador)

- Processo único feito antes do primeiro jogo (ou quando a grelha é repositicionada)
- A **posição de referência da grelha no stand está marcada fisicamente** — o utilizador coloca a grelha sempre no mesmo sítio
- A calibração é conduzida pela **CLI** em modo interativo:

### Fluxo de calibração
1. CLI: `"Modo de calibração — coloca a grelha na posição marcada e pressiona [Enter]"`
2. Câmara captura imagem → Nicla deteta os 4 cantos da grelha e as 9 células
3. CLI mostra o resultado da deteção (células identificadas): `"Detetadas 9 células. Confirmar? (s/n)"`
4. Se confirmado: Nicla guarda os centros das células para o min-max
5. CLI: `"Move o braço manualmente para o centro da célula 0 e pressiona [Enter]"`  
   → Repetir para as 9 células (ou para um subconjunto representativo + interpolação)
6. Nano regista os ângulos de cada posição → gera a lookup table hardcoded (requer reflash ou armazenamento temporário durante a sessão)
7. CLI confirma: `"Calibração concluída. Pronto para jogar."`

***

## Controlo do Braço (Nano 33 BLE)

### Informação persistente hardcoded
- Toda a informação persistente fica **hardcoded no Nano**:
  - Lookup table: índice de célula (0–8) → ângulos de servo (base, shoulder, elbow)
  - Limites de segurança de cada articulação
  - Posição de homing / espera
  - Altura de lowering e lifting para o end-effector
  - Sequência de ângulos para traçar a linha de vitória (horizontal, vertical, diagonal)

### Homing sequence (ao arrancar)
- Ao ligar, os servos SG90 não têm memória da posição anterior
- Sequência de homing: mover cada servo incrementalmente para a posição zero hardcoded
- O braço só aceita comandos de jogo depois de concluído o homing
- CLI confirma quando o homing está completo

### Controlo incremental dos servos (proteger engrenagens)
- **Nunca** comandar saltos grandes (ex: 20° → 160° numa instrução) — destrói as engrenagens de plástico
- Mover em passos de ~1° com frequência controlada
- Usar `millis()` (Arduino) — **nunca `delay()`** — para não bloquear a comunicação BLE e a CLI
- Limites de software para cada articulação definidos na lookup table hardcoded

### Mapeamento célula → ângulos de servo
- Cada célula (0–8) tem ângulos pré-calculados (base, shoulder, elbow) obtidos na calibração
- Calibração feita uma única vez (câmara e braço estacionários; grelha na posição marcada)

### Sequência de desenho (lowering / lifting)
1. Confirmar braço em posição levantada (segura)
2. Mover horizontalmente até à posição XY da célula destino
3. **Baixar** o end-effector até fazer contacto com o papel
4. Executar o traço do símbolo (O ou X) com medidas definidas em relação ao centro da célula
5. **Levantar** o braço até posição segura
6. Regressar à posição de homing / espera
- **Nunca** arrastar a caneta entre células

### Linha de vitória (sempre executada)
- **Independentemente de quem ganhar**, o braço desenha sempre a linha de vitória no final
- A linha de vitória é uma sequência de movimentos hardcoded para cada um dos 8 padrões possíveis:
  - 3 horizontais, 3 verticais, 2 diagonais
- Sequência: levantar → mover para início da linha → baixar → traçar linha → levantar → homing
- Se ganhar o utilizador, o robô desenha igualmente a linha como gesto final do jogo

***

## Interface com o Utilizador (CLI no Laptop)

- O Nano, ao ser ligado ao laptop via USB, expõe uma **app terminal (CLI)** pela porta série
- Toda a interação com o utilizador é feita através desta CLI
- **O jogo não avança sem input explícito do utilizador** em cada passo relevante

### Fluxo de inputs da CLI

| Momento | Input esperado | Ação |
|---|---|---|
| Arranque | `[Enter]` para confirmar homing | Inicia sequência de homing |
| Pós-homing | `c` calibrar / `j` jogar | Entra no modo de calibração ou inicia jogo direto |
| Calibração | `[Enter]` passo a passo | Conduz o utilizador célula a célula |
| Início do jogo | `1` / `2` — jogar primeiro ou segundo | Define quem é X e quem é O |
| Turno do utilizador | `[Enter]` após fazer a jogada na grelha | Dispara captura de imagem + validação |
| Jogada inválida | `[Enter]` para tentar de novo | Câmara captura novamente |
| Fim do jogo | `r` reiniciar / `q` sair | Reset do estado interno |

***

## Fluxo de Jogo

### Início
1. Nano liga ao laptop → CLI inicia automaticamente pela porta série
2. CLI pede confirmação para executar homing → `[Enter]`
3. Braço executa homing para posição de referência hardcoded
4. CLI oferece opção de calibrar ou jogar diretamente
5. CLI pergunta: `"Queres jogar primeiro? (1 = Sim / 2 = Não)"`
   - **1 (Sim)**: utilizador é X, robô é O
   - **2 (Não)**: robô é X e joga primeiro
6. CLI confirma grelha vazia antes de começar

### Turno do utilizador
1. CLI: `"A tua vez — faz a tua jogada e pressiona [Enter]"`
2. Utilizador coloca a peça e pressiona `[Enter]`
3. **Braço levanta para posição segura** → câmara captura imagem sem sombras
4. Nicla deteta nova célula preenchida → valida jogada
5. Se inválida: CLI avisa e pede para repetir
6. Se válida: CLI mostra estado atual e passa ao turno do robô

### Turno do robô
1. Nicla corre min-max → determina melhor célula
2. CLI anuncia: `"Robô vai jogar na célula X"`
3. Nicla envia índice via BLE ao Nano
4. Nano executa: homing → mover → baixar caneta → desenhar → levantar → posição de espera
5. CLI confirma jogada concluída

### Deteção de fim de jogo
- Verificar vitória / empate via estado interno do min-max (na Nicla)
- Confirmar visualmente com câmara
- CLI anuncia resultado

***

## Fim do Jogo

- CLI anuncia resultado com mensagem formatada
- **Braço desenha sempre a linha de vitória** — independente de quem ganhou
  - Sequência hardcoded para o padrão vencedor (horizontal / vertical / diagonal)
- CLI pergunta: `"Jogar de novo? (r = reiniciar / q = sair)"`
- Reset do estado interno → pronto para nova partida sem reiniciar o hardware

***

## Segurança (Safety Constraints)

- Limites de range de cada servo hardcoded no Nano
- Imagem só é capturada com o braço em posição levantada
- Antes de baixar a caneta, confirmar que o braço está sobre a célula correta
- Levantar sempre antes de transitar entre células
- Erro detetado → braço regressa a posição segura e CLI notifica o utilizador
- Alimentação externa 5V para os servos (não usar pinos do Arduino diretamente)
- Ground comum entre fonte externa e microcontrolador

| Articulação | Range Software | Observação |
|---|---|---|
| Base | 0° – 180° | Centro definido na calibração |
| Shoulder | 0° – 180° | 0° = posição mais baixa |
| Elbow | 0° – 180° | 180° = posição mais baixa (montado invertido) |

***

## Arquitetura do Sistema (resumo)

```
[Laptop — CLI via USB série]
  → input do utilizador em cada passo
  → mostra estado do jogo e calibração interativa
        ↓ (porta série)
[Nano 33 BLE]
  → lookup table hardcoded (célula → ângulos, linha de vitória)
  → limites de segurança hardcoded
  → recebe célula via BLE
  → executa movimento incremental com millis()
  → sequência: homing → levantar → mover → baixar → desenhar → levantar → espera
        ↑ BLE
[Nicla Vision — câmara estacionária]
  → captura imagem apenas com braço levantado
  → threshold adaptativo (fallback: fixo) para detetar X / O / vazio em P&B
  → corre algoritmo min-max (Opção A)
  → envia índice da célula ao Nano
```

***

## Questões em Aberto / A Investigar

- [ ] Viabilidade do threshold adaptativo no OpenMV — verificar funções disponíveis (`image.get_statistics`, `find_threshold`)
- [ ] Processo de calibração: guiar o utilizador célula a célula é mais preciso mas demorado — avaliar se 4 cantos + interpolação é suficiente
- [ ] Definir os 8 padrões de linha de vitória e respetivas sequências de ângulos para o braço
- [ ] Testar range seguro de cada servo antes de definir os limites hardcoded
- [ ] Sinalização ao utilizador durante a linha de vitória: pausa na CLI até o braço terminar

***

## Fazes do projeto

### Fase 1 — Nano + Servos 
É o componente mais crítico e mais físico. Se o braço não funcionar de forma segura, nada mais funciona. A ordem dentro desta fase:

1. **Testar cada servo individualmente** — varrer o range completo em passos de 1° com `millis()`, confirmar limites físicos reais antes de hardcodar
2. **Implementar o homing sequence** — posição de referência segura ao arrancar
3. **Implementar a lookup table** com as 9 células (valores placeholder por agora) e a lógica de lowering/lifting
4. **Implementar a sequência da linha de vitória** para os 8 padrões


### Fase 2 — CLI no Laptop
Com o braço a funcionar, adicionar a CLI pela porta série é simples e permite já **controlar o braço manualmente a partir do terminal** — útil para testar e calibrar:
- Input de célula destino → braço move
- Comando de homing
- Modo de calibração interativa (registar ângulos célula a célula)


### Fase 3 — Nicla Vision
Independente do Nano. Testar isoladamente:
1. Captura de imagem e deteção da grelha
2. Threshold adaptativo → identificar X / O / vazio
3. Algoritmo min-max em MicroPython


### Fase 4 — BLE (integração)
Ligar as duas fases anteriores. Nicla envia célula → Nano executa. Testar o handshake antes de integrar o jogo completo.


### Fase 5 — Calibração + Jogo completo
Com tudo a funcionar separadamente, correr o processo de calibração interativa para gerar a lookup table final e testar o fluxo de jogo de ponta a ponta.

