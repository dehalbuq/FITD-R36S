# Teste independente SDL2 + OpenGL ES 2.0 para Android 4.4 (API 19)

Este diretório contém **somente um smoke test**, não o motor FITD. O aplicativo de teste mostra um triângulo móvel com shaders GLSL ES 1.00 durante até 90 segundos. O teste abre um controle SDL2 conectado (Bluetooth/USB), inclusive controles que chegam como *joystick* genérico, e reage ao direcional, analógico esquerdo e botões. START ou B encerram, A muda a cor e a direção/analógico movimentam o triângulo. Não inclui recursos comerciais.

## Projeto Android reproduzível

O projeto Gradle fica em `android/` com arquivos próprios e estáveis (Gradle **8.1.1**, Android Gradle Plugin **8.1.1**, Java **17**, `minSdk 19`, ABI **armeabi-v7a**). O workflow `.github/workflows/android-kitkat-gles2.yml`:

1. Compila SDL2 `release-2.30.12` usando NDK r23c para ARMv7/API 19.
2. Compila `gles2_probe.cpp` como `libmain.so` e copia o Java do SDL2 e ambas as bibliotecas para o projeto Gradle.
3. Produz `FITD-KitKat-GLES2-ARMv7-TEST.apk` e valida SDK, GLES e ABI com `aapt`, além de verificar as bibliotecas dentro do APK.
4. Publica o APK no artefato GitHub Actions `fitd-kitkat-gles2-armv7-apk-test`.

Nenhuma dependência da tarefa `externalNativeBuild` do modelo SDL2 é necessária. O APK só deve ser chamado de compilado depois que o workflow terminar com sucesso.

## Teste em dispositivo real

Instalar com `adb install -r FITD-KitKat-GLES2-ARMv7-TEST.apk`. Abrir **FITD GLES2 Test** no aparelho e procurar o triângulo verde. Emparelhar um controle Bluetooth **nas configurações do Android antes de abrir o app**. O D-pad ou o analógico esquerdo deve movimentar o triângulo; A altera verde para avermelhado; START ou B encerram. Desconectar e reconectar o controle também deve funcionar. Se o controle surgir apenas como joystick genérico, o teste usa os dois primeiros eixos e botões 0/1. Confira as mensagens `FITD-ANDROID-PAD` e `FITD-ANDROID-GLES2` em `adb logcat`, além de testar suspensão/retomada.

**Importante:** o teste não executa Alone in the Dark, não usa dados do jogo, não valida bgfx e não demonstra ainda que o motor FITD funciona em Android 4.4. Essa migração só virá depois de confirmar este teste.

## Próximo passo: transformar o teste no motor FITD real

Este APK **não é o jogo**. Ele valida o caminho Android 4.4/API19 -> SDL2 -> GLES2 -> controles. Os próximos trabalhos, em ordem:

1. Migrar a camada de plataforma de `FitdLib/osystemSDL.cpp`, `input.cpp`, `bgfxGlue.cpp` e integrações ImGui de **SDL3 para SDL2**, preservando o projeto Linux/PortMaster sem modificações.
2. Compilar o motor FITD como biblioteca compartilhada `libmain.so` para `armeabi-v7a`, corrigindo o erro histórico de linker `HQR_Reset<sBody>`. Não aceitar como válido um APK que contém só o teste gráfico.
3. Gerar shaders de **OpenGL ES 2.0** com shaderc no host, revisar formatos de textura e eliminar recursos que só existem em GLES3. Validar numa GPU limitada a ES2.
4. Adaptar armazenamento privado/importação dos arquivos originais do jogo, salvamentos, áudio SoLoud e saída segura, com Java e SDL2 compatíveis com API19.
5. Validar instalação física, gamepad Bluetooth/reconexão, FPS sem acelerar a lógica, carregar/salvar e ausência de bibliotecas ARM64.

**Lição da tentativa anterior:** a branch `port/android-armeabi-v7a-release-20261031` era SDL3/GLES3/API21 e nunca entregou um APK funcional do motor. Seu último erro de build incluía `undefined symbol: HQR_Reset<sBody>`, mas corrigir esse erro isoladamente não a tornaria compatível com Android 4.4/GLES2.

**Isolamento:** essa linha Android é independente da Release ARM64 para dArkOSRE/ArkOS4Clone.
