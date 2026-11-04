# Teste independente SDL2 + OpenGL ES 2.0 para Android 4.4 (API 19)

Este diretório contém **somente um smoke test**, não o motor FITD. O aplicativo de teste mostra um triângulo móvel com shaders GLSL ES 1.00 durante até 90 segundos. O teste abre um controle SDL2 conectado (Bluetooth/USB), inclusive controles que chegam como *joystick* genérico, e reage ao direcional, analógico esquerdo e botões. START ou B encerram, A muda a cor e a direção/analógico movimentam o triângulo. Não inclui recursos comerciais.

## Projeto Android reproduzível

O projeto Gradle fica em `android/` com arquivos próprios e estáveis (Gradle **8.1.1**, Android Gradle Plugin **8.1.1**, Java **17**, `minSdk 19`, ABI **armeabi-v7a**). O workflow `.github/workflows/android-kitkat-gles2.yml`:

1. Compila SDL2 `release-2.30.12` usando NDK r23c para ARMv7/API 19.
2. Compila `gles2_probe.cpp` como `libmain.so` e copia o Java do SDL2 e ambas as bibliotecas para o projeto Gradle.
3. Produz `FITD-KitKat-GLES2-ARMv7-TEST.apk` e valida SDK, GLES e ABI com `aapt`, além de verificar as bibliotecas dentro do APK.
4. Publica o APK no artefato GitHub Actions `fitd-kitkat-gles2-armv7-apk-test`.

Nenhuma dependência da tarefa `externalNativeBuild` do modelo SDL2 é necessária. O APK só deve ser chamado de compilado depois que o workflow terminar com sucesso.

## Teste em dispositivo real

Instalar com `adb install -r FITD-KitKat-GLES2-ARMv7-TEST.apk`. Abrir **FITD GLES2 Test** no aparelho e procurar o triângulo verde. Emparelhar um controle Bluetooth **nas configurações do Android antes de abrir o app**. O D-pad ou o analógico esquerdo deve movimentar o triângulo; A altera verde para avermelhado; START ou B encerram. Desconectar e reconectar o controle também deve funcionar. Se o controle surgir apenas como joystick genérico, o teste usa os dois primeiros eixos e botões 0/1. Confira as mensagens `FITD-ANDROID-PAD` e `FITD-ANDROID-GLES2` em `adb logcat`, além de testar suspensão/retomada.

**Importante:** o teste não executa Alone in the Dark, não usa dados do jogo, não valida bgfx e não demonstra ainda que o motor FITD funciona em Android 4.4. Essa migração só virá depois de confirmar este teste.

## Próximo passo: transformar o teste no motor FITD real

Este APK **não é o jogo**. Ele valida o caminho Android 4.4/API19 -> SDL2 -> GLES2 -> controles. Os próximos trabalhos, em ordem:

1. Migrar a camada de plataforma de `FitdLib/osystemSDL.cpp`, `input.cpp`, `bgfxGlue.cpp` e integrações ImGui de **SDL3 para SDL2**, preservando o projeto Linux/PortMaster sem modificações.
2. Compilar o motor FITD como biblioteca compartilhada `libmain.so` para `armeabi-v7a`, corrigindo o erro histórico de linker `HQR_Reset<sBody>`. Não aceitar como válido um APK que contém só o teste gráfico.
3. Gerar shaders de **OpenGL ES 2.0** com shaderc no host, revisar formatos de textura e eliminar recursos que só existem em GLES3. Validar numa GPU limitada a ES2.
4. Adaptar armazenamento privado/importação dos arquivos originais do jogo, salvamentos, áudio SoLoud e saída segura, com Java e SDL2 compatíveis com API19.
5. Validar instalação física, gamepad Bluetooth/reconexão, FPS sem acelerar a lógica, carregar/salvar e ausência de bibliotecas ARM64.

