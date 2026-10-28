# Como contribuir com o FITD-R36S

Obrigado pelo interesse em ajudar a corrigir e preservar a jogabilidade original de *Alone in the Dark* no R36S. Este projeto é experimental e recebe contribuições pequenas, reproduzíveis e verificáveis.

## Base certa para desenvolvimento

- **Port ARM64 ativo:** [`fix/orb-hit-object-rehaunted-20261028`](https://github.com/dehalbuq/FITD-R36S/tree/fix/orb-hit-object-rehaunted-20261028).
- **`main`:** port ARMHF antigo. Não abra um PR de correção ARM64 contra `main` sem discutir antes: os arquivos `v2/` e o workflow novo estão na branch ARM64.
- Consulte [README.md](README.md) e [CHANGELOG.md](CHANGELOG.md) para o estado validado, bugs conhecidos e histórico.

## Prioridades de correção

1. **Salas escuras:** recuperar a lógica clássica da iluminação/lamparina. Evite importar renderização HD ou efeitos de pós-processamento.
2. **Câmera da toca do verme:** investigar alvo de câmera, troca de sala e zonas de cobertura após a sequência do verme; não forçar uma câmera sem determinar a causa.
3. **Regressões:** garantir que dano por contato (`HIT_OBJ`), amostra sonora 88, tremor, saves, porta secreta da biblioteca e arremesso do punhal continuem funcionando.
4. **Compatibilidade:** melhorar comportamento em firmwares e variantes R36S documentadas, sem presumir suporte OpenGL ES 2.0.

**Fora do escopo atual:** gráficos HD, novos efeitos de sangue/partículas, tradução, mudanças arbitrárias de dano, refatorações grandes sem teste e redistribuição dos dados do jogo.

## Entendendo o build

O arquivo `v2/build_arm64.sh` clona `yaz0r/FITD` num commit fixado e aplica patches Python de `v2/` **em ordem**. O resultado é um executável `fitd.aarch64` e um ZIP de teste. O workflow [`fitd-r36s-arm64.yml`](https://github.com/dehalbuq/FITD-R36S/blob/fix/orb-hit-object-rehaunted-20261028/.github/workflows/fitd-r36s-arm64.yml) compila em Ubuntu 24.04 **AArch64**.

Principais pontos de entrada:
- `v2/patch_upstream.py`: ajustes de compatibilidade e gameplay do port.
- `v2/patch_debug_audio_crash.py`: diagnóstico e correções de áudio/save.
- `v2/patch_library_room_crash.py`: proteção contra índice de sala inválido.
- `v2/patch_throw_null_body.py`: proteção de modelos 3D ausentes no arremesso.
- `v2/patch_orb_enemy_effects.py`: tremor e rastreamento dos inimigos.
- `v2/patch_worm_camera_orb_audio.py`: amostra sonora dos inimigos e diagnóstico da câmera.
- `v2/patch_hit_object_rehaunted.py`: implementação isolada de `HIT_OBJ` baseada no ReHaunted.
- `v2/port/FITD-R36S-ARM64.sh`: launcher com diagnóstico e ambiente do console.

Quando editar um patch, confira seus pontos de inserção **após os patches anteriores**; um patch que funciona isoladamente pode falhar quando aplicado no script completo. Não modifique os dados `.PAK` comerciais para corrigir lógica do motor.

## Fluxo de trabalho sugerido

1. Faça fork do repositório.
2. Crie uma branch curta a partir de `fix/orb-hit-object-rehaunted-20261028`.
3. Mantenha **uma correção ou diagnóstico por PR**. Evite misturar iluminação, áudio, colisões e câmera na mesma alteração.
4. Garanta que os scripts Python sejam válidos (`python3 -m py_compile v2/<patch>.py`) e que os anchors não tenham mudado.
5. Compile num Linux ARM64 com as dependências do workflow (`bash v2/build_arm64.sh`) ou solicite validação no CI do projeto.
6. Teste num R36S real quando possível. **Build verde confirma compilação, não ausência de bugs no console.**
7. Abra o PR contra a branch ARM64 ativa, descrevendo problema, causa, arquivos modificados, testes e possíveis regressões.

O workflow de push lista branches específicas: commits em branches arbitrárias de forks/PRs podem **não** iniciar automaticamente o Actions. Não marque uma versão como validada em hardware só porque compilou.

## Como reportar um bug

Uma issue útil contém:

```text
Título: [ARM64][área do jogo] resumo do problema
Branch / commit:
Modelo do console e firmware:
Etapa exata do jogo / save:
Passos para reproduzir:
Comportamento esperado:
Comportamento observado:
É repetível? Sim/Não
Log anexo: fitd-debug.log
Crash? Sinal / exit code / trecho final, se houver
```

O launcher grava `fitdr36s/logs/fitd-debug.log` e preserva o anterior como `fitd-debug-previous.log`. Inclua o log **completo**, de preferência anexado à issue, especialmente ao relatar um crash intermitente. Para arquivos muito grandes, destaque o período do evento e anexe o restante. Verifique se o log não contém dados pessoais antes de publicá-lo.

Strings úteis no diagnóstico: `[FITD-HITOBJ]`, `[FITD-ORB]`, `[FITD-EFFECT]`, `[FITD-WORM-CAM]`, `[FITD-LIBRARY]`, `[FITD-THROW]`, `[FITD-SAVE]` e `[FITD-CRASH]`.

## Critérios para aceitar uma correção

- Reproduz ou explica um bug específico, de preferência com log.
- Mantém a jogabilidade original e não inventa dano, eventos ou estados de câmera.
- Trata índices e ponteiros inválidos sem esconder o problema silenciosamente.
- Preserva compatibilidade com save e dados originais, ou documenta claramente qualquer incompatibilidade.
- Não quebra os testes confirmados de áudio, dano por contato, portas, save e menu.
- Inclui logs limitados em frequência quando o evento acontece a cada frame.
- Documenta origem e licença de trechos adaptados de outros projetos.

## Licenças e conteúdo proibido no repositório

**Não envie** arquivos comerciais do jogo (`*.PAK`, `*.ITD` de instalação/saves de terceiros), músicas, vídeos, ROMs, executáveis proprietários ou dados de distribuição não autorizados. Use uma cópia legítima do jogo apenas para testes locais.

Respeite a licença do motor FITD e os créditos dos autores. O `HIT_OBJ` adaptado do [AloneInTheDarkReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted) foi obtido de código GPLv2; preserve atribuição e termos aplicáveis. Não copie todo o remaster quando a correção relevante pode ser implementada isoladamente.
