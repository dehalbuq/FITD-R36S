# FITD Wii — Teste 04: animações LISTANIM e geometria BODY no GX

Esta versão é um **teste técnico nativo para Nintendo Wii / PowerPC**.
**Ainda NÃO é a versão jogável de Alone in the Dark.**

A versão Wii Teste 03.1 está preservada em sua própria branch.
O Android Teste 19 e o R36S ARM64 não foram modificados, e seus patches
originais continuam na ancestralidade desta branch. O checklist das
correções está em COMPAT_TEST19.md.

## Novidades reais

- Decodificação dos quadros e grupos de LISTANIM.PAK a partir do formato
  original do motor FITD: quadro, duração em ticks, deslocamento da raiz,
  tipos de transformação, rotação e translação dos grupos.
- Suporte a duas variantes binárias (8 e 16 bytes por grupo), com leitura
  little-endian segura em CPU Wii PowerPC big-endian.
- Amostragem e interpolação de poses em frequência de **50 ticks de jogo
  por segundo**, independentemente do VSync de ~60 quadros por segundo.
  Não acelera o jogo para atingir 60 FPS.
- Prévia geométrica de grupos de modelos animáveis com rotação e
  translação interpoladas, sobre os fundos originais do Teste 02.
- Controles para escolher BODY, escolher animação e pausar/retomar.
- Verificação de compatibilidade entre clip e modelo. Um BODY estático
  continua estático e nunca recebe uma animação incompatível.
- Testes de leitura para clips sintéticos normais e otimizados,
  interpolação de poses, dados truncados e índices inválidos.
- Diagnóstico salvo na raiz do SD, conforme validado no Teste 03.1.

**Limitação importante:** a aplicação das transformações em grupos é uma
prévia de animação. A interpretação total das hierarquias, a câmera do
jogo, a posição original dos atores, a movimentação real no cenário,
as máscaras de oclusão e o controle de IA ainda dependem da integração
do motor FITD C++. Os modelos podem parecer deslocados ou deformados
neste teste. Não há gameplay, combate, áudio ou saves operacionais.

## Instalação

Extraia o ZIP na raiz do cartão SD preservando a pasta apps/fitd-wii.
Conserve os arquivos da **sua cópia original do jogo**:

    SD:/apps/fitd-wii/
        boot.dol
        meta.xml
        icon.png
        README.md
        COMPAT_TEST19.md
        data/
            CAMERA00.PAK
            ITD_RESS.PAK
            LISTBODY.PAK
            LISTANIM.PAK

Nenhum arquivo do jogo comercial acompanha o ZIP.

Abra Homebrew Channel > FITD Wii Teste 04 > A.

**Importante para ver animação:** o BODY índice 0 é estático
(flags=0001), portanto não se movimenta com LISTANIM. Aperte
direcional DIREITA uma vez para BODY índice 1, que contém 2 grupos
(flags=0003). A primeira LISTANIM também contém 2 grupos e 2 quadros.
Aperte A para pausar/retomar e observe se o modelo se deforma ou
altera a pose. Não espere um personagem caminhando pelo cenário ainda.

## Controles Wii Remote

| Botão | Função |
|---|---|
| A | Na tela inicial: começar. Durante a prévia: pausar/retomar animação |
| D-pad esquerda/direita | Modelo BODY anterior/próximo |
| D-pad cima/baixo | Entrada LISTANIM próxima/anterior |
| + / - | Câmera anterior/próxima em CAMERA00.PAK |
| B | Girar o modelo para inspeção |
| 2 | Mostrar/ocultar BODY |
| 1 | Alternar fundo real e antiga cena de demonstração GX |
| HOME | Sair e terminar diagnóstico |

GameCube: START sai; X alterna BODY; Y alterna fundo; R/L
alterna câmera; direcional escolhe modelos/clips; A pausa.

## Diagnóstico e envio

Arquivo principal:

    SD:/fitd-wii-test04-diagnostic.txt

Cópia de segurança, quando possível:

    SD:/apps/fitd-wii/wii-diagnostic.txt

O aplicativo confirma na tela inicial o caminho escolhido e gera
checkpoints a cada ~300 quadros. No log procure:

- FITD-WII-BODY OK: modelo original e seus grupos.
- FITD-WII-ANIM04 OK: clip original decodificado.
- FITD-WII-ANIM04 status=DECODED compatible=1 pose=1:
  uma pose original está sendo aplicada a um modelo animável.
- FITD-WII-ANIM04 status=DECODED compatible=0 pose=0:
  modelo estático, clips de tamanho incompatível ou variantes diferentes.
- FITD-WII-MASK status=not-integrated: máscaras ainda não prontas.
- GX frames, fps: tempo da demonstração, não desempenho do jogo completo.

## O que resta

1. Validar no Wii o clip de animação 0 com BODY 1 e ajustar a
   hierarquia de ossos e a pose.
2. Ligar interpretação de câmeras/salas, máscara de oclusão, objetos
   de ETAGE e vida/scripts.
3. Integrar motor C++ e todas as correções do Android Teste 19 e
   R36S ARM64 (IA, colisões, monstros presos, saves, áudio, controles).
4. Testar uma sessão real do jogo no hardware e medir estabilidade/FPS.

## Origem, licenças e avisos legais

Baseado em Free in the Dark (https://github.com/yaz0r/FITD),
com contribuições e correções do projeto FITD-R36S. Decodificação e
amostragem estudadas em FitdLib/hqr.cpp e anim.cpp do motor original.
Rotina UnPAK de Cyril VOILA / Mark Adler.
Código GPL-2.0 (e requisitos aplicáveis dos contribuintes).

Nenhum conteúdo proprietário de Alone in the Dark é distribuído.
O usuário deve fornecer arquivos de sua cópia legitimamente adquirida.
Nintendo não é afiliada ao port homebrew.