**Lição da tentativa anterior:** a branch `port/android-armeabi-v7a-release-20261031` era SDL3/GLES3/API21 e nunca entregou um APK funcional do motor. Seu último erro de build incluía `undefined symbol: HQR_Reset<sBody>`, mas corrigir esse erro isoladamente não a tornaria compatível com Android 4.4/GLES2.

**Isolamento:** essa linha Android é independente da Release ARM64 para dArkOSRE/ArkOS4Clone.


## Diagnóstico PG-9068 — projetor Chanuo A1 / Android 9 (Teste 02)

No Teste 01, o triângulo foi exibido e o aplicativo foi encerrado com B/START,
mas o iPega PG-9068 não moveu o triângulo. Isso indica possível entrada parcial;
não afirmar que nenhum botão foi reconhecido sem confirmar o método de saída.

A variante do Teste 02 inclui:

- Suporte ao direcional HID do Android quando entregue como teclas **setas**
  (e também W/A/S/D), além de SDL GameController e joystick genérico;
- Ação por A do controle ou Enter/Espaço/Z;
- Logs `FITD-ANDROID-INPUT` para teclas, botões, hats e eixos;
- Dois indicadores visuais de diagnóstico no canto superior: **verde**
  (SDL GameController), **amarelo** (joystick genérico), **vermelho**
  (nenhum dispositivo enumerado); **roxo** no canto superior direito
  indica que pelo menos um evento de teclado foi recebido.

**Emparelhamento PG-9068:** segundo o manual oficial, desligue/remova a
conexão anterior, segure **Android + HOME** por aproximadamente 2 segundos,
emparelhe `PG-9068` e confirme **LED1 ligado** (modo HID/Gamepad). LED2
indica modo mouse/multimídia. Esses modos são do próprio controle, não do
FITD. Manual: https://m.ipega.hk/uploads/201806/5b2ca38e19381.pdf

**Como testar:** iniciar com controle conectado; anotar cores dos
indicadores; testar D-pad, analógico esquerdo, botão A e B/START. Caso não
responda, enviar `adb logcat -d | grep FITD-ANDROID-INPUT` quando ADB
estiver disponível. O teste ainda não contém o motor comercial.


## Resultado do Teste 03 — Chanuo A1 / iPega PG-9068

O usuário confirmou que o diagnóstico moveu o triângulo e mudou sua cor. O relatório exportado mostra:

- Android InputDevice id=9: PG-9068, VID 6473, PID 1026.
- SDL2 enumerou seis joysticks: índice 0 receptor IR `gpio_ir_recv`, 1–3 `virtual-search`, índice 4 `PG-9068`, índice 5 acelerômetro.
- PG-9068: 6 eixos, 19 botões, mapeamento SDL GameController. Eventos reais `SDL_JOYSTICK_BUTTON dev=4 button=13/14` e `SDL_JOYSTICK_AXIS dev=4 axis=0/1`.
- Mali-T720, contexto GLES 2.0 criado. Driver reporta GLES 3.1; não valida GPU limitada somente a GLES2.
- SDL áudio openslES; sistema reporta 48 kHz.
- Firmware declara release '12' e SDK 27, metadados conflitantes (API27 = Android 8.1).

**Causa do bug:** o teste inicial abria o primeiro SDL GameController: era o controle remoto infravermelho, não o iPega. O diagnóstico percorreu todos os dispositivos e funcionou. O helper `fitd_gamepad_select_sdl2.h` agora prioriza controles físicos e ignora entradas IR/virtuais conhecidas; integrado ao teste GLES2 e pronto para adaptação ao motor FITD. Ainda requer validação no dispositivo e em outros controles.


## Teste 04 — núcleo original do FITD confirmado no Chanuo A1

O usuário confirmou que o APK ARMv7 API19 com os módulos originais
`FitdLib/unpack.cpp`, `pak.cpp` e `cosTable.cpp` abriu e respondeu
aos comandos no projetor Chanuo A1. O teste gráfico chama primeiro
`fitd_android_core_selftest()`: valida diretório privado, tabela matemática,
PAK inexistente e descompressão DEFLATE com um bloco artificial sem dados
comerciais. Isto **não** significa que o gameplay foi executado.

