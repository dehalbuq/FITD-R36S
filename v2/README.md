# FITD moderno para R36S / dArkOSRE (ARM64)

**Estado: jogo e efeitos musicais/som validado no R36S dArkOSRE; melhorias de menus e FPS ainda experimentais.** A versão anterior ARMHF/GL4ES na raiz do repositório foi preservada. Esta nova tentativa fica em v2/.

## Origem do motor
Fonte: https://github.com/yaz0r/FITD
Commit fixado para reprodutibilidade: 543e22bc78b64405537ae545ecd382dcdf2c12b2 (2025-08-02).

## Hardware confirmado
R36S com RK3326 / Cortex-A35, dArkOSRE AArch64 Debian 13, GLIBC 2.41, Mali-G31, EGL/GBM/OpenGL ES 3, 640×480. A compilação usa Ubuntu 24.04 ARM64 nativo (GLIBC 2.39), NÃO usa QEMU, GL4ES, SDL1.2 ou ARMHF.

## Mudanças aplicadas no build
- SDL3 fullscreen 640×480 e contexto GLES 3; bgfx utiliza o contexto EGL do SDL.
- Shaders OpenGL ES embarcados (ESSL), corrigindo a variante ausente no upstream.
- Geração de shaders adicionada às dependências de CMake.
- Corrige a assinatura da thread SDL; jogo em thread própria.
- SDL_Gamepad: direcional/analógico, ação, inventário, Start. Sem gptokeyb.
- Limite inicial de 25 FPS.
- Experimento de apresentação via SDL KMSDRM após bgfx::frame: pode requerer correção no aparelho, pois duas trocas de buffer podem causar tela preta.

## Como compilar pela nuvem
Abra a aba Actions da branch experimental e o workflow FITD R36S ARM64 (yaz0r experimental). Ao terminar com sucesso, baixe o artefato FITD-R36S-AARCH64-EXPERIMENTAL que contém o ZIP fitdr36s-aarch64-experimental.zip.

Alternativamente, em ambiente Linux ARM64 com as dependências do workflow instaladas, execute:

    bash v2/build_arm64.sh

O CI verde confirma compilação e empacotamento, não funcionamento real no R36S.

## Instalação para teste
Extraia o ZIP INTERNO diretamente em /roms/ports/ ou /roms2/ports/:

    ports/
      FITD-R36S-ARM64.sh
      fitdr36s/
        fitd.aarch64
        port.json
        libs/
        build-diagnostics.txt
        data/

Coloque seus próprios dados originais de Alone in the Dark 1 diretamente na pasta fitdr36s/data; LISTBOD2.PAK deve estar presente. O motor detecta também PERE.PAK, MER.PAK, AN1.PAK ou PURSUIT.PAK para outros títulos, sem garantia de completude. Arquivos comerciais não são distribuídos.

## Controles esperados
D-pad/analógico esquerdo: movimento; botão A/South: ação; B/East ou X/North: Enter/inventário; Start: Escape/menu. O posicionamento real precisa de teste.

## Diagnósticos necessários após o teste
O launcher grava fitdr36s/fitd-r36s.log. Via SSH:

    uname -m
    getconf GNU_LIBC_VERSION
    file /roms/ports/fitdr36s/fitd.aarch64
    ldd /roms/ports/fitdr36s/fitd.aarch64
    ls -l /dev/dri/
    tail -n 120 /roms/ports/fitdr36s/fitd-r36s.log

O build-diagnostics.txt registra dependências e os símbolos de GLIBC/GLIBCXX. O pacote só inclui libstdc++, libgcc e libatomic quando presentes. Glibc, loader ELF, EGL/GLES, GBM e drivers DRM devem vir do firmware, nunca de um sistema diferente.

Riscos: falta de DRM master, incompatibilidade dos shaders, troca de buffers EGL/KMSDRM e comportamento do controle. Ainda não é um lançamento certificado no PortMaster, nem uma otimização de LOD.


## Versao DEBUG para descobrir por que nao abre
A compilacao **FITD-R36S-AARCH64-DEBUG** gera o ZIP interno fitdr36s-aarch64-debug.zip. Ela registra mensagens de inicializacao do SDL3 e bgfx, cria contexto GLES, reporta biblioteca ELF e detecta deadlock se o renderizador nao responder em 30 segundos. A build usa simbolos minimos de depuracao e conserva o motor original.

