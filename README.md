# FITD / Alone in the Dark para R36S (dArkOSRE) — port experimental

Código-fonte do motor original: https://github.com/jmimu/FITD (GPL-2.0-or-later). Este repositório fornece **scripts de compilação**, não distribui os arquivos comerciais do jogo.

## Status

- CI: [GitHub Actions](../../actions/workflows/fitd-r36s.yml).
- Alvo: **ARMv7, armhf, hard-float**, R36S / RK3326.
- Compilação: Debian Bullseye ARMHF em QEMU, glibc 2.31.
- Gráficos: FITD OpenGL legado via **gl4es** e SDL1.2 via **sdl12-compat** (SDL2 nativo do console).
- O sucesso do CI confirma compilação e empacotamento, **não** execução no dArkOSRE.
- O firmware deve fornecer seu próprio carregador ELF ARMHF, glibc compatível, SDL2, EGL/GLES e drivers da GPU. Não substitua essas bibliotecas do sistema.

## Baixar o ZIP ARMHF (após execução bem-sucedida)

1. Abra **Actions** > **FITD R36S - ARMHF PortMaster**.
2. Escolha uma execução com marca verde e abra **Artifacts**.
3. Baixe `FITD-R36S-PortMaster-ARMHF`.
4. Extraia o ZIP fornecido pelo GitHub: dentro dele está `FITD-R36S-PortMaster-ARMHF.zip`.

## Instalação manual no cartão do R36S

Extraia o conteúdo do ZIP final diretamente em `/roms/ports/` ou `/roms2/ports/` conforme o cartão de ROMs configurado:

```text
ports/
├── FITD-R36S.sh
├── port.json
└── fitd/
    ├── fitd.armhf
    ├── fitd.gptk
    ├── libs.armhf/
    ├── gl4es.armhf/
    │   └── libGL.so.1
    ├── bibliotecas.txt
    ├── glibc_requerida.txt
    ├── licenses/
    └── data/
```

Copie seus arquivos de uma instalação original de **Alone in the Dark 1** para `fitd/data/`, incluindo `LISTBOD2.PAK` e todos os demais arquivos necessários. O binário espera trabalhar com a pasta de dados como diretório corrente. Se os arquivos estiverem ausentes, a inicialização será interrompida.

O launcher `FITD-R36S.sh` usa variáveis do PortMaster e configura um caminho privado de bibliotecas. Ele grava a saída do programa em `fitd/log.txt`.

## Consertando erros de compatibilidade

- **`GLIBC_2.xx not found`** — o firmware tem glibc mais antiga do que o máximo exigido pelos binários; compare `getconf GNU_LIBC_VERSION` com `glibc_requerida.txt`. **Não** copie uma glibc aleatória do compilador para o console.
- **`libSDL2-2.0.so.0 not found`** — o dArkOSRE não está fornecendo SDL2 ARMHF; será preciso instalar/configurar um runtime adequado ao firmware.
- **`EGL_BAD_MATCH` ou tela preta** — possível falha de combinação SDL2 + EGL/GLES + GL4ES; confirme o backend de vídeo e as bibliotecas da GPU do aparelho. Não há como validar isso apenas no CI.
- **Controle sem resposta** — revise `fitd/fitd.gptk` e a versão de `gptokeyb` do firmware.
- **Arquivos .PAK não encontrados** — confira `fitd/data/`.

## Recompilar

Cada commit nos diretórios `build/` ou `port/` na branch `main` dispara uma nova compilação. Também é possível iniciar manualmente em **Actions** > **Run workflow**. O workflow usa QEMU ARM e só publica o artifact se a compilação finalizar com sucesso.

Scripts: `build/armhf.sh`, `port/FITD-R36S.sh`, `port/fitd.gptk`.

**Atenção:** esta é uma tentativa de port, não um lançamento certificado do PortMaster. A execução real precisa ser testada no R36S com seu dArkOSRE.