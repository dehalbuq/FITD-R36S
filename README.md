# FITD-R36S — Alone in the Dark (1992) no R36S

> **Novidade — FITD-R36S v1.0.0 ARM64 (dArkOSRE + ArkOS4Clone):** dois executáveis ARM64 em um único PortMaster ZIP, com seleção automática por glibc. A variante ArkOS4Clone (glibc 2.30) iniciou no console do portador; builds de ambos os firmwares passaram no GitHub Actions. O pacote unificado e o conjunto de testes de regressão ainda precisam ser validados antes da publicação como estável final. [Workflow para baixar o ZIP](https://github.com/dehalbuq/FITD-R36S/actions/workflows/fitd-r36s-dual-arm64-release.yml) · [Tutorial de publicação](https://github.com/dehalbuq/FITD-R36S/blob/release/r36s-aarch64-safe-20261101/v2/PUBLISH_V1.0.0_GITHUB.md) · [Notas da Release](https://github.com/dehalbuq/FITD-R36S/blob/release/r36s-aarch64-safe-20261101/v2/RELEASE_NOTES_V1.0.0.md).

## ARM64 v1.0.0: compatibilidade dupla

| Firmware | Binário selecionado | Requisito ABI | Evidência |
|---|---|---|---|
| **dArkOSRE** | `fitd.aarch64` + `libs.aarch64/` | base Ubuntu 22.04, glibc até 2.35 | build GitHub Actions verde e testes anteriores no hardware |
| **ArkOS4Clone** | `fitd.glibc230.aarch64` + `libs.glibc230/` | símbolos GLIBC do executável até 2.27; firmware glibc 2.30 | build GitHub Actions verde + inicialização confirmada pelo portador |

O launcher utiliza `getconf GNU_LIBC_VERSION` para selecionar a variante. Os dados originais do jogo não acompanham o pacote. Faça backup dos arquivos `SAVE*.ITD` e da pasta `data/` antes de atualizar. O pacote usa **ARM64 / OpenGL ES 3.0** e não garante suporte a ARMHF ou GLES 2.0.

**Instalação:** extraia `FITD-R36S-v1.0.0-ARM64-PortMaster.zip` na pasta `/roms/ports/`; adicione os arquivos do jogo original em `fitdr36s/data/`; abra `FITD-R36S-ARM64.sh` no menu Ports. Não substitua a glibc, a EGL ou os drivers GPU do firmware. A versão ARMHF anterior permanece na branch `main`.

**Status:** a inicialização no ArkOS4Clone foi confirmada, mas o pacote unificado e a campanha completa ainda necessitam testes físicos. O projeto não é uma Release oficial do catálogo PortMaster.


**Port comunitário do motor aberto FITD para Linux ARM64, com compatibilidade confirmada de inicialização em ArkOS4Clone e histórico de testes no dArkOSRE.** Esta é a base da primeira versão unificada **v1.0.0**.

> **Este projeto NÃO distribui o jogo original.** Para jogar, você precisa de uma cópia adquirida legalmente de *Alone in the Dark* (1992) e fornecer os arquivos originais. O port não tem vínculo ou aprovação dos titulares do jogo nem é, por si só, um lançamento oficial do catálogo PortMaster.

## Download

- [GitHub Releases](https://github.com/dehalbuq/FITD-R36S/releases) — local dos pacotes publicados; procure o arquivo **`fitdr36s.zip`** da versão v1.0.0.
- [Branch do port ARM64](https://github.com/dehalbuq/FITD-R36S/tree/release/r36s-aarch64-safe-20261101) — código, patches, testes e ferramentas de montagem.
- [Guia de publicação da v1.0.0](https://github.com/dehalbuq/FITD-R36S/blob/release/r36s-aarch64-safe-20261101/v2/RELEASE_PUBLICATION.md) — como publicar o ZIP correto e o checksum.

**Importante:** a versão unificada está preparada, mas o ZIP final reunindo as duas variantes deve ser testado nos dois firmwares antes de ser anunciado como amplamente estável. A campanha completa não foi homologada.

## Firmwares e seleção automática

| Sistema | Evidência atual | Binário selecionado |
| --- | --- | --- |
| **ArkOS4Clone / Kinhank K36** | Inicialização confirmada em dispositivo real; Ubuntu 19.10, glibc 2.30, AArch64 | `fitd.glibc230.aarch64` + `libs.glibc230/` |
| **dArkOSRE / R36S RK3326** | Histórico de testes reais de boot, controles, áudio, save/load e áreas da campanha; a variante moderna tem build ARM64 aprovado | `fitd.aarch64` + `libs.aarch64/` |
| Outros firmwares ARM64 | **Não homologados** | Depende da glibc; renderização/controles precisam de testes |

O launcher `FITD-R36S-ARM64.sh` usa `getconf GNU_LIBC_VERSION` para escolher:
- **glibc 2.30–2.34:** variante **legacy**, compilada em Debian 10/Clang 11 com ABI auditada para glibc até 2.30;
- **glibc 2.35 ou mais recente:** variante **moderna**, compilada em Ubuntu 22.04 ARM64.

A seleção evita distribuir `libc.so.6`, o carregador ELF, os drivers Mali, EGL e GBM dentro do port. **Não substitua bibliotecas do firmware.**

## Instalação / atualização

1. Faça backup da pasta de instalação e de seus saves (`SAVE*.ITD`).
2. Baixe **`fitdr36s.zip`**, não o ZIP externo do GitHub Actions.
3. Extraia o conteúdo do ZIP em **`/roms/ports/`** ou no diretório `ports` usado pelo seu firmware.
4. Copie **seus próprios arquivos do jogo** para `/roms/ports/fitdr36s/data/`; por exemplo `LISTBOD2.PAK`, `LISTLIFE.PAK`, `LISTSAMP.PAK` e os demais arquivos originais.
5. Abra `FITD-R36S-ARM64.sh` em **Ports**.

**Estrutura do ZIP:**

```text
ports/
├── FITD-R36S-ARM64.sh
├── port.json
├── gameinfo.xml
├── README.md
└── fitdr36s/
    ├── fitd.aarch64
    ├── libs.aarch64/
    ├── fitd.glibc230.aarch64
    ├── libs.glibc230/
    ├── data/                 # incluir somente seus arquivos originais
    ├── splash/
    ├── font.conf
    ├── graphics.conf
    ├── splash.conf
    └── licenses/
```

Ao atualizar, **não apague nem substitua a pasta `data/` inteira**, pois ela contém o jogo comercial e pode conter saves.

## Recursos

- Motor FITD ARM64, renderizador **OpenGL ES 3.0** via SDL3/KMSDRM/BGFX.
- Compilação Release com otimizações conservadoras para Cortex-A35, sem seções de debug.
- Correções de estabilidade na gravação/carregamento, áudio, colisões e áreas específicas.
- Fonte alternativa legível CP850 (com opção de fonte clássica), máscara gráfica clássica por padrão.
- Telas iniciais de créditos e de informação legal; FPS máximo configurável (padrão 60) e VSync desligado por padrão.
- Compatibilidade **OpenGL ES 2.0 e ARMv7/ARMHF não incluída** nesta Release.

## Diagnóstico e limites atuais

Para registrar erros de inicialização, pelo terminal/SSH:

```bash
FITD_RELEASE_LOG=1 bash /roms/ports/FITD-R36S-ARM64.sh
sh /roms/ports/fitdr36s/check_compatibility.sh
```

Os relatórios ficam em `fitdr36s/logs/fitd-release.log` e `fitdr36s/compatibility-report.txt`.

**Pendências conhecidas:** a campanha completa não foi validada; iluminação clássica de algumas salas, câmera/ações na sequência do verme e sincronização de alguns eventos com FPS altos podem precisar de ajustes. A experiência em ArkOS4Clone teve a inicialização confirmada, mas ainda demanda teste estendido de controles, som, save/load e encerramento com o ZIP unificado. Não afirmar suporte irrestrito a outros dispositivos ou firmwares.

Para reportar bugs, inclua firmware, modelo do console, os passos para reproduzir e os logs. Não envie arquivos comerciais `.PAK` ou saves privados.

## Status e código-fonte

A branch **`main`** mantém ferramentas e experimentos históricos **ARMHF**. A linha de lançamento **ARM64** é [`release/r36s-aarch64-safe-20261101`](https://github.com/dehalbuq/FITD-R36S/tree/release/r36s-aarch64-safe-20261101). Para histórico de mudanças e problemas anteriores, consulte [CHANGELOG.md](CHANGELOG.md).

### Compilações de origem da v1.0.0

- [ARM64 moderna (dArkOSRE), GitHub Actions #41561105602](https://github.com/dehalbuq/FITD-R36S/actions/runs/41561105602).
- [ARM64 legacy (ArkOS4Clone), GitHub Actions #41562095088](https://github.com/dehalbuq/FITD-R36S/actions/runs/41562095088).

O pacote final combina os dois **sem substituir um pelo outro**, e passa por auditoria de arquitetura, símbolos GLIBC e integridade de ZIP.

## Créditos e licença

- Jogo original: **Infogrames** e respectivos titulares de *Alone in the Dark*.
- Motor livre FITD: [jmimu/FITD](https://github.com/jmimu/FITD) e [yaz0r/FITD](https://github.com/yaz0r/FITD).
- Referência para correções de colisão: [AloneInTheDarkReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted).
- Port, scripts, testes e integração RK3326: **dehalbuq** e colaboradores.
- Licenças e avisos de dependências: pasta `fitdr36s/licenses/` do ZIP.

**Os dados comerciais do jogo não são licenciados nem redistribuídos por este projeto.**
