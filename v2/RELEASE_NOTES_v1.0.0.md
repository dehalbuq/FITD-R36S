# FITD-R36S v1.0.0 — primeira Release ARM64 unificada

**Alone in the Dark (1992) — motor FITD de código aberto para R36S / RK3326**

Esta versão reúne, **no mesmo pacote PortMaster**, os executáveis ARM64 para **dArkOSRE** e **ArkOS4Clone**. O launcher escolhe automaticamente a variante apropriada com base na versão da glibc.

### Compatibilidade

- **dArkOSRE:** mantém a compilação moderna do FITD, com histórico de execução e testes de gameplay no R36S.
- **ArkOS4Clone (Kinhank K36):** usa compilação AArch64 independente, produzida com glibc 2.28 e verificada para o sistema glibc 2.30; inicialização confirmada em hardware real.
- **glibc 2.30–2.34:** seleciona `fitd.glibc230.aarch64` e `libs.glibc230/`.
- **glibc 2.35+:** seleciona `fitd.aarch64` e `libs.aarch64/`.
- Outros CFWs e dispositivos ainda requerem testes; ARMv7/ARMHF e OpenGL ES 2.0 não estão incluídos.

### Recursos

- Engine FITD, renderização SDL3/KMSDRM/OpenGL ES 3.0 e otimizações conservadoras para RK3326.
- Correções progressivas para estabilidade de saves, áudio, colisões e outras situações.
- Fonte legível opcional, modo clássico de máscaras, splash de créditos/aviso legal e configurações por arquivo.
- Binários Release sem seções de debug; scripts, licença e atribuições incluídos.

### Instalação

1. Baixe o arquivo **`fitdr36s.zip`** desta Release (não o ZIP do artefato do GitHub Actions).
2. Faça backup da instalação existente e dos saves (`SAVE*.ITD`).
3. Extraia o conteúdo de `fitdr36s.zip` na pasta `ports/` do firmware, por exemplo `/roms/ports/`.
4. Copie os arquivos de sua **própria cópia original de Alone in the Dark 1** para `fitdr36s/data/`.
5. Abra **`FITD-R36S-ARM64.sh`** no menu Ports.

Não sobrescreva `data/` ou os saves ao atualizar. O jogo comercial e seus arquivos `.PAK` **não estão incluídos**.

### Integridade

Baixe também **`SHA256SUMS.txt`** e valide usando `sha256sum -c SHA256SUMS.txt` (Linux/WSL), ou compare o SHA-256 no Windows com `Get-FileHash .\fitdr36s.zip -Algorithm SHA256`.

### Limites e reporte de bugs

- **A campanha completa não foi validada.** Ainda há áreas em investigação, como iluminação e certas sequências de câmera; pode ocorrer variação de tempo de jogo com FPS elevados.
- A inicialização da variante ArkOS4Clone foi confirmada; o novo ZIP unificado ainda deve ser testado com controles, áudio, save/load e saída em ambos os CFWs.
- Esta Release no GitHub **não representa aprovação ou publicação no catálogo oficial PortMaster**; screenshots reais e a rodada de testes oficial são etapas separadas.
- Diagnóstico: execute `FITD_RELEASE_LOG=1 bash /roms/ports/FITD-R36S-ARM64.sh` e envie `fitdr36s/logs/fitd-release.log`, informando seu firmware e o cenário do problema.

### Créditos / legal

Port e integração RK3326: **dehalbuq**. Motor FITD: **yaz0r** e **jmimu**. Referências de correção: projeto **AloneInTheDarkReHaunted**. Jogo comercial original: **Infogrames** e titulares pertinentes. Fontes e avisos de terceiros acompanham o pacote.

**Código-fonte:** https://github.com/dehalbuq/FITD-R36S

> A distribuição contém apenas o motor, scripts, recursos próprios do port e bibliotecas necessárias. Os arquivos comerciais do jogo devem ser fornecidos legalmente pelo usuário.