Instalacao: extraia o ZIP interno na pasta ports, substituindo o launcher e o arquivo fitdr36s/fitd.aarch64 anteriores. Preserve os dados originais do jogo em fitdr36s/data e todas as pastas do jogo.

Por padrao o modo diagnostico esta ATIVADO (FITD_DEBUG=1). Execute FITD-R36S-ARM64.sh pelo menu PortMaster/Ports normalmente. Se o jogo nao abrir, mesmo saindo imediatamente, procure no cartao:

    /roms/ports/fitdr36s/logs/fitd-debug.log

Se seus jogos ficam em /roms2, use /roms2/ports/fitdr36s/logs/fitd-debug.log. A ultima execucao anterior e guardada no arquivo fitd-debug-previous.log. Envie fitd-debug.log junto com fitdr36s/build-diagnostics.txt para analise. Os logs podem conter nome de caminhos, dispositivos e outros detalhes do sistema: confira antes de compartilhar publicamente.

Se o jogo travar totalmente e voce desligar o console, o arquivo fitd-debug.log ainda guarda as ultimas etapas impressas antes do travamento (desde que a gravacao no cartao tenha sido concluida).

Opcional, somente para testes via SSH se o R36S tiver gdb instalado:

    cd /roms/ports
    FITD_GDB=1 ./FITD-R36S-ARM64.sh

Isso tenta incluir backtraces das threads no mesmo log. Se o gdb nao estiver presente, o launcher executa sem o depurador. FITD_DEBUG=0 reduz verbosidade SDL, mantendo diagnósticos basicos.

O script nao exige root, nao instala bibliotecas no sistema e nao altera EGL/GLIBC da firmware.

## Corrigido: BGFX FATAL usampler2D no Mali-G31
O log do R36S mostrou que a deteccao de Alone in the Dark funcionou, mas a GPU rejeitou o shader GLSL ES 3.0 na declaracao s_paletteTexture: "S0032: no default precision defined for variable 's_paletteTexture'" (exit 134). A compilacao anterior nao detectava esse problema porque o shader era compilado no driver GPU durante a execucao.

A versao **FITD-R36S-AARCH64-GLES3-FIX** incorpora a declaracao **precision highp usampler2D;** no shader compartilhado palette.sh (condicional para GLSL >= 300). O pacote ainda inclui o modo DEBUG e o launcher que produz **fitdr36s/logs/fitd-debug.log**. Baixe especificamente o artefato desta nova execucao, nao o DEBUG anterior, e copie novamente o executavel/launcher sem apagar a pasta data.

Importante: o arquivo **fitd-r36s.log** com "Compiled the Oct 23 2026 at 22:49:31" e da versao anterior, que nao incluia os novos diagnosticos. No proximo teste, envie **fitd-debug.log** gerado pelo launcher atualizado. Compilacao verde nao garante sucesso em runtime na Mali-G31.


## RK3326/Mali-G31: tela preta apos corrigir shader palette
Em testes reais enviados pelo usuario, o dArkOSRE AArch64 com kernel 4.4.189 abre SDL3/KMSDRM, cria EGL/GLES 3, detecta Alone in the Dark e inicializa bgfx. Entretanto, SDL3 3.2.14 reporta centenas de "Could not queue pageflip: -22" (EINVAL) e em seguida bgfx aborta com "No matching overload for function 'mod' found". A versao anterior tambem exibiu tela preta.

**Build experimental: FITD-R36S-AARCH64-KMSDRM-FIX** / ZIP interno fitdr36s-aarch64-kmsdrm-fix.zip.

Correcao em tres partes:
1. Tenta SDL_GL_SetSwapInterval(1) para evitar DRM_MODE_PAGE_FLIP_ASYNC em kernels que nao o aceitam.
2. No SDL3 KMSDRM embutido, se drmModePageFlip retornar -EINVAL, faz o fallback para drmModeSetCrtc do framebuffer mais recente e imprime um aviso unico. Esse fallback pode causar tearing, custo de CPU ou incompatibilidade com outros firmwares. Nao altera drivers do sistema e somente afeta o SDL estaticamente integrado ao jogo.
3. Substitui o modulo vetorial de inteiros (operador %) no shader noise_ps.sc por calculo float com floor(), evitando um overload inexistente de mod() no GLSL ES gerado.

bgfx foi inicializado com contexto EGL **externo ao bgfx**, criado pelo SDL3. Nesse modo, o proprio bgfx nao realiza a apresentacao do buffer SDL; manter SDL_GL_SwapWindow e necessario. Nao remover a chamada. A pagina preta ainda pode ter outras causas a investigar.