## Teste 05 — seleção de dados locais, parser PAK e controles

Workflow: `.github/workflows/android-armv7-game-data.yml`.
APK: `FITD-Android-ARMv7-GAME-DATA-API19-TEST.apk`.
Pacote distinto: `io.github.dehalbuq.fitdandroidassets`.

Este APK mantém SDL2, API19, GPU GLES2, e o seletor de iPega físico para evitar
o controle remoto IR que ocupa a primeira entrada do SDL. Foi acrescentada uma
tela Java antes do teste nativo, com duas maneiras de importar dados originais:

- **Android 5.0 ou superior:** escolher uma pasta inteira via Document Tree,
  com botão de seleção manual de múltiplos arquivos como alternativa;
- **Android 4.4:** selecionar múltiplos documentos com
  `ACTION_OPEN_DOCUMENT`, disponível desde API19.

A importação copia arquivos reconhecidos `.PAK`, `.ITD`, `.TAT`,
`.CC1`, `.BBS`, `.COM` e `.BIN` para `getFilesDir()`,
sem internet ou privilégios de root, ignorando `SAVE*.ITD`. A verificação
nativa lê os arquivos `LISTBOD2.PAK` e `LISTBODY.PAK`, valida
pré-requisitos de cabeçalho e invoca o **leitor original**
`PAK_getNumFiles()`; arquivos opcionais também são examinados.
O triângulo é **azulado/ciano** quando os dados passaram no teste e o
botão A continua tornando-o avermelhado. Logs usam a marca
`FITD-GAME-DATA:`.

**Importante:** ainda não há motor completo jogável neste APK. Falta
compilar e migrar vídeo bgfx/shaders e ciclo de jogo SDL3 para SDL2/GLES2,
além do áudio, transições, salvamento e controles do gameplay. A aprovação
do CI não comprova leitura correta de PAK comercial até testada no aparelho.
Nenhum arquivo comercial é incluído no artefato.


## Resultado do Teste 05 com dados embutidos — Chanuo A1 (03/11/2026)

**Aprovado em teste físico pelo usuário.** O APK privado ARMv7 de Teste 05,
com recursos do jogo fornecidos pelo próprio usuário e instalados automaticamente
no armazenamento do aplicativo, foi relatado como funcionando perfeitamente
no projetor Chanuo A1 com controle iPega PG-9068. Isso valida a execução do
teste de motor, o processo de instalação dos recursos e o uso dos controles
**no cenário testado**. Não equivale a testar gameplay completo, salvamentos,
áudio do jogo, troca de salas ou compatibilidade em outros dispositivos.

Os recursos proprietários do jogo ficam fora do repositório e dos workflows
públicos. O projeto Linux/PortMaster publicado não foi alterado.

### Próximo marco: inicialização de gameplay real

- Converter integração SDL3 para SDL2 sem afetar o executável R36S;
- Preparar janela e backend BGFX OpenGLES 2.0 no Android, gerando shaders ESSL
  adequados e eliminando pressupostos exclusivos de GLES3;
- Compilar ciclo principal, assets, áudio e funções de save do motor original;
- Substituir o triângulo de diagnóstico por inicialização de jogo;
- Selecionar gamepad físico em vez de `gpio_ir_recv` e dispositivos virtuais;
- Validar cena inicial, ações, áudio, saída e salvamento, com logs específicos.

A fase de gameplay **ainda não foi compilada nem validada** neste marco.


## Teste 06 — renderização de câmeras reais em OpenGL ES 2.0

**Código:** `v2/android-kitkat/engine-scene/fitd_scene_preview.cpp` e CMakeLists.
**CI:** `.github/workflows/android-armv7-engine-scene.yml`.
**Objetivo:** primeira imagem **real** do jogo, antes do gameplay completo.
O workflow compila SDL2 e os módulos FITD originais `pak.cpp`,
`unpack.cpp` e `cosTable.cpp` em `libmain.so` ARMv7 API19, sem SDL3.

