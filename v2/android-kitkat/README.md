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
