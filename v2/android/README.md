# FITD Android ARMv7 — port experimental (32 bits)

**Branch separada:** `port/android-armeabi-v7a-release-20261031`.

## Escopo e compatibilidade

Este é um **port experimental**, independente dos executáveis Linux do R36S, para Android 5.0 (API 21) ou superior, arquitetura **ARMv7 32 bits / `armeabi-v7a`** e GPU com **OpenGL ES 3.0**. Ser ARMv7 não garante GLES3: aparelhos limitados ao GLES 2.0 **ainda não são suportados** por esta primeira versão do renderizador bgfx.

O APK usa SDL3 para contexto EGL/OpenGL ES e entradas de gamepad do Android. Após emparelhar um controle Bluetooth pelo sistema, o SDL recebe o controle como gamepad, incluindo hotplug e analógico esquerdo/direcional. O mapeamento experimental é: direcional/analógico para movimento, A ou RB para ação, B ou START para menu/voltar, Y para confirmar. Teste esse mapeamento no dispositivo.

## Apenas motor; sem jogo comercial

O APK contém somente o motor de código aberto e uma tela nativa que importa arquivos fornecidos pelo usuário. **Não distribui Alone in the Dark, seus arquivos `.PAK`/`.ITD`, músicas, gráficos nem recursos comerciais.** Para jogar, obtenha legalmente os dados originais do jogo e use a opção **Importar minha pasta do jogo original** no menu inicial. O Android Storage Access Framework copia os dados para a pasta privada do aplicativo, acessível ao motor sem permissões amplas de armazenamento. Os saves existentes não são sobrescritos durante a importação.

A desinstalação do aplicativo pode apagar os saves privados. Faça backup.

## Perfil de compilação

- `CMAKE_BUILD_TYPE=Release`, `-DNDEBUG`, sem os `FITD_DEEP_TRACE`/`FITD_PERF_TRACE` e demais patches de instrumentação do Linux/R36S.
- `armeabi-v7a` exclusivamente; não inclui `arm64-v8a` nesta versão.
- Gradle `assembleRelease`, `debuggable=false`, shader compilado previamente no x86_64 do runner e executável JNI nativo `libmain.so`.
- Assinatura da compilação de teste é **temporária**, criada automaticamente pelo CI. Para versões atualizáveis sem desinstalar, configure um keystore privado e estável, fora do repositório, via Secrets do GitHub Actions. Não distribua uma chave privada de assinatura.
- A compilação e a execução dependem de compatibilidade do SDL3, bgfx e SoLoud com o NDK selecionado. O CI tem que concluir antes que qualquer APK seja anunciado como funcional.

## Fontes e licenças

- Motor original: [yaz0r/FITD](https://github.com/yaz0r/FITD) e histórico de [jmimu/FITD](https://github.com/jmimu/FITD); licenças GPL e avisos dos arquivos originais.
- Port ARMv7 e integração Android: [dehalbuq/FITD-R36S](https://github.com/dehalbuq/FITD-R36S).
- SDL3, bgfx, SoLoud e demais terceiros: licenças próprias incluídas nas respectivas fontes.
- Dados de *Alone in the Dark* permanecem comerciais e fora do pacote.

## Build reproduzível

Abra o workflow `.github/workflows/android-armv7-release.yml`. Ele utiliza um SDK/NDK Android de CI, clona o upstream FITD em commit fixo, compila shaders no host, aplica `v2/android/prepare_android.py`, executa `v2/android/build_android.sh` e publica `FITD-ANDROID-ARMV7-RELEASE-APK-TEST` somente após build e checagem do pacote bem-sucedidos.

**Situação atual:** primeiro port compilável em desenvolvimento. Não considere compatibilidade física, performance ou suporte GLES2 confirmados antes de teste em aparelho ARMv7.
