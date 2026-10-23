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