Preserve /roms/ports/fitdr36s/data e instale o novo ZIP extraindo o arquivo interno em /roms/ports. Teste o launcher e envie fitdr36s/logs/fitd-debug.log, particularmente as mensagens "SDL swap interval", "FITD-R36S: drmModePageFlip", "BGFX FATAL" e "Exit code". Se a tela permanecer preta sem crash, encerre o jogo, aguarde a gravacao do log e envie o arquivo.

Observacao: compilacao CI verde comprova apenas que o port ARM64 compila; nao valida o KMSDRM do R36S.


## Diagnosticando erro L0001 do GLSL ES (tela preta, 2026-10-23)
O console com dArkOSRE R36S kernel 4.4.189 confirmou que o patch anterior removeu completamente as mensagens "Could not queue pageflip -22", e que SDL3/KMSDRM, GLES 3, bgfx e os buffers DRM funcionam. O jogo encerra depois de "LM_WATER" / "LM_SHAKING" com BGFX FATAL "Failed to compile shader" e erros "L0001 Expected token ';'" nas linhas 49,50,73,74,98,99,138. O erro anterior no mod() GLSL desapareceu.

O pacote **FITD-R36S-AARCH64-GLSL-TRACE** adiciona instrumentacao dentro do backend GLES do bgfx para imprimir no fitd-debug.log o shader exato entregue a glShaderSource, com marcadores **[R36S-GLSL] 0001 | ...**. Isso permite identificar o trecho problemático e corrigi-lo sem suposicoes. O pacote contem as correcoes anteriores de shader, KMSDRM e as bibliotecas apropriadas. Nenhum dado do jogo e incluido.

Para testar, substitua os arquivos do ZIP interno em /roms/ports/ e preserve fitdr36s/data; execute pelo menu Ports e envie fitdr36s/logs/fitd-debug.log. Nesta etapa a nova build e para diagnostico; nao e prometida a correcao completa da tela preta. O bloco [R36S-GLSL] contem apenas codigo do shader, nao os arquivos comerciais originais.


