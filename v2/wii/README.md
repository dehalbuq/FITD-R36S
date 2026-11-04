# FITD Wii — Teste 02: arquivos reais e cenário GX (v0.2)

## Situação

**Prévia gráfica experimental, NÃO é o jogo jogável.** Teste 01 já funcionou
no Wii original a 59,95 FPS na cena experimental. O Teste 02 começa a ler
os arquivos originais, suas paletas e fundos indexados.

**Android Teste 19 e R36S ARM64 permanecem inalterados.** Esta branch Wii é
descendente do Android Teste 19 e preserva todos os patches originais.
Veja COMPAT_TEST19.md: essas correções ainda precisarão ser adaptadas ao
PowerPC quando integrarmos o motor.

### Novidades

- Parser real de PAK: índices LE32, validação de limites e arquivos truncados.
- Descompressão modo 1 (implode), adaptada do código Cyril VOILA / Mark Adler.
- Leitura de dados modo 0 sem compressão.
- **Modo 4 (DEFLATE) ainda não suportado.**
- Carregamento de imagem CAMERA00.PAK índice 0 (320x200, 8 bits indexados).
- Conversão ao formato Wii GX RGB565 em blocos 4x4.
- Paleta original ITD_RESS.PAK entrada 3; caso não esteja disponível, tons
  de cinza provisórios (as cores originais ainda não são garantidas).
- Teclas para mudar de câmera, alternar a prévia com a demo 3D, gerar log.
- Testes sintéticos do PAK, sem arquivos comerciais.

### Instalação pelo Homebrew Channel

Copie o conteúdo do ZIP para a raiz do cartão SD:

    SD:/apps/fitd-wii/boot.dol
    SD:/apps/fitd-wii/meta.xml
    SD:/apps/fitd-wii/icon.png
    SD:/apps/fitd-wii/data/CAMERA00.PAK
    SD:/apps/fitd-wii/data/ITD_RESS.PAK

**CAMERA00.PAK e ITD_RESS.PAK devem ser copiados da sua própria instalação
original de Alone in the Dark.** NÃO estão no ZIP. Sem eles, a demo de
cubos do Teste 01 continua funcionando e o log explica a falha.

Abra Homebrew Channel > FITD Wii Teste 02 > A.

### Controles

- Wii A / GameCube A: entrar na prévia GX
- Wii + e - / GameCube R e L: trocar imagem de CAMERA00.PAK
- Wii 1 / GameCube Y: alternar imagem original e demo 3D
- D-pad (demo): mover personagem experimental
- Wii B / GameCube B (demo): girar personagem experimental
- Wii HOME / GameCube START: sair

Envie o arquivo de log após testar:
SD:/apps/fitd-wii/wii-diagnostic.txt (alternativo: SD:/fitd-wii-diagnostic.txt).

### Limitações antes da versão jogável

1. Integrar motor FITD C++ (salas, câmeras, máscaras e BODY/ANIM).
2. Desenhar geometria real, UI, paleta e máscaras no backend GX.
3. Portar IA, colisões, scripts, efeitos, áudio e cinco saves das versões
   Android Teste 19 e R36S ARM64, sem perder as correções existentes.
4. Resolver casts de ponteiro e outros acessos binários little-endian no Wii.
5. Testar gameplay e desempenho em hardware original.

Este pacote NÃO é o jogo completo. O Teste 01 validou somente a demo GX.

### Créditos e legalidade

Baseada em yaz0r/FITD (https://github.com/yaz0r/FITD), com correções
de dehalbuq/FITD-R36S e comparação ao Re-Haunted. Descompressor:
Cyril VOILA (2004), parcialmente Mark Adler (1992). Código do motor
GPL-2.0 e respectivos avisos de licença preservados.

Nenhum asset comercial do jogo é distribuído. Nintendo não é afiliada.
