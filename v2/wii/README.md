# FITD Wii — Teste 03: modelos BODY originais em GX (v0.3)

## Status e escopo

**Prévia técnica 3D, NÃO é o jogo completo ainda.** Os testes anteriores
confirmaram no Nintendo Wii original a renderização dos cenários
CAMERA00.PAK e a descompactação PAK original a ~60 FPS.

Agora carregamos e desenhamos **vértices e polígonos reais do
LISTBODY.PAK**, sobre imagens do CAMERA00.PAK. A animação esquelética,
os personagens na posição/câmera correta do jogo, máscaras de
oclusão, scripts, combate, áudio e save state **ainda não foram
integrados**: não confundir a rotação demonstrativa com animação
original do personagem.

**Android Teste 19 e R36S ARM64 não foram alterados.** Todos os patches
dessas versões permanecem na ancestralidade da branch Wii para
integração posterior; consultar COMPAT_TEST19.md.

## Novidades

- Parser BODY do FITD baseado em createBodyFromPtr() original, com
  vértices XYZ, primitivas de linha, polígonos, pontos e grupos.
- Dados de grupos INFO_ANIM e INFO_OPTIMISE lidos com checagem de
  tamanho, alinhamento e endian-safety no PowerPC.
- Renderização GX de polígonos reais e paleta RGB do ITD_RESS.PAK;
  primitivas texturizadas ainda aparecem com cor sólida.
- Visualização de modelos em 3D sobre o cenário original,
  com rotação e seleção de entradas do LISTBODY.PAK.
- Leitura de cabeçalhos da LISTANIM.PAK; **não executa as
  animações esqueléticas na versão 0.3**.
- Rejeição segura de modelos inválidos, entradas quebradas e tipos
  de primitivas não suportados, mantendo a versão anterior visível.
- Testes automáticos com modelos sintéticos e compilação devkitPPC.

## Estrutura do SD

Extraia o ZIP de Homebrew Channel diretamente na raiz do cartão SD.
**Copie da sua instalação original** estes quatro arquivos
(não estão incluídos):

    SD:/
    └── apps/
        └── fitd-wii/
            ├── boot.dol
            ├── meta.xml
            ├── icon.png
            ├── README.md
            ├── COMPAT_TEST19.md
            └── data/
                ├── CAMERA00.PAK
                ├── ITD_RESS.PAK
                ├── LISTBODY.PAK
                └── LISTANIM.PAK

Abra o Homebrew Channel, selecione FITD Wii Teste 03 e pressione A.
Se os quatro arquivos existirem e os dados forem suportados, verá
um modelo original do jogo sobre o cenário da câmera.

### Controles

| Botões | Função |
|---|---|
| Wii A / GameCube A | Iniciar GX |
| Wii + e - / GC R e L | Selecionar imagem da câmera |
| Wii D-pad ESQUERDA / DIREITA, na cena original | Trocar índice do BODY |
| Wii 2 / GC X | Mostrar ou ocultar o modelo BODY |
| Wii B / GC B | Girar o modelo |
| Wii 1 / GC Y | Alternar imagem original e demo 3D do Teste 01 |
| Wii HOME / GC START | Sair |

A seleção de índices BODY pode escolher um objeto de cenário ou um
modelo que ainda não renderizamos corretamente. Teste alguns índices
com o D-pad até encontrar um objeto/personagem reconhecível.

## Log

Após sair, envie:
SD:/apps/fitd-wii/wii-diagnostic.txt
ou SD:/fitd-wii-diagnostic.txt.

Mensagens novas:
- FITD-WII-BODY OK index=... vertices=... groups=... primitives=...
- FITD-WII-BODY REJECT index=... parser=...
- FITD-WII-ANIM index=0 status=METADATA_OK ...
- FITD-WII-MODEL status=visible:... index=... fps=...

Com elas identificaremos formatos reais encontrados e ajustaremos a
decodificação. Nenhuma alteração é feita nos arquivos .PAK do SD.

## Próximos marcos

1. Validar modelos reais exibidos no console; ajustar coordenadas e
   ordenação de polígonos onde necessário.
2. Interpretar os grupos de animação originais e aplicar poses corretas.
3. Implementar as máscaras/oclusão da câmera da sala atual.
4. Conectar o motor FITD C++ completo; integrar TODAS as correções de IA,
   monstros presos, scripts, colisão, áudio, five save slots e controle
   do Android Teste 19 / ARM64.
5. Medir o desempenho com o jogo real (60 FPS da demo GX não garante
   60 FPS do motor completo).

## Créditos e legalidade

Base do motor: Free in the Dark (yaz0r/FITD), GPL-2.0.
Port e correções: dehalbuq/FITD-R36S e contribuições anteriores;
referência comparativa ao Re-Haunted.
Leitura PAK original: Cyril VOILA / Mark Adler.
Os créditos e obrigações de licenciamento deverão ser preservados
na integração completa do motor.

Não contém arquivos comerciais do Alone in the Dark.
Use arquivos de sua cópia legitimamente adquirida.
Nintendo não é afiliada ao projeto.


## Teste 03.1 — correção do diagnóstico no cartão SD

Esta revisão não modifica a renderização dos modelos BODY.
Ela melhora a gravação do log quando o Homebrew fecha:

- A primeira tela exibe DIAGNOSTIC LOG: e o caminho usado; a criação
  do arquivo é testada antes de entrar no GX.
- Caminho prioritário: SD:/fitd-wii-test03-diagnostic.txt, na RAIZ do SD.
- Em caso de erro na raiz, o programa tenta:
  SD:/apps/fitd-wii/wii-diagnostic.txt.
- A cada 300 quadros (aproximadamente 5 segundos), o log é fechado e
  reaberto, para atualizar o arquivo no FAT antes de qualquer saída abrupta.
- Ao sair normalmente, o aplicativo também tenta copiar o log da raiz
  para a pasta apps/fitd-wii, mantendo a versão original caso falhe.
- Se SD permitir ler os PAKs mas não permitir gravar, a tela inicial
  mostra WARNING: DIAGNOSTIC NOT SAVED (errno=...).
  Verifique proteção de gravação e espaço livre.

Para testar, abra o app, verifique a mensagem SD write test: PASS,
pressione A, alterne alguns modelos, saia com HOME e envie o arquivo
encontrado em um dos caminhos acima. A prévia BODY permanece idêntica
à do Teste 03.