## Correcao especifica para a tela preta: sphere_ps.sc
O GLSL-TRACE no R36S mostrou GLSL ESSL 300 invalido nos testes de material de esfera: \`(0 == tmpvar_7) tmpvar_8 = bool(1);\` seguido de \`tmpvar_9 tmpvar_8 = bool(0);\`. Os erros "Expected token ';'" tinham causa na conversao de \`switch(material)\` por glsl-optimizer.

O novo pacote **FITD-R36S-AARCH64-SPHERE-FIX** reescreve o shader de esferas com \`if (material == 3)\` para marmore, \`if (material == 2)\` para transparencia, e o caminho padrao de cor para os demais materiais, sem o switch/case. Substitui matematicamente cos(asin(y)) por sqrt(max(1-y*y, epsilon)) para manter estabilidade nas bordas. Os shaders anteriores, o patch KMSDRM e o GLSL-TRACE continuam incluidos.

Baixe o novo pacote do workflow e substitua launcher, binario e libs, mas preserve a pasta fitdr36s/data. Caso apareca tela preta ou crash novamente, anexe fitdr36s/logs/fitd-debug.log. A compilacao ARM64 bem-sucedida nao valida execucao no hardware real.


## R36S RGB + SAIDA - Diagnostico da tela preta com audio
Em teste da versao SPHERE-FIX, o console continuou tocando audio, mas ficou com a imagem preta e nao respondeu ao comando de saida. O som confirma apenas atividade da parte do motor/audio, NAO comprova que a renderizacao esteja apresentando imagem. E necessario conferir o novo fitd-debug.log para descartar outros shaders e falhas.

**Novo pacote: FITD-R36S-AARCH64-RGB-EXIT-TEST** (ZIP interno fitdr36s-aarch64-rgb-exit-test.zip).

- No inicio do SDL/KMSDRM, antes de bgfx iniciar, um teste direto de OpenGL ES desenha VERMELHO, VERDE e AZUL por 650 ms cada. Veja se as tres cores aparecem na tela do R36S antes do jogo. O log registra `VIDEO TEST: submitted RED/GREEN/BLUE` e resultados das trocas de buffer. Se o log informa que as cores foram submetidas mas a tela ficou preta, o problema provavelmente esta no caminho EGL/KMSDRM/scanout. Se as cores aparecem mas o jogo fica preto, o proximo passo e revisar o bgfx e a propria renderizacao do FITD. `FITD_VIDEO_TEST=0` desabilita o teste.
- Durante o jogo, registra `game rendered N frames; GL context=...` a cada 125 quadros (~5s), alem de mensagens quando SDL_GL_SwapWindow falhar.
- Segure **START + SELECT** durante 1,5 segundo para sair quando SDL reconhecer o controle como gamepad. Tambem aceita Enter + Escape quando o firmware emula teclado. O log informa quantos joysticks e gamepads SDL encontrou e os codigos dos botoes pressionados. O botao pode nao funcionar se a firmware nao expuser um gamepad reconhecido.
- Para evitar travamentos indefinidos, o launcher usa o comando `timeout` (se estiver disponivel), encerrando a versao diagnostica automaticamente em **90 segundos**. O programa normal nao deve usar esse limite; altere FITD_TEST_TIMEOUT ou defina zero para desabilitar posteriormente. O codigo de saida 124 indica timeout (nao erro de compilacao).

Preserve a pasta fitdr36s/data ao atualizar. Envie o fitdr36s/logs/fitd-debug.log atualizado e informe se viu VERMELHO, VERDE, AZUL antes de ficar preta. Nao force outros desligamentos se puder evitar: apos ~90s o teste deve encerrar sozinho (se a ferramenta timeout existir).


## FITD RGB passou mas jogo permanece preto (POSTBGFX-PROBE)
Teste no dArkOSRE confirma que RED, GREEN, BLUE aparecem corretamente usando glClear/SDL_GL_SwapWindow ANTES de bgfx inicializar, gamepad SDL funciona, audio toca, e >2100 iteracoes da engine rodam em 90 segundos. A tela segue preta apesar de nenhum BGFX FATAL ou erro DRM pageflip. O comando timeout da versao de teste gerou exit 124 esperado.

Artefato **FITD-R36S-AARCH64-POSTBGFX-PROBE** (ZIP interno fitdr36s-aarch64-postbgfx-probe.zip). O novo codigo diagnostica na etapa depois de bgfx::frame:
- BGFX STATS inclui numero real de draw calls, numero de views, programas, texturas e tamanho do framebuffer; 'frames rendered' sozinho NAO confirma draws GPU.
- PIXEL BEFORE MAGENTA registra RGBA central lido do framebuffer padrao que SDL vai apresentar.
- MAGENTA PRESENTATION TEST sobrescreve temporariamente o framebuffer com magenta *depois* de bgfx::frame e *antes* de SDL_GL_SwapWindow. Por aproximadamente 3,5 segundos apos o inicio do motor, espera-se tela MAGENTA (roxo-rosa). Isso serve para saber se a apresentacao permanece correta mesmo apos bgfx inicializar e desenhar. Depois a sobreposicao para, permitindo observar novamente a imagem do jogo.
- Controle: START+SELECT **ou A+B** segurados por 1,5 segundos para sair. O firmware mostrou alguns botoes apenas como joystick raw, entao A+B mapeado como gamepad e alternativa mais confiavel. Se nenhuma combinacao funcionar, timeout automatico apos 90 segundos.
- DEBUG e impressao GLSL das falhas mantidos.

**Teste esperado:** veja RED/GREEN/BLUE, depois confira se MAGENTA aparece no inicio do jogo, e entao se o jogo sai da tela preta. Envie fitdr36s/logs/fitd-debug.log. Nao extrapolar de 'BGFX STATS frames': somente numDraw/PIXEL e visibilidade da MAGENTA ajudam a localizar falha. Em uso normal desligue FITD_VIDEO_TEST/FITD_TEST_TIMEOUT e retire instrumentacao de debug.


## R36S: RGB e magenta aparecem; bgfx game draws 45 mas framebuffer preto
O teste POSTBGFX-PROBE confirmou glReadPixels do framebuffer padrao com RGBA=(0,0,0,255) mesmo depois de bgfx::frame e com 45 draw calls por quadro. O teste glClear magenta diretamente via GL antes de SDL_GL_SwapWindow mostra magenta no display; logo a integracao SDL/EGL/DRM e a troca de buffers podem apresentar cor. O bgfx compila shaders sem FATAL e tem desenhos ativos, mas os pixels do FITD permanecem pretos.

**Novo artefato: FITD-R36S-AARCH64-BGFX-GREEN-TEST** / ZIP interno fitdr36s-aarch64-bgfx-green-test.zip. Em uma view dedicada ID 30, bgfx limpa o framebuffer padrao com VERDE por 10 segundos apos inicializar o motor (depois de renderizar o game na view 0). O teste magenta em GL continua nos primeiros 3,5 segundos. Resultado esperado: vermelho, verde, azul (teste SDL) -> magenta (~3,5s, SDL GL depois de bgfx) -> verde (~6,5s, limpeza BGFX) -> jogo (~restante). Se o verde BGFX nao aparecer, o gargalo esta no caminho de saida do bgfx, nao nos recursos do jogo; se aparecer, verificar shaders, dados e renderizacao da geometria FITD. O diagnostico grava `BGFX GREEN VIEW` e `COLOR GRID: nonblack=N/9` com amostragem de nove pixels do framebuffer padrao.

Mantenha o launcher com timeout 90s, joystick START+SELECT ou A+B e a pasta fitdr36s/data original. Relate se VERDE apareceu DEPOIS do magenta e anexe fitd-debug.log. Esta e uma compilacao para diagnostico experimental; a tela preta do jogo ainda nao foi corrigida.


## Tentativa de correcao real: R8U integer textures PRECISAM nearest (2026-10-23)
Os ultimos testes no R36S demonstraram que RED/GREEN/BLUE via SDL e MAGENTA via GLES aparecem normalmente, que o bgfx consegue limpar o framebuffer em VERDE (GL readback de 9/9 pixels), e que o game renderiza ate 45 draws/quadro, mas seus pixels sao TODOS pretos (0/9) sem a limpeza de teste. Isso praticamente elimina KMSDRM/GL swap como causa primaria.

Em GLES 3.0, texturas inteiras R8UI nao aceitam filtragem linear: precisam GL_NEAREST no filtro de minificacao e magnificacao. O FITD usa bgfx::TextureFormat::R8U para background, UI, palette, ruido, mascaras, mas upstream cria com filtros implicitos (default linear). Alem disso, na revisao de bgfx usada por esse projeto, TextureGL::setSamplerState so forca nearest se **!m_gles3**, deixando os samplers GLES3 incompletos para R8U. O driver Mali pode entao retornar zeros para texelFetch em todas as texturas indexadas, fazendo a imagem do game ficar totalmente preta enquanto os draw calls e a GPU parecem funcionais.

**FITD-R36S-AARCH64-R8U-NEAREST-FIX** / ZIP interno `fitdr36s-aarch64-r8u-nearest-fix.zip` aplica BGFX_SAMPLER_POINT tanto na criacao das 5 texturas R8U quanto em 13 ligacoes de sampler. Mantem shaders corrigidos, EGL/KMSDRM, logs e saida A+B / START+SELECT. Remove a view de teste BGFX verde. Magenta agora somente opt-in FITD_MAGENTA_TEST=1; RGB desativado por padrao via FITD_VIDEO_TEST=0 (pode ser reativado=1). O limite automatico continua em 90 segundos por seguranca de diagnostico.

Se aparecer imagem real mas houver problemas de paleta, controles, ou tela preta depois, capture `fitdr36s/logs/fitd-debug.log` e envie. Compilacao verde nao prova que a GPU realmente resolveu o problema.

## R36S port JOGAVEL: imagem confirmada, restaurando AdLib, menus e performance

O teste `R8U-NEAREST-FIX` funcionou no hardware R36S V20 dArkOSRE: o jogo e personagens aparecem perfeitamente, sem BGFX FATAL, KMSDRM erro pageflip ou crashes. O usuario relatou musica ausente, teclas START/SELECT nao mapeadas para save/load/opcoes e lentidao. O log mostra `BGFX STATS` ate 45 chamadas de desenho, pixels de cores reais, gamepad conectado e saida por timeout de 90 segundos — nao um crash.

Novo artefato: **FITD-R36S-AARCH64-MUSIC-CONTROLS-PERF**, ZIP interno **fitdr36s-aarch64-music-controls-perf.zip**.

**Musica**: a rota de emulacao YM3812 OPL2 em `FitdLib/osystemAL_adlib.cpp` estava inteiramente sob `#if 0`. Novo AudioSource/AudioSourceInstance SoLoud transmite PCM mono 16-bit 44100Hz produzido por `musicUpdate` convertido para float a partir do primeiro `playMusic`. Mantem efeitos sonoros SoLoud e suporte a arquivos %02d.wav caso existam. Ajusta initMusicDriver para sinalizar musicConfigured quando OPL inicializar. Este codigo e experimental em ARM64; se houver travamento, mute, estalos ou volume alto, envie fitd-debug.log.