Na execução:

1. Obtém os recursos originais instalados na pasta privada do aplicativo;
2. Usa a rotina original `loadPak()` para ler `CAMERA00.PAK` ou uma câmera
   disponível dos arquivos `CAMERA01..07.PAK`;
3. Usa `ITD_RESS.PAK`, entrada 3, como paleta do AITD1, segundo
   `FitdLib/main.cpp::loadPalette()`;
4. Converte a imagem indexada de 320×200 para uma textura RGBA no GLES2;
5. Renderiza uma viewport **4:3 com barras pretas** quando necessário,
   preservando o aspecto de exibição DOS;
6. Permite percorrer câmeras pelo direcional/analógico do gamepad físico
   (sem priorizar `gpio_ir_recv`), e pisos pelos ombros L/R.

**Comandos de pré-visualização:** esquerda/direita percorre câmeras;
A avança, X retrocede; L1/R1 muda piso `CAMERA00..07`;
B ou START encerra. Setas do teclado e PageUp/PageDown também funcionam.

**Limitações declaradas:** esta é uma pré-visualização de cenários,
não uma inicialização de gameplay (sem personagens, eventos, áudio,
colisões ou salvamentos). O Android ainda não executa `FitdMain`
nem o renderizador completo de polígonos; a prévia usa o parser original,
mas um backend GLES2 simples para as imagens dos cenários.
CI comprova compilação / formato da biblioteca, não o resultado visual
até a execução no hardware.

**Dados comerciais:** não são publicados no GitHub nem nos artefatos CI.
A versão de teste privada utiliza apenas os arquivos enviados pelo usuário.
Uma nova assinatura de APK pode requerer desinstalação do Teste 05; isso
remove dados privados dessa instalação. Não instalar sobre versões
com saves sem backup. A Release Linux do R36S permanece separada.

**Próximo marco:** implementar inicialização efetiva da simulação
(`FitdMain`), adaptar a camada SDL3 do upstream para SDL2 e desenvolver
o backend de personagens/objetos em GLES2, com FPS sem acelerar a lógica.


## Teste 06 aprovado em hardware — 03/11/2026

O usuário confirmou **funcionou como esperado** no Chanuo A1 com o
`FITD-Alone-in-the-Dark-ARMv7-TEST06-CENARIOS-REAIS.zip`.
A validação abrange a pré-visualização de cenários do jogo real
`CAMERAxx.PAK` renderizados em OpenGL ES 2.0, preservando o aspecto 4:3
e o suporte ao iPega PG-9068 já testado. Este marco **não** equivale a
gameplay completo, pois a prévia ainda não executa o ciclo `FitdMain`
nem desenha os personagens.

### Início da integração de controles de gameplay

O módulo `v2/android-kitkat/engine-gameplay/fitd_gameplay_input_sdl2.h`
mapeia as entradas SDL2 no formato nativo do motor original:

- `JoyD`: cima 1, baixo 2, esquerda 4 e direita 8;
- `Click`: botão de ação A ou gatilho/ombro direito, teclado espaço/Z;
- `key`: `0x1B` para B/START ou Escape, `0x1C` para Y/Enter;
- Entrada do iPega físico selecionada pelo helper do Teste 03,
  em vez do receptor IR virtual do firmware.

A pré-visualização também coleta `FITD-GAMEPLAY-INPUT` para validar o
mapeamento sem alterar o funcionamento comprovado no Teste 06.
Esses valores serão ligados aos globais `JoyD`, `Click` e `key`
quando o loop `FitdMain` estiver integrado.

**Próximo bloqueio principal:** converter o motor completo do upstream,
que usa SDL3, bgfx, shaders e ImGui, para SDL2 e GLES2 com timing e áudio
consistentes. Não publicar um APK de jogo real antes de compilar e testar
a lógica de gameplay.
