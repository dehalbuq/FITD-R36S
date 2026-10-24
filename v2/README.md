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
