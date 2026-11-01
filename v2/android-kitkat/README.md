# Teste independente SDL2 + OpenGL ES 2.0 para Android 4.4 (API 19)

Este diretório contém **somente um smoke test**, não o motor FITD. O aplicativo de teste mostra um triângulo verde com shaders GLSL ES 1.00 durante até 30 segundos; START de gamepad ou Escape encerram o teste. Não inclui recursos comerciais.

## Projeto Android reproduzível

O projeto Gradle fica em `android/` com arquivos próprios e estáveis (Gradle **8.1.1**, Android Gradle Plugin **8.1.1**, Java **17**, `minSdk 19`, ABI **armeabi-v7a**). O workflow `.github/workflows/android-kitkat-gles2.yml`:

1. Compila SDL2 `release-2.30.12` usando NDK r23c para ARMv7/API 19.
2. Compila `gles2_probe.cpp` como `libmain.so` e copia o Java do SDL2 e ambas as bibliotecas para o projeto Gradle.
3. Produz `FITD-KitKat-GLES2-ARMv7-TEST.apk` e valida SDK, GLES e ABI com `aapt`, além de verificar as bibliotecas dentro do APK.
4. Publica o APK no artefato GitHub Actions `fitd-kitkat-gles2-armv7-apk-test`.

Nenhuma dependência da tarefa `externalNativeBuild` do modelo SDL2 é necessária. O APK só deve ser chamado de compilado depois que o workflow terminar com sucesso.

## Teste em dispositivo real

Instalar com `adb install -r FITD-KitKat-GLES2-ARMv7-TEST.apk`. Abrir **FITD GLES2 Test** no aparelho e procurar o triângulo verde. Conferir a saída de logs do SDL2 em `adb logcat` e testar START do controle Bluetooth, além de suspensão/retomada.

**Importante:** o teste não executa Alone in the Dark, não usa dados do jogo, não valida bgfx e não demonstra ainda que o motor FITD funciona em Android 4.4. Essa migração só virá depois de confirmar este teste.
