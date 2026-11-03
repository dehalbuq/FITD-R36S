# Publicar a primeira Release FITD-R36S v1.0.0 (dArkOSRE + ArkOS4Clone)

Repositório: **https://github.com/dehalbuq/FITD-R36S**

Branch que contém o port ARM64: `release/r36s-aarch64-safe-20261101`

Esta é a **primeira distribuição unificada ARM64** para os firmwares dArkOSRE e ArkOS4Clone. É uma publicação do projeto no GitHub; **não** significa aprovação no catálogo oficial PortMaster. Não inclui arquivos comerciais de *Alone in the Dark*.

## 1. Preparar os três arquivos da Release

O método recomendado é deixar o GitHub Actions empacotar os dois builds validados.

1. Abra **[FITD-R36S v1.0.0 Dual ARM64 PortMaster Release](https://github.com/dehalbuq/FITD-R36S/actions/workflows/fitd-r36s-dual-release.yml)**.
2. Escolha **Run workflow** e selecione a branch `release/r36s-aarch64-safe-20261101`.
3. Execute e abra a execução mais recente.
4. Só continue se todas as etapas estiverem **verdes**: download de ambos os builds, SHA256 de origem, verificação dos ELF AArch64 e ZIP final.
5. Em **Artifacts**, baixe `FITD-R36S-v1.0.0-DUAL-ARM64-PORTMASTER`.
6. Extraia o **ZIP externo** que o GitHub Actions fornece. Dentro dele estarão os **três arquivos** que devem ser anexados à Release:
   - `fitdr36s.zip` — pacote final para instalar na pasta `ports/`;
   - `SHA256SUMS.txt` — hash SHA-256 do ZIP;
   - `RELEASE_NOTES_v1.0.0.md` — texto pronto para a descrição.

**Não publique o ZIP externo do artefato.** Publique apenas os arquivos acima.

O workflow usa compilações confirmadas nos runs [moderno #41561105602](https://github.com/dehalbuq/FITD-R36S/actions/runs/41561105602) e [legacy #41562095088](https://github.com/dehalbuq/FITD-R36S/actions/runs/41562095088). Os artefatos do Actions expiram; por isso, depois que a Release estiver publicada, os assets do GitHub Releases serão a distribuição permanente.

### Conferência da integridade

No Linux/WSL, estando na pasta dos arquivos:

```bash
sha256sum -c SHA256SUMS.txt
unzip -t fitdr36s.zip
unzip -l fitdr36s.zip
```

No PowerShell/Windows:

```powershell
Get-FileHash .\fitdr36s.zip -Algorithm SHA256
```

Compare esse valor com a primeira coluna de `SHA256SUMS.txt`. Também é possível abrir o ZIP com 7-Zip e testar sua integridade.

Confira no ZIP:

```text
FITD-R36S-ARM64.sh
port.json
gameinfo.xml
README.md
fitdr36s/fitd.aarch64
fitdr36s/libs.aarch64/
fitdr36s/fitd.glibc230.aarch64
fitdr36s/libs.glibc230/
fitdr36s/data/
fitdr36s/licenses/
```

Não devem existir arquivos originais comerciais do jogo (`.PAK`, `.ITD`), ROMs, ISOs ou saves incluídos.

## 2. Criar uma Release no seu GitHub

1. Abra **https://github.com/dehalbuq/FITD-R36S/releases/new**.
2. Clique em **Choose a tag** → digite `v1.0.0` → **Create new tag**.
3. Em **Target**, escolha **`release/r36s-aarch64-safe-20261101`**. Isso é importante: a branch `main` ainda mantém código ARMHF histórico.
4. Em **Release title**, escreva:
   **`FITD-R36S v1.0.0 — ARM64 dArkOSRE + ArkOS4Clone`**
5. Na descrição, cole o conteúdo de **`RELEASE_NOTES_v1.0.0.md`**.
6. Em **Attach binaries by dropping them here or selecting them**, anexe `fitdr36s.zip` e `SHA256SUMS.txt`. Opcionalmente anexe também o próprio arquivo `.md`.
7. Escolha **Save draft** para conferir os arquivos e os links de download, especialmente se sua conta exigir Releases imutáveis.
8. Quando estiver tudo correto, clique em **Publish release**. Para anunciar como primeira versão final, não marque **Set as a pre-release**; não confunda isso com validação completa da campanha.

O GitHub criará a página `https://github.com/dehalbuq/FITD-R36S/releases/tag/v1.0.0` após a publicação. Esse endereço só funcionará depois que a tag/Release existir.

## 3. Instalação que os usuários finais verão

- Baixe `fitdr36s.zip` na Release.
- Faça backup de `fitdr36s/data/` e dos arquivos `SAVE*.ITD`.
- Extraia **o conteúdo** do ZIP na pasta de Ports do seu firmware, tipicamente `/roms/ports/`.
- Copie os arquivos de sua própria cópia original de *Alone in the Dark 1* para `fitdr36s/data/`.
- Execute `FITD-R36S-ARM64.sh` no menu Ports.

Seleção automática do launcher:
- **ArkOS4Clone, glibc 2.30:** `fitd.glibc230.aarch64` + `libs.glibc230/`;
- **dArkOSRE com glibc >=2.35:** `fitd.aarch64` + `libs.aarch64/`.

Em outros sistemas, o comportamento dependerá de arquitetura, versões de bibliotecas, SDL/KMSDRM e GPU.

## 4. Critérios mínimos para anunciar como v1.0.0

- [ ] ZIP unificado com checksum validado e **os dois** binários presentes;
- [ ] Boot, controles, som, save/load e sair testados em dArkOSRE com **o ZIP final**;
- [ ] Boot, controles, som, save/load e sair testados em ArkOS4Clone com **o ZIP final**;
- [ ] Backup dos saves testado e documentação sem dados comerciais do jogo;
- [ ] Limitações conhecidas e créditos declarados na descrição da Release.

Os dois executáveis têm ABI auditada, e o usuário confirmou a inicialização do ArkOS4Clone. **Isso não substitui o smoke test do ZIP combinado.** Se ainda não cumpriu todos os itens, prefira publicar como **pre-release `v1.0.0-rc1`**, depois marque a final após a validação.

## 5. Publicação no catálogo PortMaster (separada)

Leia as regras atuais em **https://portmaster.games/packaging.html**. A publicação no seu GitHub não coloca o port automaticamente no catálogo oficial. Para submissão, o PortMaster exige, entre outros pontos:

- Screenshot **real** de gameplay 4:3 (mínimo 640×480), sem usar imagens geradas para substituí-la;
- testes documentados em firmwares e resoluções exigidos pela comunidade;
- `port.json`, `gameinfo.xml`, README, launcher, licenças e estrutura correta;
- revisão e Pull Request no repositório de submissões apropriado.

O arquivo `SUBMISSION_BLOCKERS.txt` pode permanecer no ZIP para deixar claro que essas condições não estão todas satisfeitas. **Não apague esse aviso só para parecer aprovado.**

## 6. Fontes e como reconstruir

- Fonte e patches: branch `release/r36s-aarch64-safe-20261101`;
- script de montagem: `v2/assemble_dual_release.py`;
- workflow de pacote final: `.github/workflows/fitd-r36s-dual-release.yml`;
- build da variante moderna: `.github/workflows/fitd-r36s-arm64.yml`;
- build da variante antiga: `.github/workflows/fitd-arkos4clone-arm64-legacy.yml`;
- notas para copiar: `v2/RELEASE_NOTES_v1.0.0.md`.

**Observação:** o workflow atual de montagem usa dois runs específicos já concluídos. Para futuras versões, reconstrua ambos os binários e ajuste os IDs dos runs para que o pacote sempre reflita a mesma versão do código.
