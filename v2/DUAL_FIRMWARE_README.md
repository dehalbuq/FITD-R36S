# FITD-R36S v1.0.0 — ARM64 (dArkOSRE + ArkOS4Clone)

Motor FITD de código aberto para **Alone in the Dark (1992)** no R36S / RK3326.

**Não acompanha o jogo comercial.** Você deve fornecer os arquivos originais de sua cópia legítima.

## Instalação

1. Faça backup de `/roms/ports/fitdr36s/`, sobretudo `SAVE*.ITD`.
2. Extraia **o conteúdo** de `fitdr36s.zip` em `/roms/ports/`.
3. Copie seus arquivos comerciais originais para `fitdr36s/data/`.
4. Abra `FITD-R36S-ARM64.sh` no menu Ports.

## Variantes selecionadas automaticamente

| Firmware / glibc | Arquivo |
| --- | --- |
| ArkOS4Clone, glibc 2.30; ambientes 2.30–2.34 | `fitd.glibc230.aarch64` e `libs.glibc230/` |
| dArkOSRE e ambientes com glibc >=2.35 | `fitd.aarch64` e `libs.aarch64/` |

A inicialização do ArkOS4Clone foi confirmada por teste em console. O dArkOSRE tem histórico de testes progressivos no R36S; a compilação moderna foi aprovada no GitHub Actions. **O ZIP reunindo as duas variantes ainda exige testes físicos completos em ambos os sistemas.**

O launcher não substitui glibc, EGL, GBM, DRM nem drivers Mali do firmware.

## Diagnóstico

```sh
FITD_RELEASE_LOG=1 bash /roms/ports/FITD-R36S-ARM64.sh
sh /roms/ports/fitdr36s/check_compatibility.sh
```

Logs: `fitdr36s/logs/fitd-release.log`, `fitdr36s/compatibility-report.txt`.

## Recursos e limitações

- Linux AArch64, SDL3/KMSDRM, BGFX e OpenGL ES 3.0.
- Versão Release com correções de estabilidade, fonte alternativa CP850, créditos e avisos legais.
- Otimizações conservadoras; limite de renderização padrão de 60 FPS e VSync desligado.
- **A campanha completa não foi homologada.** Algumas áreas, iluminação, sequências de câmera e sincronização de tempo merecem testes adicionais.
- OpenGL ES 2.0 e sistemas ARMv7/ARMHF **não** fazem parte desta versão.
- **Este ZIP para GitHub Releases não é submissão oficial ao PortMaster.** Uma screenshot real de gameplay 640x480 (4:3) e testes comunitários continuam necessários.

## Créditos e direitos

Port ARM64 / integração RK3326: **dehalbuq** e colaboradores. Motor FITD: **yaz0r** e **jmimu**. Referências de correções: **AloneInTheDarkReHaunted**. Jogo original: **Infogrames** e respectivos titulares.

Código: https://github.com/dehalbuq/FITD-R36S

Licenças de código e terceiros: `fitdr36s/licenses/`. Não são fornecidos PAKs, saves ou outros recursos comerciais do jogo.
