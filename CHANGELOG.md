# Histórico de desenvolvimento — FITD-R36S

Registro dos testes e correções do **port ARM64 experimental**. Não são lançamentos estáveis ou certificados pelo PortMaster. O histórico de compilação está no [GitHub Actions](https://github.com/dehalbuq/FITD-R36S/actions).

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
