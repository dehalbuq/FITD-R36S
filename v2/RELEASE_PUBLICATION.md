# Como compilar e publicar a Release ARM64 do FITD-R36S

Repositorio: https://github.com/dehalbuq/FITD-R36S
Branch de trabalho: `release/r36s-aarch64-safe-20261101`
Destino: Linux AArch64, SDL3/KMSDRM, OpenGL ES 3.0, PortMaster.
O port NAO contem dados originais do jogo.

## 1. Obter o binario de verdade
1. Abra: https://github.com/dehalbuq/FITD-R36S/actions/workflows/fitd-r36s-arm64.yml
2. Clique em **Run workflow** (Executar fluxo de trabalho).
3. Selecione a branch **release/r36s-aarch64-safe-20261101**.
4. Clique em **Run workflow** e abra a execucao.
5. Aguarde o status **Success** (verde). Se a compilacao falhar, NAO publique.
6. Na secao **Artifacts**, baixe:
   `FITD-R36S-AARCH64-RELEASE-RK3326-60FPS-NO-DEBUG`
7. O GitHub Actions entrega um ZIP externo do artifact. Extraia esse arquivo.
   Dentro dele devem existir `fitdr36s.zip` e `SHA256SUMS.txt`.
8. O `fitdr36s.zip` interno e o pacote do port. NAO envie o ZIP externo do artifact ao usuario final.

### Confirmar estrutura e integridade
No Linux ou WSL:
```sh
unzip -t fitdr36s.zip
unzip -l fitdr36s.zip
sha256sum -c SHA256SUMS.txt
```
No Windows, voce pode inspecionar o ZIP com 7-Zip, mas a verificacao de hash
via SHA256SUMS.txt e recomendada.

O ZIP precisa incluir `FITD-R36S-ARM64.sh`, `fitdr36s/fitd.aarch64`,
`port.json`, `gameinfo.xml`, README e licencas.
Para submissao oficial tambem precisa incluir `screenshot.png` de gameplay
e um `cover.png` somente se ele atender ao criterio do PortMaster.
Se houver `SUBMISSION_BLOCKERS.txt`, NAO apresente o pacote como pronto
para submissao oficial.

## 2. Criar uma pre-release no seu GitHub
1. Abra: https://github.com/dehalbuq/FITD-R36S/releases/new
2. Clique em **Choose a tag** e digite `v0.1.0-arm64-test`.
3. Crie a nova tag apontando para a branch
   `release/r36s-aarch64-safe-20261101`, confirmando que ela aponta para
   o commit que produziu o ZIP testado.
4. Titulo: `FITD-R36S ARM64 v0.1.0 - PortMaster test build`.
5. Marque **Set as a pre-release**, pois ainda faltam testes multi-firmware.
6. Anexe `fitdr36s.zip` e `SHA256SUMS.txt` em **Attach binaries**.
7. Escreva a descricao com limitacoes e creditos (modelo abaixo).
8. Clique em **Publish release**.

**IMPORTANTE**: nao marque `Set as the latest release` em testes ainda nao
validados. Se o GitHub pedir `Target`, use a branch indicada acima.

## 3. Texto sugerido para a Release
### FITD-R36S ARM64 - PortMaster test build
Engine FITD open-source para Alone in the Dark (1992) executado em Linux
AArch64 / OpenGL ES 3.0. Port independente por dehalbuq com base no codigo
de yaz0r e jmimu, com referencias a ReHaunted.

- Otimizacao segura para RK3326/Cortex-A35; binario release sem debug.
- VSync desativado por padrao; limite de renderizacao configurado para ate 60 FPS.
- Timing do motor equivalente ao da Debug anterior; algumas rotinas podem
  acelerar em FPS mais altos. Ainda nao foi validado em todas as fases.
- Usa arquivos comerciais originais de Alone in the Dark 1, NAO inclusos.
- Compatibilidade com outros firmwares ARM64 ainda precisa de testes.
- Nunca declara suporte a ARMv7 ou GLES2.

Instale extraindo o conteudo de fitdr36s.zip na pasta ports/ do firmware,
com o launcher na raiz; adicione seus arquivos originais em fitdr36s/data/.
Faca backup dos saves SAVE*.ITD antes de atualizar.

Licencas: ver pasta fitdr36s/licenses.
Codigo-fonte: https://github.com/dehalbuq/FITD-R36S

## 4. Publicacao no catalogo oficial PortMaster (etapa futura)
Leia: https://portmaster.games/packaging.html

- Obtenha screenshot 4:3 real de gameplay, pelo menos 640x480.
- Preencha capa somente com imagens que incluam gameplay.
- Teste boot, controles, audio, save/load e sair em firmwares ARM64.
- Confira licencas e dependencias reais do binario.
- Verifique unicidade do nome de port `fitdr36s`.
- Confira os arquivos e abra um Pull Request seguindo o guia oficial,
  com a estrutura `port/fitdr36s/` no repositorio de submissao.
- Release no seu GitHub NAO publica automaticamente o jogo no catalogo
  PortMaster.

## 5. Relatorios de teste no dispositivo
```sh
cd /roms/ports/fitdr36s
sh ./check_compatibility.sh
```
O resultado fica em `fitdr36s/compatibility-report.txt`.
Se o firmware usar outro caminho para ROMs, ajuste o `cd`.