**Controle**: R1 ou START ou botao bruto 7 abre sistema/opcoes (ESC); nesse menu, seta para baixo 1 vez para Salvar e 2 vezes para Carregar, confirmar com A (Click) ou B (Enter). L1 ou SELECT ou botao bruto 6 abre inventario (Enter). A continua acao e B tambem pode confirmar/inventario. Direcional e analogico: movimento. Segurar START+SELECT ou A+B 1,5s para encerrar. O reconhecimento de indices 6/7 e provisoriamente baseado nos eventos do firmware, confirmar no equipamento.

**Desempenho**: desligados por padrao `FITD_GPU_TRACE` (glReadPixels e 9 leituras sincronas da GPU, muito caras), `FITD_INPUT_TRACE` e SDL debug. Limite inicial 25 FPS para preservar ritmo do jogo original, ajustavel `FITD_FPS_LIMIT=30` (20–35; valores acima de 25 podem acelerar a logica pois o game foi escrito para clock de 25Hz). Melhor avaliar 25 sem instrumentacao primeiro e so depois testar 30. Manter vsync KMSDRM para compatibilidade, sem mudar GPU modeset. O launcher NAO encerra mais a sessao depois de 90 segundos: `FITD_TEST_TIMEOUT=0` por padrao; usar por exemplo `FITD_TEST_TIMEOUT=120` para teste se desejar.

