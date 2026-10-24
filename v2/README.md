# FITD moderno para R36S / dArkOSRE (ARM64)

**Estado: experimental, sem jogo rodando validado no console.** A versão anterior ARMHF/GL4ES na raiz do repositório foi preservada. Esta nova tentativa fica em v2/.

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
