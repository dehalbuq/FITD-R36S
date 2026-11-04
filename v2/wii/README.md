# FITD Wii Teste 05.1 — correção do índice PAK de ETAGE00

**Esta revisão corrige um erro confirmado no Teste 05.**
ETAGE00.PAK possui duas entradas físicas:
- índice 0: registros de salas;
- índice 1: registros de câmeras.

O leitor Wii contava uma entrada a menos. Assim, o Teste 05
registrou FITD-WII-STAGE floor=0 archive_error=BAD_ARGUMENT_OR_INDEX,
apesar de o arquivo estar presente.

O Teste 05.1 aplica a contagem física correta: first_data_offset / 4 - 1.
A mesma correção revela a última imagem de CAMERA00.PAK e os últimos
índices de LISTBODY.PAK e LISTANIM.PAK. Não modifica nenhum arquivo
original do usuário.

O log principal desta versão está em:

    SD:/fitd-wii-test05-1-diagnostic.txt

Há uma cópia adicional, se possível, em:

    SD:/apps/fitd-wii/wii-diagnostic.txt

Para testar: mantenha os PAKs originais, sobretudo ETAGE00.PAK,
abra no Homebrew Channel, pressione A e ative a visualização de
máscaras com B+1. Experimente B + ou B - para alternar salas. Saia
com HOME e envie o novo diagnóstico. Procure:

    FITD-WII-STAGE05-PACK floor=0 directory_status=0 entries=2
    FITD-WII-STAGE05-ENTRIES room=0:... camera=1:...
    FITD-WII-STAGE OK floor=0 rooms=... cameras=...
    FITD-WII-MASK05 ... ready=...

A leitura real das salas e a sobreposição ainda precisam ser
confirmadas no Wii. A compilação pode revelar erros posteriores do
formato interno; os novos logs detalham a etapa. Isso ainda é um
visualizador técnico, não o motor completo com gameplay.

---

# FITD Wii — Teste 05: ETAGE, salas, câmeras e máscaras originais GX

Esta versão é um **teste técnico nativo PowerPC para Nintendo Wii**.
Ela integra as estruturas de cenário do Alone in the Dark original ao
visualizador 3D e de animações. **Ainda NÃO é o motor completo jogável.**

### Preservação dos testes aprovados

- Wii Teste 03.1: diagnóstico SD estável e modelos BODY, preservado.
- Wii Teste 04: animações LISTANIM originais com amostragem interpolada,
  compilado e validado no Wii, preservado.
- Android Teste 19 e R36S ARM64: não modificados. Os patches de IA,
  colisão, save, áudio e demais correções permanecem no repositório.
  Consulte COMPAT_TEST19.md: as correções de gameplay ainda **não foram
  conectadas ao executável Wii**.

## O que implementamos no Teste 05

- Leitor real de ETAGE00.PAK, entrada 0 (salas) e entrada 1 (câmeras).
- Tabelas de salas, posição relativa, índices das câmeras associadas,
  caixas de colisão do cenário e zonas de scripts.
- Dados reais das câmeras: ângulos originais A/B/G, posição, perspectiva
  e referências de salas visíveis.
- Decodificação dos polígonos de máscaras do AITD1 com leitura
  PowerPC little-endian, limites contra memória inválida e rasterização
  de 320x200 pixels.
- Conversão das áreas cobertas por máscaras em textura GX RGB5A3
  com pixels transparentes e cores tiradas do CAMERA00.PAK original;
  redesenha a parte frontal do cenário *sobre* o modelo BODY.
- Alternância manual do efeito de oclusão e navegação entre salas.
  Quando o usuário troca a câmera, a máscara correspondente é
  reconstruída sem alterar os arquivos originais.
- A animação real do BODY e o log persistente continuam funcionando.
- Testes automáticos com dados sintéticos, sem redistribuir assets
  comerciais (PAK, BODY, ANIM, ETAGE, máscaras).

**Limitação fundamental:** atualmente a prévia sobrepõe a união das
máscaras do cômodo em modo de inspeção. Ainda não faz a escolha de máscara
por profundidade/posição real do ator, não usa a projeção cinematográfica
definitiva da câmera, não percorre scripts de ETAGE ou collision response,
e não carrega a lógica completa do jogo. Ative o efeito apenas para
verificar a decodificação: pode cobrir indevidamente a personagem.
As caixas de colisão são lidas e testadas, **não estão aplicadas** ao
movimento. A estrutura pronta permite a integração dessas regras depois.

## Instalação no SD