**Instalacao**: sobrepor os arquivos do pacote em /roms/ports mantendo `fitdr36s/data`, incluindo saves originais. Sem arquivos comerciais no ZIP. A compilacao CI so garante compatibilidade de build; musica, controles e FPS precisam ser validados no R36S.


## 2026-10-24 — Save slots, abandonar, configuracoes verdadeiras e FPS livre
**Artefato: FITD-R36S-AARCH64-SAVE-QUIT-FPS**, ZIP interno `fitdr36s-aarch64-save-quit-fps.zip`.

- `parseAllSaves()` original retornava sempre 0, `makeSave()` escrevia sempre `SAVE0.ITD`. Substituidos por menu de 10 slots (colunas 0..4 e 5..9), os arquivos existentes aparecem SALVO e vazios VAZIO. As setas/analógico mudam a selecao, A/B confirmam, R1/START cancelam. O menu de salvamento agora chama `makeSaveFile(slot)` e o carregamento so permite slots existentes. **Cuidado:** se escolher slot ocupado ao salvar, o arquivo sera sobrescrito. Preserve todos os `SAVE*.ITD` ao atualizar.
- `Abandonar` (ultima linha do menu, indice 6) agora encerra o executavel com `std::_Exit(0)`. Este comportamento sai ao launcher Ports em vez de retornar ao menu inicial de personagens. Continua existindo A+B ou START+SELECT segurados como saida de seguranca.
- Entradas `Musica` e `Efeitos` agora sao acionaveis e representam o estado real. A musica pausa/despausa o canal sintetizado OPL2 no SoLoud. Efeitos sao silenciados na rotina de samples quando desativados. Texto de qualidade grafica ajustado para `ALTO` (indice 50) porque o codigo base nao possui presets de grafico por nivel: o valor anterior `BAIXO` era apenas string fixa. Essa escolha permanece informativa e nao muda shaders/texturas.
- `FITD_FPS_LIMIT=30` passa a ser a configuracao padrao no launcher, comparado com 25 anteriormente; para manter velocidade original use `25`; para destravar o limite software use `0` (atenção: nao necessariamente chega a 60 devido a Mali/KMSDRM e pode acelerar animacoes/logica). A variavel aceita 20..120 e 0. `FITD_VSYNC=1` como padrao; altere para 0 para testar sem sincronizacao, que pode rasgar a tela ou nao ser suportado pelo firmware.
- Mantidos correção R8U+NEAREST, suporte EGL/KMSDRM, sintetizador OPL2, botoes mapeados e log. Para desempenho, `FITD_GPU_TRACE` desligado (evita leituras bloqueantes de GL). `FITD_TEST_TIMEOUT=0` sem auto-saida.

Nao existe garantia que FPS ilimitado aumente o FPS real, pois o RK3326 usa workaround `drmModeSetCrtc` na falha de pageflip; tambem FPS de render nao foi desacoplado do passo de simulacao da engine de 25Hz. Recomenda-se testar 30, depois 0 com atencao a jogabilidade, sem alterar clock e hardware. O menu de 10 slots e a funcao abandonar precisam ser testados no console.


