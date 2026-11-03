# FITD-R36S — publicação da primeira Release ARM64 unificada

## Arquivos e versões

- **Tag sugerida:** `v1.0.0-arm64` (após validação final).
- **ZIP para publicação:** `FITD-R36S-v1.0.0-ARM64-PortMaster.zip`.
- **Verificação SHA-256:** `SHA256SUMS.txt`.
- **Firmware 1:** dArkOSRE / R36S ARM64, biblioteca e executável usuais.
- **Firmware 2:** ArkOS4Clone / Kinhank K36 ARM64 / glibc 2.30, binário separado.
- **Arquivos originais de *Alone in the Dark*:** não incluídos.

## 1. Validar a montagem

Acesse [workflow unificado](https://github.com/dehalbuq/FITD-R36S/actions/workflows/fitd-r36s-dual-arm64-release.yml).
Abra a execução mais recente da branch `release/r36s-aarch64-safe-20261101`.
Verifique o sinal verde em **todas** as etapas.
Em **Artifacts**, baixe `FITD-R36S-v1.0.0-ARM64-PortMaster`.
O ZIP do *GitHub Actions* é apenas um contêiner: extraia para encontrar
`FITD-R36S-v1.0.0-ARM64-PortMaster.zip` e `SHA256SUMS.txt`.

Confirme a soma:

```bash
sha256sum -c SHA256SUMS.txt
unzip -t FITD-R36S-v1.0.0-ARM64-PortMaster.zip
unzip -Z1 FITD-R36S-v1.0.0-ARM64-PortMaster.zip | grep -E '(^FITD-R36S-ARM64.sh$|^fitdr36s/fitd.*aarch64$|^fitdr36s/libs.*libstdc\+\+)'
```

**Não** publique o ZIP externo de Artifacts por engano. Publique o ZIP interno
de instalação, que possui `FITD-R36S-ARM64.sh` e `fitdr36s/` na raiz.

## 2. Testes finais antes de chamar de estável

Copie o ZIP interno para `/roms/ports/` do console, sem sobrescrever
`fitdr36s/data/` nem arquivos `SAVE*.ITD`. Faça backup antes.

Confira separadamente no ArkOS4Clone **e** no dArkOSRE:
inicialização, controles, áudio, mudança de salas, menu, salvar/carregar,
saída do jogo e regressões gráficas. A inicialização no ArkOS4Clone foi
confirmada pelo autor; não trate isso como prova de todos os testes.

O launcher seleciona por `getconf GNU_LIBC_VERSION`. Para firmware
glibc 2.30 ou anterior, ele escolhe `fitd.glibc230.aarch64` e
`libs.glibc230/`. Para firmware mais recente, usa `fitd.aarch64`
e `libs.aarch64/`. Não distribua glibc, libEGL ou GPU drivers do sistema.

## 3. Criar a Release no GitHub

1. Acesse https://github.com/dehalbuq/FITD-R36S/releases/new .
2. Escolha **Choose a tag**, digite `v1.0.0-arm64`. Crie a tag a partir
   da branch `release/r36s-aarch64-safe-20261101`, apontando para
   o commit validado, **não** para `main` (que mantém ARMHF legado).
3. Título sugerido: `FITD-R36S v1.0.0 — PortMaster ARM64 (dArkOSRE + ArkOS4Clone)`.
4. Cole o texto de `v2/RELEASE_NOTES_V1.0.0.md` na descrição.
5. Faça upload dos **dois** arquivos: ZIP interno e `SHA256SUMS.txt`.
6. Se testes de áudio, controles, salvamento e saída ainda não estiverem
   concluídos em ambos, marque **Set as a pre-release**, e não estável final.
7. Quando validado, desmarque pre-release e publique. Se possível selecione
   **Set as the latest release**.
8. Confira o link público em https://github.com/dehalbuq/FITD-R36S/releases .

## 4. Instalação para os usuários

Extrair o ZIP para `/roms/ports/` (ou diretório configurado no firmware).
Copiar dados de uma instalação legítima do jogo para
`/roms/ports/fitdr36s/data/`. Abrir `FITD-R36S-ARM64.sh` em Ports.
O pacote **não contém** o jogo comercial.

## 5. Segurança e manutenção

- Preserve o histórico da versão ARMHF na branch `main`.
- Um único pacote ARM64 inclui as duas arquiteturas de dependência de
  **mesmo processador**, selecionadas pela versão da glibc.
- Não declare aprovação oficial do PortMaster antes da submissão.
- A campanha completa ainda não foi validada em ambos os firmwares.
- Nunca publique saves ou arquivos PAK/ITD comerciais.
