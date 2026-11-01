# FITD Android 4.4 / ARMv7 — migração API 19

**Status: preparação técnica, não é um APK funcional.** Esta branch deriva de `port/android-armeabi-v7a-release-20261031`, cujo protótipo exige Android 5.0/API 21, SDL3 e OpenGL ES 3.0. Não execute o workflow existente esperando compatibilidade com KitKat.

## Meta

- `minSdkVersion 19` e `ANDROID_PLATFORM=android-19` em todas as bibliotecas nativas.
- ABI exclusivamente `armeabi-v7a`; não exigir NEON incondicionalmente em aparelhos antigos.
- OpenGL ES 2.0 real, com shaders e backend bgfx compatíveis; não basta alterar a versão de contexto EGL ou o Manifest.
- Gamepad Bluetooth/USB via SDL2, com reconexão, D-pad, analógico, botões e menu.
- APK Release sem símbolos de debug, sem overlay de diagnóstico, com FPS limitado a 60.
- Importação de dados originais via seletor de documentos, saves privados preservados; APK não inclui arquivos comerciais.
- Preservar créditos, licenças e correções funcionais do motor da branch de origem.

## Bloqueadores identificados no protótipo

1. `v2/android/prepare_android.py` modifica SDL3 e usa funções `SDL_Gamepad*`, `SDL_GetGamepads`, `SDL_GetAndroidInternalStoragePath` e outras APIs SDL3; migrar cuidadosamente para SDL2 e sua atividade Java, incluindo diferenças de assinaturas e eventos.
2. O projeto Gradle vem de `ThirdParty/SDL/android-project` (SDL3). Substituir por template SDL2 com suporte documentado a API 19 e adaptar integração JNI `libmain.so`.
3. `prepare_android.py` fixa `ANDROID_PLATFORM=android-21`, solicita contexto ES 3.0 e altera o Manifest para exigir GLES3. Reescrever a configuração para API 19/ES2, com verificação em tempo de execução.
4. O motor e os shaders bgfx atuais foram preparados para GLES3. Auditar versões do bgfx, shaders, atributos de textura, extensões e caminho de renderização para GLES2 antes de anunciar suporte.
5. Auditar SoLoud, zlib e demais dependências para APIs Android <=19 e compatibilidade de CMake/NDK; selecionar toolchain que ainda aceite API 19.
6. O script de compilação cria uma chave de assinatura temporária a cada execução. Builds instaláveis sucessivamente precisam de keystore estável mantido fora do repositório.
7. `FITDImportActivity.java` e acesso aos arquivos devem ser testados no Android 4.4, incluindo documentos em provedores diferentes e restauração dos saves.

## Sequência de implementação

1. Criar `v2/android-kitkat/` e um workflow separado, sem modificar o workflow ARMv7/GLES3 anterior.
2. Construir primeiro um APK SDL2 ARMv7/API19 mínimo que inicialize vídeo ES2, áudio, armazenamento e gamepad.
3. Integrar o motor FITD e suas correções, compilando `libmain.so` para Android com todas as dependências.
4. Adaptar renderizador e shaders bgfx para GLES2, com testes em GPU real.
5. Integrar importação de dados, controles, tela legal e persistência de saves.
6. Validar APK com `aapt` (minSdk 19), `readelf` (ELF 32-bit ARM), `unzip` (somente ABI desejada, sem dados comerciais), instalar/testar em Android 4.4 real.
7. Só depois ativar Release otimizada (O2/O3 medido, LTO quando suportado, strip) e publicar artefato assinado de teste.

## Critérios de aceitação

- Instala e abre em Android 4.4 real, ARMv7, GPU limitada a OpenGL ES 2.0.
- Importa os dados legais do usuário, entra no jogo, movimenta personagem, toca áudio, salva e carrega.
- Reconhece controle Bluetooth antes e depois da inicialização; botões de saída funcionam.
- Não há crashes nos trechos já corrigidos no port R36S; FPS não ultrapassa 60.
- Nenhum `.PAK`/`.ITD` comercial é distribuído.
- Artefato de CI validado, com commit e versão de ferramentas registrados.

Referência: https://wiki.libsdl.org/SDL2/README-android (SDL2, mínimo API 19). Atenção: a linha SDL mais recente pode exigir API 21.
