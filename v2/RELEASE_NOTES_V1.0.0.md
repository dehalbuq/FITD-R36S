# FITD-R36S v1.0.0 — PortMaster ARM64 (dArkOSRE + ArkOS4Clone)

Primeira versão ARM64 unificada do port comunitário FITD para consoles R36S e compatíveis.

## Destaques
- Um só pacote PortMaster ARM64 para **dArkOSRE** e **ArkOS4Clone**.
- Seleção automática do executável e das bibliotecas por versão da glibc.
- Variante dArkOSRE com executável ARM64 habitual (`fitd.aarch64`).
- Variante ArkOS4Clone (`fitd.glibc230.aarch64`) compilada sobre glibc 2.28; símbolos auditados para execução em glibc 2.30.
- Telas de créditos e aviso legal; fonte legível opcional; correções e proteções da linha FITD-R36S.
- Sem arquivos comerciais do jogo.

## Compatibilidade e limitações

| Firmware | Estado |
|---|---|
| dArkOSRE | Funcionamento reportado em testes anteriores no R36S; pacote unificado aguarda teste completo |
| ArkOS4Clone / Kinhank K36 | Inicialização confirmada no dispositivo; pacote unificado aguarda teste completo |

**Atenção:** a versão não foi aprovada oficialmente pelo projeto PortMaster. A campanha inteira, todos os saves, áudio, controles e desempenho em ambos os firmwares não estão integralmente verificados. OpenGL ES 3.0 e ARM64 são necessários; GLES 2.0 e ARMHF não são suportados neste pacote.

## Como instalar

1. Faça backup da pasta `fitdr36s/data/`, dos saves e do launcher existente.
2. Extraia **FITD-R36S-v1.0.0-ARM64-PortMaster.zip** na pasta de ports, normalmente `/roms/ports/`.
3. Copie os arquivos de uma cópia legítima do *Alone in the Dark* original para `fitdr36s/data/`.
4. Execute `FITD-R36S-ARM64.sh` no menu Ports.

O pacote inclui apenas o motor comunitário, scripts e bibliotecas redistribuíveis. **Não inclui o jogo original** e não é afiliado aos titulares da franquia.

## Créditos

- Jogo original: *Alone in the Dark* (1992), Infogrames.
- Motor FITD: jmimu e yaz0r.
- Correções específicas de R36S/PortMaster: dehalbuq.
- Código de referência adaptado, quando indicado no repositório: AloneInTheDarkReHaunted.

Licenças e atribuições acompanham o pacote. Código-fonte, changelog e instruções: https://github.com/dehalbuq/FITD-R36S .

**Arquivos anexos à Release:** `FITD-R36S-v1.0.0-ARM64-PortMaster.zip` e `SHA256SUMS.txt`.