Extraia o ZIP na raiz do cartão SD, substituindo somente os arquivos
do aplicativo. Mantenha na pasta data/ os arquivos da sua cópia
original do jogo. Para este teste são usados:

    SD:/
    └── apps/
        └── fitd-wii/
            ├── boot.dol
            ├── meta.xml
            ├── icon.png
            ├── README.md
            ├── COMPAT_TEST19.md
            └── data/
                ├── ETAGE00.PAK
                ├── CAMERA00.PAK
                ├── ITD_RESS.PAK
                ├── LISTBODY.PAK
                └── LISTANIM.PAK

O pacote NÃO contém arquivos comerciais do jogo.
Execute pelo Homebrew Channel e pressione A para iniciar.

## Controles

| Wii Remote | Ação |
|---|---|
| D-pad esquerda/direita | Trocar BODY |
| D-pad cima/baixo | Trocar animação LISTANIM |
| A | Pausar / continuar animação |
| B | Girar modelo |
| + / − | Avançar / voltar câmera de CAMERA00.PAK |
| 1 | Alternar cenário original / demonstração GX |
| 2 | Mostrar / ocultar modelo |
| **Segurar B e apertar 1** | **Ativar / desativar sobreposição das máscaras originais** |
| **Segurar B e apertar + ou −** | **Avançar / voltar entre salas de ETAGE00.PAK** |
| HOME | Sair e gravar diagnóstico |

GameCube: B+Y alterna máscara, B+R/L troca sala, R/L sozinho
troca câmera, direcional muda BODY/ANIM, A pausa, START sai.

### Como confirmar a etapa

1. Inicie e observe se os cenários do Teste 02 e o personagem animado
   continuam aparecendo.
2. Segure B e aperte 1; observe se alguma parte do cenário original
   passa a aparecer *por cima* do modelo. Isso valida a máscara mas
   NÃO deve ser confundido com o efeito de oclusão final do jogo.
3. Troque as câmeras com +/- e observe se as máscaras se atualizam.
4. Teste B + ou B − para navegar entre os cômodos do ETAGE00.
   Atenção: uma sala pode não ter câmera associada ao CAMERA00;
   a prévia pode manter a câmera anterior nesses casos.
5. Saia com HOME. O log contém os metadados de sala, câmera e máscara.

### Diagnóstico

Principal:

    SD:/fitd-wii-test05-diagnostic.txt

Cópia alternativa:

    SD:/apps/fitd-wii/wii-diagnostic.txt

Procure as linhas:
- FITD-WII-STAGE OK floor=0 rooms=... cameras=...
- FITD-WII-ROOM05 index=... cams=... hard=... sce=...
- FITD-WII-CAMERA05 index=... angle=... position=... focal=...
- FITD-WII-MASK05 OK camera=... room=... masks=... polygons=... covered_pixels=...
- FITD-WII-MASK05 toggled=1 available=1 pixels=...
- FITD-WII-ANIM04 status=DECODED compatible=1 pose=1 ...
- GX frames=... fps=...
- finish ... endianness_check=PASS

Se alguma máscara mostrar status negativo ou pixels=0, anexe
o arquivo: pode ser variante do formato ou ausência de máscaras
para aquela câmera, sem necessariamente indicar falha na renderização.

## O que ainda falta para o jogo completo

1. Posicionamento real do ator e projeção exata dos modelos usando
   ângulos, posição e perspectiva de cada câmera ETAGE.
2. Escolha automática da máscara por zona/profundidade e
   oclusão correta de personagens e objetos do cenário.
3. Execução real dos scripts, máquinas de estado e interações;
   controle de personagem, inventário e combate.
4. Colisões de salas e objetos com todas as correções das versões
   Android Teste 19 e R36S ARM64.
5. Áudio, efeitos, save/load, transições de cena e testes funcionais
   de gameplay. 60 FPS deste visualizador não garante 60 FPS do jogo.

## Licenças, créditos e responsabilidade

Baseado em Free in the Dark (yaz0r/FITD, código GPL-2.0).
Correções e contribuições: dehalbuq/FITD-R36S, com referências
comparativas ao Re-Haunted e demais colaboradores. Parser de PAK:
Cyril VOILA e Mark Adler. ETAGE e máscara: port com verificação
de bounds a partir de floor.cpp, room.cpp e main.cpp do upstream.

Nenhum jogo comercial, sprites, personagens, modelos, música, sons ou
texto proprietário acompanha o executável. É necessária a cópia
legitimamente adquirida de Alone in the Dark. Nintendo não participa
nem aprova o projeto.