## 2026-10-24 — Perfil real de desempenho RK3326 (hardware jà funcionando)
O jogo reproduz cenarios, musica AdLib e SFX, e o menu de salvar/carregar e sair funcionam, mas o personagem se move lentamente. O log anterior registra `FPS limit=30`, NAO FPS real. Prova `renderer=OpenGL ES 3.0`: a GPU Mali foi acelerada pelo bgfx, portanto instalar GL4ES ou desligar a GPU nao e o caminho.

Artefato **FITD-R36S-AARCH64-PERF-PROFILE**, ZIP interno `fitdr36s-aarch64-perf-profile.zip`.

1. **Compilador CPU**: aplica `-O3 -mtune=cortex-a35 -fomit-frame-pointer -DNDEBUG -march=armv8-a` em vez de `-O2 -g1 -fno-omit-frame-pointer` em toda a compilacao nativa ARM64. Sem `-ffast-math`; preservar compatibilidade numerica. Possivel ganho em caminhos CPU-bound, nao garantido se travado no DRM.
2. **DRM fallback**: apos detectar `drmModePageFlip=-EINVAL` pela primeira vez, evita tentar novamente o ioctl invalido a cada quadro, mantendo o `drmModeSetCrtc` que comprovadamente apresenta a imagem nesse kernel. Continua aplicando modeset a cada quadro (ponto que pode ser gargalo).
3. **Medicao**: `FITD_PERF_TRACE=1` ativa mensagens `[FITD-R36S] PERF: actual_fps=N limit=M bgfx_ms=P swap_ms=Q frames=X` a cada 5s. Medicoes usam somente SDL_GetPerformanceCounter, sem leitura bloqueante de GPU. O launcher mostra frequencia/governador do CPU, se o sistema fornecer. Recomenda-se coletar 30 s de jogo no mesmo local sem menu, variando `FITD_FPS_LIMIT=25`, `30` e `0`. `FITD_VSYNC=0` e um experimento separado, pois pode causar tearing e problemas de exibicao no KMSDRM.
4. **Limite de FPS**: 30 segue padrao. Mais quadros nao implicam a mesma velocidade de movimentos: engine original avanca logica por chamadas a `process_events` com relogio nominal de 25Hz. Se o FPS real cair a 10-15, as animacoes tambem podem aparentar lentidao. Antes de mexer em movimento/animacao, usar telemetry para distinguir gargalo na GPU, bgfx ou em DRM/CPU.

Esta versao experimental nao garante mais FPS. Preserve `fitdr36s/data` e os `SAVE*.ITD`, e mantenha o ZIP anterior funcional como backup. O uso de `-O3` deve ser validado no hardware porque pode alterar otimizacoes de codigo legado.


## 2026-10-24 — Jogo a 60 FPS, inimigo da janela preso
O usuario confirmou ~58,5 FPS reais com `FITD_FPS_LIMIT=60` no R36S, com visuais fluidos; entretanto, um inimigo que quebra a janela do sotao fica preso. No log consta `Unsupported case 1 in createFlow`. Isso e efeito especial flow=1 *nao implementado pelo FITD original*; nao ha prova de relacao causal com movimento de inimigo.

Artefato **FITD-R36S-AARCH64-NPC-CLOCK-FIX** (ZIP fitdr36s-aarch64-npc-clock-fix.zip) com ajustes **experimentais**:
- render a 60 FPS como antes, mas o relogio da simulacao do motor agora progride conforme tempo real, a 25 ticks/s, em vez de avancar pelo menos 1 tick a cada um dos ~58,5 quadros/s; `FITD_GAME_CLOCK=fixed` padrao, `legacy` para comparacao. Renderizar nao exige avanco de tempo em cada frame.
- modo `LM_SPECIAL flow=1` ignorado com aviso em vez de criar ator auxiliar AF_SPECIAL mal inicializado. Esse recurso/efeito ja nao era implementado pelo upstream. Nao altera IA de trajetoria nem colisao do monstro.
- `FITD_AI_TRACE=1` ativado para diagnostico (a cada 2 s escreve `AI TRACE` e `AI ACTOR` com id, sala, track, coordenadas, velocidade e colisao). Para desligar use 0.

