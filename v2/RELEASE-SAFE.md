# FITD-R36S — release conservadora ARM64

Esta variante fica isolada em `release/r36s-aarch64-safe-20261101` e usa o último estado compilado da branch de créditos/aviso legal (commit `4680022820e3`). Não altera as versões Android ou debug.

**Otimização:** `-O1 -g0 -DNDEBUG -march=armv8-a -mtune=cortex-a35`, com `-fno-strict-aliasing -fwrapv -fstack-protector-strong -D_GLIBCXX_ASSERTIONS`, remoção de seções não usadas e `strip --strip-unneeded`. O nível O1 foi mantido propositalmente porque versões mais agressivas podem expor defeitos no motor antigo. Sem LTO, fast-math, O2, O3 ou Ofast.

**Sem depuração:** o executável não contém seções `.debug_*`, a rotina de backtrace/sinais e `R36S_TRACE` ficam desabilitadas via `FITD_R36S_RELEASE=1`. O launcher da release não usa GDB, testes RGB, rastreios de IA/áudio/FPS nem timeout. Os consertos de salvamento, áudio, jogabilidade e gráficos permanecem aplicados. A saída de log normal é descartada; para registrar eventual falha, use `FITD_RELEASE_LOG=1`.

**FPS:** 60 por padrão (v-sync ativo, até a taxa permitida pelo hardware). A release corrige o relógio baseado em quadros de `osystem_startOfFrame()`: `timeGlobal` e `timer` avançam até 25 ticks por segundo real, sem forçar um tick por quadro. Essa alteração trata cronômetros, animações e scripts baseados nesses relógios. **Parte da lógica do FITD ainda ocorre uma vez por quadro**, então a velocidade dos personagens e algumas animações precisam ser verificadas no R36S antes de afirmar que a simulação está totalmente dissociada do renderizador.

Para comparar ou ajustar sem recompilar, use `FITD_FPS_LIMIT=50` (ou `60`) e `FITD_R36S_TIMEBASE=real` (padrão) ou `legacy` (comportamento antigo de um tick mínimo por quadro). Aumentar o valor de `FITD_FPS_LIMIT` não garante que a GPU entregará todos esses quadros.

### Baixar e instalar

Acesse **Actions > FITD R36S ARM64 (safe release)** e o artefato `FITD-R36S-AARCH64-SAFE-RELEASE`. Extraia o ZIP interno `fitdr36s-aarch64-safe-release.zip` em `/roms/ports/` no R36S com dArkOSRE AArch64.

Faça backup da instalação, especialmente da pasta `fitdr36s/data`, dos saves `SAVE*.ITD` e do executável antigo. **Não apague nem substitua seus dados originais.** O pacote contém somente o motor, launcher e materiais do projeto; não distribui os dados comerciais de Alone in the Dark.

O CI confirma compilação e integridade do pacote, não estabilidade no aparelho. Recomenda-se testar abertura, controle, áudio, Emily e Edward, transições de sala, salvar e carregar.
