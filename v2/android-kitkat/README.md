# Primeiro teste GLES2 / Android 4.4

Código-fonte de um **smoke test**, ainda sem APK compilado. Desenha um triângulo verde com shaders GLSL ES 1.00 e contexto OpenGL ES 2.0. Encerra com Escape, START de controle ou após 30 segundos. Sem arquivos comerciais.

## Integração

Usar o template Android da versão SDL2 compatível com API 19, adicionando `gles2_probe.cpp` como biblioteca JNI `main`. Configurar `minSdkVersion 19`, `ANDROID_PLATFORM=android-19`, `abiFilters 'armeabi-v7a'` e declarar `android:glEsVersion=\"0x00020000\"`. Compilar SDL2 junto ao projeto e ajustar o alvo CMake ao template escolhido. O arquivo CMake fornecido pressupõe que o alvo SDL2 já existe.

**Importante:** este teste é separado do FITD, não compila o motor, não contém importador de arquivos, não garante funcionamento do bgfx e não substitui o APK SDL3/GLES3 anterior. Não afirmar compatibilidade Android 4.4 sem compilar e testar em dispositivo real.

## Critérios

- Verificar com aapt: minSdkVersion 19, ABI armeabi-v7a.
- Verificar via logcat: contexto GLES 2.0, shaders compilados e triângulo visível.
- Testar suspensão/retomada, saída via START e controle Bluetooth.
- Só depois migrar o motor e shaders bgfx.