Teste na **mesma cena**: iniciar novo jogo (ou salvar ANTES da aparicao do monstro), deixar `FITD_FPS_LIMIT=60`, comparar primeiro `FITD_GAME_CLOCK=fixed` e, se ainda estiver preso, `FITD_GAME_CLOCK=legacy`. Enviar logs mostrando o mesmo monstro antes e depois de ficar preso. A taxa visual de 60 Hz pode incluir frames com pose igual a anterior; a interpolacao suave de personagens a 60 Hz preservando a simulacao a 25 Hz sera uma evolucao separada. Guarde a versao anterior jogavel e backup de SAVE*.ITD.


## 2026-10-24 — Reorientacao de debug do monstro da janela
Usuario esclareceu que o monstro ja ficava enroscado **antes** do aumento de FPS para 60. Portanto, NAO associar esse bug ao relogio acelerado sem evidencia. A tentativa anterior de `NPC-CLOCK-FIX` **nao compilou** (`mainLoop.cpp` nao possuia cabecalhos SDL3; `SDL_getenv` e `Uint64` nao encontrados). Nunca instalar essa build. 

Novo artifact **FITD-R36S-AARCH64-NPC-AI-TRACE**, ZIP interno `fitdr36s-aarch64-npc-ai-trace.zip`: preserva render ~58,5 FPS, a temporizacao original do FITD (como na versao PERF-PROFILE ja validada pelo usuario), a logica `LM_SPECIAL flow=1` original sem alteracoes, os shaders R8U corrigidos, musica, menus e saves. Apenas adiciona `#include SDL3/SDL.h` em `mainLoop.cpp` e telemetria NPC. Com `FITD_AI_TRACE=1` escreve `AI TRACE` e `AI ACTOR` cada 2 segundos: id, sala, scripts de vida, modo de trajeto, waypoint, posicao X/Z, movimento X/Z, velocidade, animacao e colisao; um inimigo parado pode ser diagnosticado observando valores repetidos. `FITD_AI_TRACE=0` desliga as mensagens.

Teste a cena original sem empurrar o armario na frente da janela. Se o monstro estiver travado sem obstrucao proposital, capture mais 15 a 30 segundos do mesmo local e envie `fitdr36s/logs/fitd-debug.log`. Comparar id do inimigo, modo de trajetoria, posicao, speed e colisoes. Nao alterar precocemente IA/colisao nem reescalar o relogio do jogo.


## 2026-10-24 — Rifle nao acerta e monstros ficam presos em salas diferentes
A usuario testou um monstro preso no sotao e outro no quarto da chave/espelhos. Log com `AI ACTOR` mostra atores recebendo `speed=4` mas com coordenadas inalteradas e sem HARD_COL visivel, por exemplo na sala 4, ator world=57 (id=32). Em sala 2, world=35 muda coordenadas mas oscila. A GPU fornece aproximadamente 58,5 FPS: problema de jogabilidade, nao desempenho.

Build FITD-R36S-AARCH64-HIT-COLLISION-FIX, ZIP `fitdr36s-aarch64-hit-collision-fix.zip`:
1. **Balistica**: upstream `checkLineProjectionWithActors()` tinha condicao `AsmCheckListCol(...) <= 0` e **parava o tiro quando nenhuma parede era detectada**; quando parede existia, buscava atores. Corrigido para `> 0`: parede agora interrompe raio, caminho desobstruido permite registrar acerto no alvo. Guarda de 512 passos impede loop infinito para dados invalidos.
2. **Colisoes invisiveis**: `CheckObjectCol()` agora ignora atores marcados `AF_SPECIAL` (efeitos nao fisicos), mesmo se `indexInWorld=-2`. Origem `InitSpecialObjet()` implementa apenas mode=0; modes 1,2,3,4 antes alocavam atores com campos visuais/colisao nao configurados (objetos-fantasma). Agora esses efeitos incompletos retornam sem alocar atores. O modo de tiro `LM_SPECIAL 1` nao desreferencia `HIT_BY` possivelmente invalido antes de invocar efeito ausente. Efeitos visuais ausentes permanecem ausentes.
3. **Diagnostico**: cada disparo imprime `[FITD-R36S] SHOT: shooter=N room=N touched=N origin=X,Y,Z beta=N radius=N power=N`, tornando verificavel se o sistema detecta o ator; `AI TRACE` e `AI ACTOR` anteriores permanecem. Compatibilidade OpenGL ES3/SDL3/SoLoud, menu, saves e o ritmo atual de 60 FPS foram preservados.

A correcao da colisao de fantasmas e plausivel para monstros enroscados, mas **nao comprovada no console**; se continuar, enviar log com shot e AI ACTOR para rastrear pathfinder/rotas da fase. Fazer backup dos saves antes do teste.
