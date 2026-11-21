# FITD para H36S — Knulli Scarab (experimental)

Esta variante do port original de Alone in the Dark usa o hardware real do H36S: Allwinner A133P (ARM64/AArch64) e GPU **PowerVR GE8300**. Não reutiliza o binário RK3229 ARMHF do Lakka. O firmware unofficial-knulli-h36s-1331-scarab já identificado tem ambiente de usuário ARM64 e não possui runtime GLES2 ARMHF 32-bit.

**Situação:** código de compilação e patch do backend preparados; o executável ainda não foi compilado nem testado num H36S real. A primeira compilação pode revelar incompatibilidades adicionais no SDL3/EGL, no driver PowerVR, no áudio ou nos controles.

## Decisões técnicas
- CPU: ARMv8-A/AArch64, `-mtune=cortex-a53`.
- Renderizador: BGFX / OpenGL ES 2.0, shaders GLSL ES 1.00, 4:3.
- SDL3 estático com vídeo de framebuffer/EGL inspirado no Handkit (código original de Handkit incluído no build, commit pinado e licença MIT preservada). O backend especial usa `SDL_VIDEODRIVER=handkit`, e não `kmsdrm` nem o driver `mali` do SDL2 instalado no Knulli.
- SoLoud SDL3 e ALSA para o áudio; sem reutilizar `plughw:2,0` do Lakka RK3229.
- Compilação: contêiner Debian Bullseye ARM64, glibc 2.31, executado no Kali x86_64 pelo Docker/QEMU.
- Release: binário nativo ARM64, sem seções de debug; logs de compilação automáticos.

## Compilar no Kali (necessário Docker e QEMU aarch64)
Se estiver num checkout antigo do FITD-LAKKA:
```bash
cd ~/FITD-LAKKA
git fetch origin experimental/knulli-h36s-a133p-gles2
git switch -c experimental/knulli-h36s-a133p-gles2 --track origin/experimental/knulli-h36s-a133p-gles2
sudo bash knulli/run_kali_build.sh
```

Se já estiver na branch H36S, use `git pull --ff-only`.
No caso de compilação interrompida após a configuração CMake/Ninja:
```bash
sudo bash knulli/run_kali_build.sh --resume
```

O log completo é escrito em `work-h36s/logs/build-h36s-AAAAmmdd-HHMMSS.log`; em caso de falha, forneça o log, evitando repetir etapas já construídas.

## Pacote previsto
`work-h36s/FITD-H36S-KNULLI-ARM64-GLES2-TEST.zip`:
```text
FITD-H36S.sh
fitd-h36s/
  fitd.aarch64
  libs.aarch64/
  data/README.txt
  licenses/
```

## Instalar para teste no SHARE do Knulli
Na partição SHARE (exFAT) ou via acesso SSH, coloque o launcher `FITD-H36S.sh` dentro de `roms/ports/`, o executável em `roms/ports/fitd-h36s/fitd.aarch64`, e os arquivos originais `.PAK` do jogo em `roms/ports/fitd-h36s/data/`. Não mova nem remova saves já existentes; faça backup antes de sobrescrever qualquer diretório.

Caminho habitual no Knulli em execução:
```text
/userdata/roms/ports/FITD-H36S.sh
/userdata/roms/ports/fitd-h36s/fitd.aarch64
/userdata/roms/ports/fitd-h36s/data/
/userdata/roms/ports/fitd-h36s/fitd-h36s.log
```

Teste: execute o launcher pelo menu **Ports**. Se não iniciar, obtenha o log `fitd-h36s.log`. Caso o sistema retorne `Permission denied`, examine a montagem exFAT/noexec antes de tentar chmod (exFAT não possui permissões POSIX reais); não altere firmware por causa disso.

**Não distribua arquivos comerciais do jogo**. O pacote contém apenas o motor recompilado e os arquivos de licença.

## Compatibilidade
A compatibilidade foi fundamentada no código SDL3/Handkit e na auditoria prévia do Knulli A133P. Ela ainda depende da compilação e do teste reais em H36S: vídeo, joysticks, áudio e FPS não são garantidos.
