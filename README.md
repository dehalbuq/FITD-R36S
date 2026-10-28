# FITD-R36S — Alone in the Dark (1992) no R36S

**Port experimental e aberto para contribuições.** O objetivo é executar o motor de *Alone in the Dark* no R36S com os recursos e a jogabilidade clássicos, corrigindo problemas de compatibilidade e lógica sem acrescentar remasterização ou melhorias visuais nesta fase.

> **Projeto em desenvolvimento, não lançamento oficial do PortMaster.** Este repositório disponibiliza scripts e patches do motor; **não distribui os dados comerciais do jogo**. É necessário possuir uma cópia legítima de *Alone in the Dark*.

## Qual versão usar?

| Linha | Situação | Código e compilação |
| --- | --- | --- |
| **ARM64 / AArch64 — desenvolvimento atual** | Testada no R36S com dArkOSRE; foco em correção de bugs e diagnóstico | [Branch de referência](https://github.com/dehalbuq/FITD-R36S/tree/fix/orb-hit-object-rehaunted-20261028) · [Build validado no GitHub Actions](https://github.com/dehalbuq/FITD-R36S/actions/runs/40771399610) |
| **ARMHF / ARMv7 — legado** | Experimento anterior com gl4es e sdl12-compat; não representa as correções ARM64 mais recentes | Branch `main` · [Workflow ARMHF](https://github.com/dehalbuq/FITD-R36S/actions/workflows/fitd-r36s.yml) |

**Atenção:** a branch padrão `main` ainda contém os scripts ARMHF antigos. O desenvolvimento ARM64 e as correções recentes estão na branch **`fix/orb-hit-object-rehaunted-20261028`**, não foram incorporados ao motor ARMHF. Para contribuir com o port atual, use essa branch como base.

## Estado dos testes — 28/10/2026

Alvo de teste: **R36S-V20 / Rockchip RK3326**, **dArkOSRE**, Linux **AArch64**, renderização **OpenGL ES 3.0** via SDL/KMSDRM/bgfx. Executável com símbolos de diagnóstico; a taxa observada em trechos de teste ficou próxima de 55–59 FPS, sem garantia de desempenho uniforme.

| Funcionalidade | Estado | Observações |
| --- | --- | --- |
| Inicialização e controles | ✅ Testado | Executa no R36S e permite retornar ao menu |
| Salvar/carregar | ✅ Testado | Correções de estabilidade e leitura de objetos de mundo |
| Porta secreta e itens da biblioteca | ✅ Testado | Proteção de índices de salas inválidos na colisão |
| Uso e descarte do punhal | ✅ Testado | Proteção contra modelos 3D ausentes no arremesso |
| Inimigos do salão: dano por contato | ✅ **Confirmado no console** | `HIT_OBJ` / ação 8 adaptada do ReHaunted; o jogo passa a reconhecer os acertos |
| Inimigos do salão: efeito sonoro | ✅ **Confirmado no console** | Amostra 88 acionada pelo comando `LM_REP_SAMPLE` |
| Tremor de tela | ✅ Testado | Primeira implementação de `LM_SHAKING` |
| Salas que deveriam estar escuras | ❌ Pendente | Biblioteca e outros locais continuam excessivamente claros |
| Câmera após a sequência do verme | ⚠️ Reportado | Cena pode permanecer presa na câmera do verme; diagnóstico existente, correção pendente |
| Campanha completa | 🔎 Não validada | Testes progressivos por área; outros bugs podem existir |

**Os estados “testado” descrevem sessões reais no console, não uma certificação de todos os caminhos do jogo.** O último teste confirmou o funcionamento do dano e do som dos inimigos; a iluminação foi intencionalmente deixada para depois.

Mais detalhes e histórico: [CHANGELOG.md](CHANGELOG.md) · [Como contribuir](CONTRIBUTING.md).

## Download da versão ARM64

1. Abra a [execução ARM64 de referência](https://github.com/dehalbuq/FITD-R36S/actions/runs/40771399610).
2. Na seção **Artifacts**, obtenha `FITD-R36S-AARCH64-HIT-OBJECT-REHAUNTED-DIAG`. Pode ser necessário estar autenticado no GitHub; artefatos do Actions têm prazo de retenção.
3. Extraia o ZIP fornecido pelo GitHub; dentro dele está `fitdr36s-aarch64-hit-object-rehaunted-debug.zip`.
4. Extraia **o conteúdo do ZIP interno** para `/roms/ports/` (ou a partição de ROMs configurada no seu dispositivo).

Estrutura do pacote **ARM64**:

```text
/roms/ports/
├── FITD-R36S-ARM64.sh
└── fitdr36s/
    ├── fitd.aarch64
    ├── port.json
    ├── libs/                  # bibliotecas auxiliares empacotadas quando disponíveis
    ├── LICENSE.FITD.txt
    ├── build-diagnostics.txt
    ├── logs/                  # criada pelo launcher na primeira execução
    │   ├── fitd-debug.log
    │   └── fitd-debug-previous.log
    └── data/
        └── ...               # arquivos do jogo original, fornecidos pelo usuário
```

Copie os arquivos da instalação original de *Alone in the Dark 1* para `fitdr36s/data/` (por exemplo `LISTBOD2.PAK`, `LISTLIFE.PAK`, `LISTSAMP.PAK` e os demais arquivos da versão original). Não altere os nomes dos arquivos.

**Atualizando uma instalação existente:** faça backup de `fitd.aarch64` e dos saves (`SAVE*.ITD`) antes de substituí-los. **Não sobrescreva sua pasta `data/` nem os saves** com conteúdo de um pacote de teste. Execute `FITD-R36S-ARM64.sh` pelo menu de ports.

### Diagnóstico e problemas comuns

- **Não abre / tela preta:** verifique `fitdr36s/logs/fitd-debug.log`, `SDL_VIDEODRIVER`, EGL/GLES e as bibliotecas do firmware. O build usa OpenGL ES 3.0; **OpenGL ES 2.0 não está validado**.
- **`GLIBC_2.xx not found`:** consulte `build-diagnostics.txt` e a versão da glibc do sistema; não substitua `libc.so.6` nem o carregador ELF do firmware por arquivos aleatórios.
- **Sem áudio:** registre o backend ALSA e procure `AUDIO PLAY` ou `SOUND sample=` no log.
- **Crash ou bug de jogabilidade:** informe o cenário, como reproduzir, o commit/branch e o log completo. Logs de `HIT_OBJ`, `FITD-ORB`, `FITD-WORM-CAM`, `FITD-SAVE` e `FITD-CRASH` ajudam a localizar a causa.
- O launcher de diagnóstico aceita variáveis como `FITD_DEEP_TRACE`, `FITD_AI_TRACE`, `FITD_GDB`, `FITD_FPS_LIMIT` e `FITD_PERF_TRACE`. Algumas opções geram logs volumosos.

## Compilação e organização

O build **ARM64** está na branch de desenvolvimento: `v2/build_arm64.sh` e `.github/workflows/fitd-r36s-arm64.yml`. O workflow usa um runner nativo **Ubuntu 24.04 ARM64**, clona [yaz0r/FITD](https://github.com/yaz0r/FITD) no commit fixado `543e22bc`, aplica uma sequência de patches Python em `v2/`, compila o motor e monta o pacote de teste.

```bash
git clone https://github.com/dehalbuq/FITD-R36S.git
cd FITD-R36S
git switch fix/orb-hit-object-rehaunted-20261028
bash v2/build_arm64.sh
```

A execução local requer ambiente **Linux AArch64** e as dependências de compilação descritas no workflow; no computador x86_64, prefira o GitHub Actions ou um ambiente ARM64 apropriado. O build de diagnóstico não representa uma distribuição otimizada para produção.

As correções ficam em patches aplicados **após** baixar o upstream, em vez de alterar diretamente o código do jogo comercial. Em particular, `v2/patch_hit_object_rehaunted.py` incorpora apenas a lógica de colisão/dano de `HIT_OBJ` do [AloneInTheDarkReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted) — **não** incorpora seus efeitos visuais ou sistemas HD.

## Como ajudar

São bem-vindas contribuições em **C/C++**, **Python**, **GitHub Actions**, SDL/KMSDRM, ARM64, áudio, câmeras, colisões, testes no console e documentação. Prioridades atuais: **iluminação clássica das salas** e **câmera na toca do verme**, mantendo as correções comprovadamente funcionais.

Leia [CONTRIBUTING.md](CONTRIBUTING.md) antes de abrir uma issue ou pull request. Inclua passos reproduzíveis e mantenha mudanças pequenas para reduzir regressões.

## Créditos e licenças

- **Jogo original:** *Alone in the Dark* — os recursos comerciais pertencem aos seus titulares e não são distribuídos aqui.
- **Motor FITD:** [jmimu/FITD](https://github.com/jmimu/FITD) e [yaz0r/FITD](https://github.com/yaz0r/FITD).
- **Referência de correção `HIT_OBJ`:** [spacefarergames/AloneInTheDarkReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted) (GPLv2).
- **Port, scripts, diagnósticos e testes R36S:** colaboradores deste repositório.

Preserve os cabeçalhos de autoria e as licenças do código adaptado. Consulte o arquivo de licença empacotado e os repositórios de origem para detalhes.
