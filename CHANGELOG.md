# Histórico de desenvolvimento — FITD-R36S

Registro dos testes e correções do **port ARM64 experimental**. Não são lançamentos estáveis ou certificados pelo PortMaster. O histórico de compilação está no [GitHub Actions](https://github.com/dehalbuq/FITD-R36S/actions).

## 2026-10-30 — Reversão da máscara de água experimental; comparação ReHaunted

**Branch:** `fix/rehaunted-mask-reptile-diagnostic-20261030` · **Status:** teste pendente no R36S.

- O experimento anterior que estendia a máscara do primeiro plano para o **centro da ZV** não resolveu a imersão e foi removido desta branch. A checagem por caixa inteira continua idêntica ao FITD original e ao ReHaunted.
- O log `fitd-debug(20261030-120654).log` confirma ausência do ator world **228** na sala 7/andar 5, enquanto o log anterior o mostrava como ator 2 / life 476. O novo teste carregou `SAVE0.ITD`; o anterior atravessou a campanha em sessão completa. **Não forçar** spawn sem confirmar o estado persistido do objeto.
- A altura do jogador muda entre `-4010`, `-2010` e `-10` nessa sala, associada a scripts de movimento. Não alterar coordenadas Y arbitrariamente.
- Portadas somente opções de **máscara SD** do ReHaunted em `rendererBGFX.cpp`: `BGFX_STATE_WRITE_Z` e `BGFX_STATE_DEPTH_TEST_ALWAYS`; sampler de máscara alinhado ao nome da variável declarada em `maskBackground_ps.sc`. Textura R8U/sampler inteiro original preservados: sem renderização HD ou texturas adicionais.
- Novo diagnóstico `[FITD-REPTILE] WORLD228` e `ACTOR228` identifica estado do objeto não ativado. `[FITD-REPTILE] WATER_MASK` registra máscaras originais usadas na sala. Repetir com o mesmo save e, se possível, com save anterior à entrada na toca do verme.

A presença do monstro e a imersão visual continuam **não validadas** até novo teste real.

## 2026-10-28 — Dano por contato validado

**Branch de referência:** [`fix/orb-hit-object-rehaunted-20261028`](https://github.com/dehalbuq/FITD-R36S/tree/fix/orb-hit-object-rehaunted-20261028) — commit inicial validado `81e7f3c`. [Workflow bem-sucedido](https://github.com/dehalbuq/FITD-R36S/actions/runs/40771399610).

- Implementado `case 8 / HIT_OBJ` em `GereFrappe()`, adaptando a parte de colisão da implementação do [ReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted) (GPLv2), **sem** sangue, partículas ou novos efeitos.
- O motor passa a registrar alvo, origem e força do golpe para processamento pelos scripts originais (`HIT`, `HIT_BY`, `hitForce`).
- **Teste real no R36S:** usuário confirmou que o dano dos inimigos do salão e o efeito sonoro passaram a funcionar perfeitamente.
- Diagnóstico `[FITD-HITOBJ] CONTACT_DAMAGE` incluído para regressões.

## 2026-10-28 — Diagnósticos anteriores preservados

| Branch | Mudança principal |
| --- | --- |
| `debug/worm-camera-orb-audio-20261028` | Reprodução controlada da amostra sonora 88; rastreamento do alvo/zonas de câmera da toca do verme |
| `debug/ball-enemies-hit-shake-20261028` | Primeiro tremor de tela com `LM_SHAKING`; rastreamento dos eventos de contato e dano |
| `debug/throw-null-body-20261028` | Proteção e diagnóstico de ponteiros de modelos 3D inválidos no arremesso |
| `debug/library-room-crash-20261028` | Proteção contra acesso a sala inexistente ao processar colisões |
| `debug/audio-door-room-crash-20261027` | Correções de áudio de portas, estabilidade de save e diagnóstico de falhas |

As branches acima são marcos de teste. A branch de referência mais recente **acumula essas correções**.

### Bugs conhecidos (ainda abertos)

- **Iluminação:** biblioteca e outras salas que deveriam estar escuras aparecem claras. Correção adiada, sem implementação incorporada até esta revisão.
- **Câmera na toca do verme:** após a sequência do verme, a câmera pode permanecer presa na cena anterior. Há diagnósticos, mas não foi comprovada uma correção.
- **Cobertura geral:** a campanha inteira ainda não foi validada; outras regressões podem aparecer em áreas posteriores.

## 2026-10 — Port ARM64 em desenvolvimento

- Novo fluxo de build baseado no [yaz0r/FITD](https://github.com/yaz0r/FITD), compilado para Linux AArch64 em runner nativo.
- Renderização via SDL/KMSDRM e bgfx/OpenGL ES 3.0 em dArkOSRE.
- Pacote de teste com launcher, diagnóstico, logs e dependências auxiliares.
- Dados originais do jogo continuam necessários; não são redistribuídos.

## Linha legada ARMHF

A branch `main` mantém o experimento anterior de ARMv7/ARMHF, gl4es e sdl12-compat. Este histórico e as correções recentes descrevem a **linha ARM64**; não devem ser interpretados como alterações aplicadas ao código ARMHF.

Para reproduzir, reportar um bug ou ajudar: [README.md](README.md) · [CONTRIBUTING.md](CONTRIBUTING.md).
