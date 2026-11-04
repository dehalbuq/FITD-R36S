# Correções a preservar: motor FITD Android Teste 19 + ARM64 R36S

Esta branch é descendente direta de:
- port/wii-devkitppc-gx-prototype
- fix/android-armv7-test19-arm64-rehaunted-ai
- fix/android-armv7-test18-engine-60hz

As correções abaixo permanecem NO REPOSITÓRIO e são base obrigatória para o
futuro motor PowerPC. **Não estão executando no Wii Teste 02**, que ainda é um
leitor de archives e visualizador GX. Não alterar Android Teste 19 nem ARM64.

## Checklist de regressão (integração do futuro motor Wii)

- [ ] Motor e animações com agendamento nativo 60Hz, sem reproduzir bug de 25 Hz
- [ ] Correções de IA do Re-Haunted (velocidade follow/patrol, animação de NPC)
- [ ] Recuperação limitada dos monstros das janelas world21 / world57
- [ ] Guardas de colisão com objetos de efeito AF_SPECIAL e atores invisíveis
- [ ] Correção de NPC preso em objetos e parede física, sem tornar paredes atravessáveis
- [ ] Correção HIT_OBJ para ataques/projéteis do Re-Haunted
- [ ] Verificações de scripts e inventário da adega
- [ ] Recuperação da parede do subsolo, sem deslocamento fora de área
- [ ] Proteções de save state, cinco slots e reinicialização
- [ ] Áudio OPL / PCM e efeitos sem repetição anormal
- [ ] Legibilidade da fonte e máscaras visuais
- [ ] Controles, menu de saída e estabilidade após relançamento
- [ ] 32/64bit, alinhamento e **big-endian PowerPC** em todos os arquivos
- [ ] Preservar avisos legais e créditos de FITD / Re-Haunted / portadores

## Fontes rastreadas (não importar cegamente para Wii)

Android Test19:
- v2/android-kitkat/engine-runtime/patch_android_enemy_arm64_rehaunted.py
- v2/android-kitkat/engine-runtime/patch_cellar_revolver_android.py
- v2/patch_hit_object_rehaunted.py
- v2/patch_graphics_regression.py

ARM64 R36S:
- v2/patch_cellar_wall_recovery.py
- v2/patch_cellar_barrel_stuck.py
- v2/patch_orb_enemy_effects.py
- v2/patch_worm_camera_orb_audio.py
- v2/patch_rehaunted_mask_reptile.py

Não copiar binários ARM, SDL2 Android, GLES2 ou chamadas do driver Android
diretamente. A camada de gameplay deve ser portada a partir dos mesmos
patches do código-fonte, passando por revisão dos casts e acesso a PAK/HQR.

## Status de Wii Teste 02

- ✅ Compilação GX já validada no Wii na versão anterior (Teste 01).
- ✅ Implementação de leitor FAT + PAK endian-safe e código do decoder imploded.
- ✅ Prévia de CAMERA00 e paleta ITD_RESS implementada em GX (a validar).
- ⏳ Motor FITD completo e correções de gameplay **ainda não estão linkados**.
- ⏳ Fundo e parser com arquivos originais **dependem de teste no Wii real**.

Este documento evita confundir port de código com correções já validadas em
hardware, e impede que alguma correção seja esquecida ao integrar o motor.
