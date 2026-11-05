# FITD Wii Teste 07.1 — galeria paginada, BODYs e movimentos reais

**Nova rodada integrada; preserva a 06.3.1 validada visualmente para máscaras.**
BODY 15 mostrou um rifle na mão mesmo na pose neutra; pode ser a
geometria desta variante, e não uma animação. Sem mexer nas texturas,
este teste permite comparar outros modelos originais sem alterar assets.

## ÚNICA rodada recomendada

1. Extraia o ZIP na raiz do SD, preservando os arquivos originais em
   `SD:/apps/fitd-wii/data/`. No Homebrew Channel, pressione **A**.
2. A inicialização analisa 32 animações compatíveis com BODY 15,
   5 câmeras, máscaras, 8 trajetórias de colisão e até 64 modelos
   iniciais de LISTBODY, filtrando os articulados com 18 ou 20 grupos.
3. **B + 1** inicia uma galeria de 8 animações por página. Agora cada
   animação fica na tela **5,2 segundos**. Ao terminar, **B + 1
   novamente mostra a página seguinte**, até percorrer todos os clipes.
   O número grande no canto é a entrada do LISTANIM.PAK. Não precisa
   testar a mesma página várias vezes.
4. **B + direcional CIMA** muda para o próximo BODY compatível;
   **B + direcional BAIXO** retorna ao anterior. Observe qual BODY
   não possui o rifle e informe o número do log `SELECT body=`.
   Ao selecionar novo modelo, sua pose de repouso é remontada e
   a lista de clipes compatíveis é recalculada.
5. Para um BODY com 18 grupos, **B + direcional ESQUERDA** testa
   a animação original 287 e **B + direcional DIREITA** testa 288.
   Se não for compatível, o teste registra a rejeição e conserva a
   pose segura. Estas animações não são atribuídas à caminhada
   automaticamente.
6. Você também pode caminhar, mudar de câmera, observar máscaras
   ou usar **B + A** para ir ao próximo candidato manualmente.
   **HOME** encerra e grava o relatório consolidado.

No controle GameCube, Y corresponde ao botão 1, X ao botão 2
e Start encerra.

## Como interpretar

- `FITD-WII-BODYCAT07-1 candidate=`: BODYs alternativos articulados;
- `FITD-WII-BODYCAT07-1 SELECT body=`: modelo apresentado na tela;
- `FITD-WII-BODYCAT07-1 TEST clip=287/288`: teste de compatibilidade;
- `FITD-WII-GALLERY07 start page=2/4`: avanço para uma página nova;
- `FITD-WII-SUITE07 anim=...` agora inclui `move_forward` e
  `move_x`: deslocamentos originais por ciclo de animação.

Não há garantia de que todo BODY com 18 grupos seja o personagem
desarmado ou que 287/288 correspondam à caminhada daquele modelo.
Isso precisa de confirmação visual e da integração futura de LIFE 549/550.
Também não muda os arquivos originais.

## Log a enviar

`SD:/fitd-wii-test07-1-body-gallery-diagnostic.txt`

Cópia: `SD:/apps/fitd-wii/wii-diagnostic.txt`

Créditos: yaz0r/Free in the Dark, dehalbuq/FITD-R36S e colaboradores.
Motor/protótipo GPL. Sem ativos comerciais, não afiliado à Nintendo.

---

# FITD Wii Teste 07 — diagnóstico integrado (um teste, vários subsistemas)

**A versão 06.3.1 já foi validada visualmente para fundo/máscaras e é preservada.**
Esta branch de teste adiciona apenas uma bateria de diagnósticos e uma
galeria automática das animações. Ainda não é o jogo completo.

## Como testar uma só vez no Wii

1. Copie o ZIP para a raiz do SD, mantendo os arquivos originais do jogo na pasta
   `SD:/apps/fitd-wii/data/`.
2. Abra no Homebrew Channel e pressione **A**. Na primeira entrada ele testa
   automaticamente **todas as animações estruturalmente compatíveis** encontradas,
   todas as câmeras do ETAGE00, a leitura das máscaras, a projeção e simulações
   de deslocamento e colisão, **sem alterar a posição do personagem em jogo**.
   Os resultados são registrados em `FITD-WII-SUITE07`.
3. Com o personagem na cena, segure **B e aperte 1** (controle GameCube: **B + Y**).
   A galeria exibirá automaticamente **até 8 animações diferentes**, selecionadas
   por quantidade de movimento geométrico. Cada animação percorre um ciclo
   completo, de maneira acelerada, em cerca de **2,2 segundos**. A galeria leva
   aproximadamente **18 segundos** no total, caso existam oito candidatos.
4. Um número grande no alto da tela identifica o índice exato do
   `LISTANIM.PAK` em exibição, e a barra mostra o andamento da galeria.
   Observe quais índices parecem **caminhada**, **repouso**, **dano** etc.
   **O ranking não identifica caminhada sozinho**; clipes de ação podem
   ter alto movimento e acabar entre os oito mostrados.
5. Depois da galeria o ator retorna automaticamente à pose de repouso.
   Você pode andar, mudar de câmera e verificar a oclusão normalmente.
   Pressionar **B + 1** novamente cancela uma galeria em andamento.
   **B + A** continua disponível para percorrer as 32 animações manualmente,
   quando for necessário investigar além dos oito candidatos.
6. Encerre com **HOME** (GameCube: Start). **Um único log é suficiente**.

## O que é verificado automaticamente

- Integridade estrutural da pose de repouso e de cada animação candidata;
  quatro momentos por clipe, coordenadas finitas e dimensões plausíveis
- Decodificação das câmeras do ETAGE00; máscaras e quantidade de polígonos
- Projeção do personagem em cada câmera
- Oito trajetórias de movimento simuladas, com passos aceitos/bloqueados e
  seleção de câmera, sem modificar o ator real
- Seleção das animações com movimento geométrico mais acentuado
- Renderização habitual a ~60 FPS, com logs de posição, esqueleto,
  máscara ativa, câmera automática e colisões reais

O teste automático **não prova** que a animação representa caminhada,
que as máscaras estão visualmente perfeitas ou que há compatibilidade
com todos os cenários; a observação no Wii continua sendo necessária
nesses pontos específicos.

## Diagnóstico

`SD:/fitd-wii-test07-batch-diagnostic.txt`

Cópia: `SD:/apps/fitd-wii/wii-diagnostic.txt`

Busque por `FITD-WII-SUITE07 SUMMARY` e `FITD-WII-SUITE07 FINAL`
para o resumo, `FITD-WII-SUITE07 shortlist=` para candidatos
classificados por movimento e `FITD-WII-GALLERY07` para o ensaio visual.
O índice exibido na tela corresponde ao mesmo índice no log.

O pacote não inclui dados comerciais. Projeto GPL-2.0 ou posterior,
créditos a Free in the Dark/yaz0r e contribuidores de FITD-R36S.
Não afiliado à Nintendo.

---

# FITD Wii Teste 06.3.1 — máscaras com suavização uniforme, sem borda preta

Esta revisão corrige o último defeito visual relatado no Wii: na 06.3,
a borda preta havia desaparecido, porém a máscara era mais nítida que
a imagem de fundo. **A pose de repouso e o scanner de 32 candidatos
da 06.3 foram preservados, sem alterações na lógica do personagem.**

## Alterações

- A textura de máscara usa o formato GX **RGBA8**, com cores RGB da
  mesma paleta original do cenário em cada pixel, inclusive nos pixels
  transparentes. A opacidade vem exclusivamente da seleção dos polígonos.
- A textura do cenário e a máscara usam **GX_LINEAR**.
- O contorno de alpha continua mesclado sobre o cenário; como as cores
  transparentes também correspondem ao cenário, a interpolação não
  produz um halo preto.
- Memória de textura da máscara: 256 KB (320 x 200 x 4 bytes), adequada
  ao Wii. Novos testes verificam o empacotamento em tiles AR/GB.

O teste deve determinar se as máscaras agora têm suavidade e cores
semelhantes às do fundo e se a borda preta continua ausente.

## Instalação e teste

Extraia o ZIP na raiz do cartão SD e mantenha os PAKs originais em
`SD:/apps/fitd-wii/data/`. Use o Homebrew Channel e pressione A.
Movimente o personagem atrás de objetos em diferentes câmeras, comparando
a aparência com a versão 06.3.

- Tecla **1**: alterna máscara do modo ator
- **B + A**: visualiza o próximo candidato compatível (mesmo que na 06.3)
- **2**: alterna modo ator/inspetor
- **HOME**: fecha, salva o diagnóstico

Logs:
`SD:/fitd-wii-test06-3-1-diagnostic.txt` e
`SD:/apps/fitd-wii/wii-diagnostic.txt`.

Este é um protótipo, não o jogo completo. Não inclui arquivos comerciais.
Versões de referência: 06.1 (articulação) e 06.3 (pose neutra confirmada).

---

# FITD Wii Teste 06.3 — pose articulada segura e bordas de máscaras

**Experimento para hardware Wii físico. Não é o motor jogável completo.**
O Teste 06.1 continua preservado na branch `fix/wii-test06-1-skeletal-hierarchy`.

## Problemas corrigidos com base nos logs de 06.2

- BODY masculino 15: **20 grupos**. As animações 287 e 288 tinham **18 grupos**, portanto nunca devem ser atribuídas automaticamente ao personagem.
- Ao invés de exibir os vértices locais do BODY sem montagem (pose baixa/colapsada), o modo ator reconstrói o **repouso articulado** a partir das transformações iniciais, hierarquia, grupo-base e pivôs originais do BODY. Se essa montagem falhar, o ator não é desenhado de maneira inválida.
- A seleção do estado parado/andando continua sendo calculada pelo deslocamento real, mas nenhuma animação desconhecida é ligada automaticamente. Ela depende de validação visual no Wii.
- O carregamento faz uma busca em `LISTANIM.PAK` pela quantidade de grupos e formato adequados. Apenas candidatos com parsing válido entram no modo de inspeção.
- As máscaras originais agora usam **GX_NEAR** para evitar interpolar pixels transparentes RGB preto, além de teste de alpha para descartar transparência nula. O resultado visual ainda precisa ser verificado em hardware.

## Instalação

Extraia o ZIP na raiz do cartão SD. Preserve os arquivos originais em
`SD:/apps/fitd-wii/data/`: `CAMERA00.PAK`, `ETAGE00.PAK`,
`ITD_RESS.PAK`, `LISTBODY.PAK` e `LISTANIM.PAK`.

## Teste de hardware

1. Abra no Homebrew Channel e aperte A. Observe se a postura do BODY 15 está naturalmente articulada e não mais colapsada. A pose padrão é **REST**, sem animação.
2. Ande com o direcional e solte. O estado de movimento deve mudar entre WALK e IDLE, **mas ainda sem usar um clip de caminhada** até identificarmos o correto.
3. Passe atrás dos elementos mascarados nas diferentes câmeras. Verifique se a borda escura desapareceu. A tecla **1** liga/desliga a oclusão.
4. Opcionalmente, pressione **B + A** no modo ator para selecionar a próxima animação estruturalmente compatível. Repita para percorrer os candidatos; depois do último, volta para REST. Observe se alguma animação corresponde a repouso/caminhada; anote a ordem dos botões, pois o log fornece os índices correspondentes.
5. Botão **2** alterna com o inspetor antigo, onde ANIM 301 permanece para reproduzir o Teste 06.1. **HOME** encerra o aplicativo.

O botão A sozinho ainda pausa/retoma a animação e B sozinho gira o personagem.
O controle GameCube usa X para 2, Y para 1, Start para HOME.

## Arquivos de log

- Principal: `SD:/fitd-wii-test06-3-diagnostic.txt`
- Espelho: `SD:/apps/fitd-wii/wii-diagnostic.txt`

Procure por `FITD-WII-REST06-3` (`ready=1`),
`FITD-WII-SCAN06-3 candidate=...` (índices compatíveis),
`FITD-WII-SCAN06-3 inspect=...` (índice selecionado),
`FITD-WII-LOCOMOTION06-3` (estados de movimento) e
`FITD-WII-BONES06-1` (`pose=1` no modo ator).

O arquivo de log sozinho não prova que uma pose é anatomicamente
correta nem que uma máscara está visualmente perfeita.

Créditos: yaz0r/Free in the Dark (GPL-2.0) e dehalbuq/FITD-R36S,
além de colaboradores. Nenhum arquivo comercial é distribuído.
Nintendo não é afiliada ao projeto.

---

# FITD Wii Teste 06.2 - movimento e animações separados

Este teste continua exatamente a versão 06.1: esqueleto original
com rotação hierárquica corrigida, câmera cinematográfica automática,
máscaras de oclusão e colisões. O usuário confirmou o personagem
articulado corretamente na 06.1, mas ele repetia uma animação de dano
porque o teste anterior reproduzia LISTANIM index 301 permanentemente.

## O que mudou no Teste 06.2

- O BODY masculino 15 continua, sem modificar seu parser nem os ossos.
- A animação 301 foi removida do modo ator normal. Ela fica disponível
  apenas no modo antigo de inspeção, para reproduzir o Teste 06.1.
- Carregamento dos **candidatos** LISTANIM 287 (parado/idle) e
  LISTANIM 288 (andar/walk), associados em referências da comunidade
  aos movimentos de Edward em determinadas transições.
- Compatibilidade verificada no momento do carregamento: exatamente
  20 grupos para o BODY 15, mesma variante de formato e dados válidos.
  Se um arquivo for incompatível, o ator será exibido sem animação
  em vez de executar a animação de dano ou distorcer os ossos.
- Estado IDLE enquanto o personagem não se desloca.
- Estado WALK enquanto o deslocamento real é aceito pelas colisões.
- A animação reinicia somente quando ocorre uma mudança entre esses
  estados; não é reiniciada a cada quadro de movimento.
- As animações continuam a 50 ticks por segundo, sem depender
  dos aproximadamente 60 FPS da GPU.

**Importante:** 287 e 288 são candidatos fundamentados em referências
dos scripts do jogo, mas ainda precisam ser validados VISUALMENTE no
seu BODY 15 original. O programa não conclui que um movimento é
correto apenas porque a estrutura binária é compatível. Se o seu
asset usa outros índices, os novos logs indicarão claramente.

## Instalação

Extraia o ZIP na raiz do SD, sem apagar os arquivos originais do jogo
na pasta SD:/apps/fitd-wii/data/:

CAMERA00.PAK, ETAGE00.PAK, ITD_RESS.PAK, LISTBODY.PAK, LISTANIM.PAK.

## Teste no Wii

1. Inicie pelo Homebrew Channel, aperte A.
2. OBSERVE o personagem parado: ele não deve mais repetir o movimento
   de dano do Teste 06.1. Veja se está com os braços relaxados.
3. Caminhe com o direcional. A animação de caminhada deve aparecer
   somente quando o personagem realmente mudar de posição.
4. Solte o direcional. O personagem deve voltar à pose de repouso
   sem repetir o movimento de dano ou caminhar parado.
5. Ande contra uma parede: quando a colisão bloquear o personagem,
   a animação deverá retornar ao estado de repouso.
6. Alternativamente aperte 2 para abrir o modo de inspeção antigo
   (onde o clip 301 de dano continua acessível apenas para diagnóstico).
7. Saia com HOME, envie o arquivo de log.

## Diagnóstico

Principal: SD:/fitd-wii-test06-2-diagnostic.txt

Exemplos de novos registros:

FITD-WII-LOCOMOTION06-2 idle=287 loaded=1 compatible=1 ...
  walk=288 loaded=1 compatible=1 ...
FITD-WII-LOCOMOTION06-2 transition=WALK index=288 compatible=1 ...
FITD-WII-LOCOMOTION06-2 transition=IDLE index=287 compatible=1 ...
FITD-WII-LOCOMOTION06-2 state=IDLE anim=287 ...
FITD-WII-BONES06-1 actor_mode=1 pose=1 body=15 anim=287 ...

O resultado precisa ser confirmado no Wii físico: a compatibilidade
estrutural não garante que a animação escolhida pareça a marcha
correta, especialmente quando BODYs específicos trocam de roupa,
item ou pose nos scripts originais.

## Estado do projeto

A versão 06.1 aprovada continua intacta na branch
fix/wii-test06-1-skeletal-hierarchy.
O Teste 06.2 é uma nova branch experimental. Ainda faltam as
máquinas de estado do jogo completo, a execução de LIFE 549/550
(ANIM_MOVE), inventário, combate, áudio, save/load, transições de
sala e demais subsistemas. Sem distribuição de assets comerciais.

Base legal: Free in the Dark (yaz0r/FITD, GPL-2.0),
dehalbuq/FITD-R36S e demais autores e licenças aplicáveis.
Use dados da sua cópia original do jogo. Nintendo não é afiliada.

---

# FITD Wii Teste 06.1 - correção da animação 3D torta

Esta versão é uma correção dedicada ao problema relatado após testar
o Wii Teste 06 no hardware real: o personagem masculino aparecia
com os membros / animações deformados ("todos tortos").

## O que foi corrigido

- Estados de LISTANIM.PAK agora são aplicados aos grupos físicos do
  LISTBODY.PAK pelo seu índice real, NÃO pelo índice de ordenação
  de desenho. Essa diferença associava movimentos de um braço/perna
  a outro grupo do modelo.
- Rotações do grupo-pai agora alcançam seus grupos filhos conforme
  as relações de parentesco do BODY, antes da composição dos pivôs.
- Eixos de rotação corrigidos de Y-Z-X para Y-X-Z, como no FITD original.
- No modo ator, a rotação global do primeiro grupo do modelo não é
  duplicada: o ângulo do personagem já é aplicado pela câmera de jogo.
- Novos testes automáticos de deformação detectam associação errada
  entre grupo e animação, hierarquia de grupo-pai e grupo-filho,
  ordem dos três eixos e a duplicação do giro do ator.
- Diagnóstico de largura/altura/profundidade da pose para identificar
  alongamentos extremos em hardware.

## O que não foi alterado

O tamanho do personagem continua mudando quando ele se aproxima ou
se afasta da câmera: este é o comportamento correto de perspectiva
dos jogos clássicos Alone in the Dark. Não implementamos escala fixa
artificial nem removemos a projeção cinematográfica.

A troca automática de câmera, as máscaras condicionais de oclusão,
as zonas de colisão e o modo de inspeção do Teste 05.3 foram mantidos.

## Instalação e testes no Wii

Extraia o ZIP na raiz do cartão SD e preserve os arquivos originais
do jogo na pasta SD:/apps/fitd-wii/data/:
CAMERA00.PAK, ETAGE00.PAK, LISTBODY.PAK, LISTANIM.PAK, ITD_RESS.PAK.

Abra Homebrew Channel > FITD Wii Teste 06.1. Pressione A para iniciar.
O teste carrega BODY 15 e ANIM 301 como no Teste 06.

- Direcional: mover personagem em coordenadas da sala.
- B: girar personagem.
- 1: ligar/desligar oclusão automática.
- 2: alternar modo ator / visualizador de modelos.
- A: pausar/continuar animação depois da inicialização.
- + / -: troca manual de câmera (modo diagnóstico).
- HOME: encerrar e salvar diagnóstico.

Primeiro observe o personagem parado, se tronco/pernas/braços continuam
nas posições coerentes. Depois caminhe em direções diferentes, observe
se a animação permanece sem torções e confirme que o tamanho muda
suavemente conforme a distância da câmera, sem saltos artificiais.
Se a articulação ainda parecer errada, informe o que está deformado
(pernas, braços, tronco ou corpo inteiro); isso ajudará a validar
as associações dos ossos no Wii físico.

## Diagnóstico

Principal: SD:/fitd-wii-test06-1-diagnostic.txt
Cópia: SD:/apps/fitd-wii/wii-diagnostic.txt

Novidade:
FITD-WII-BONES06-1 actor_mode=1 pose=1 body=15 anim=301
  group_count=20 root_yaw_suppressed=1
  extent=X,Y,Z frame=... alpha=...

O valor extent mede as dimensões do corpo em coordenadas LOCAIS do
modelo (sem a distância da câmera). Mesmo com perspectiva variável,
a extensão corporal deve variar apenas conforme a pose, e não por
um bug de rotação hierárquica.

## Limitações

Esta ainda é a prévia técnica de ator e câmera do FITD, NÃO o motor
jogável. Continua faltando estado original de atores e objetos,
scripts, inventário, combate, áudio, salvamento e transições.
A correção geométrica foi comparada com Free in the Dark e passou
pelos testes sintéticos, mas PRECISA de confirmação visual no Wii.

Créditos: yaz0r/FITD, dehalbuq/FITD-R36S e colaboradores,
licença GPL-2.0 e demais licenças aplicáveis.
Sem nenhum arquivo proprietário do jogo no pacote. Use os arquivos
da sua cópia original. Nintendo não é afiliada ao projeto.

---

# FITD Wii Teste 06 - ator, câmera e máscara automática

STATUS: PROTÓTIPO TÉCNICO NATIVO PARA NINTENDO WII, NÃO É O JOGO COMPLETO.

Este teste parte do Wii 05.3, cuja oclusão foi confirmada visualmente
em hardware. O código anterior não foi alterado: Teste 05.3,
Android Teste 19 e R36S ARM64 permanecem preservados.

Novidades do 06:
- BODY 15 e ANIM 301, modelo e animação originais da cópia do usuário,
  carregados juntos para representar o ator de teste.
- Modelo 3D projetado nos fundos CAMERA00 usando as posições,
  ângulos e valores de perspectiva vindos do ETAGE00.
- Ator movido nas coordenadas reais da sala, com bloqueio básico
  pelas zonas hard do ETAGE (volume aproximado para o ator).
- Troca automática de câmera ao entrar em polígonos de cobertura
  presentes no ETAGE, mantendo a atual se nenhuma for apropriada.
- Seleção automática das máscaras conforme as condições originais
  de retângulos X/Z de cada máscara: não usamos a união inteira.
- A prévia 05.3 ainda está disponível, permitindo comparação.

Importante: isto NÃO implementa o jogador real dos scripts do FITD.
A posição inicial é estimada em um polígono de câmera livre de colisões
(com fallback na origem); não é posição de um save original.
O volume do ator é provisório e a câmera ainda não foi testada no Wii
com os modelos em coordenadas de jogo. Precisamos validar isso antes de
dizer que a projeção está visualmente perfeita.

## Instalação

Extraia o ZIP na raiz do cartão SD, preservando seus arquivos de jogo:

SD:/apps/fitd-wii/data/
  CAMERA00.PAK
  ITD_RESS.PAK
  LISTBODY.PAK
  LISTANIM.PAK
  ETAGE00.PAK

Abra o FITD Wii Teste 06 no Homebrew Channel e pressione A.

## Controles

| Botão | Modo ator (padrão) | Visualizador 05.3 |
|---|---|---|
| Direcional | Mover ator nas coordenadas da sala | Trocar BODY / ANIM |
| Segurar B | Girar ator | Girar modelo |
| 1 | Liga/desliga máscaras automáticas | Ciclo cena/máscara/GX |
| 2 | Trocar para visualizador 05.3 | Voltar ao ator |
| A | Pausar/retomar animação | Pausar/retomar animação |
| + / - | Mudar câmera manualmente | Mudar câmera manualmente |
| B + / - | Trocar sala ETAGE | Trocar sala ETAGE |
| HOME | Sair | Sair |

No controle GameCube: X=2, Y=1, R/L=+/- e START=HOME.

## Diagnóstico

Principal: SD:/fitd-wii-test06-diagnostic.txt
Cópia: SD:/apps/fitd-wii/wii-diagnostic.txt

Procure:
- FITD-WII-ACTOR06 enabled=1 body=15 anim=301 ...
- FITD-WII-ACTOR06 spawn=... polygon_match=...
- FITD-WII-ACTOR06 active=1 ... projected=... origin_screen=...
- FITD-WII-ACTOR06 camera_auto=... pos=...
- FITD-WII-ACTOR06 occlusion=1 ... matched=...
- FITD-WII-MASK05 ... actor_auto=1 actor_matched=...
- GX frames=... fps=...
- finish ... endianness_check=PASS

Se projected=0, informe a câmera em uso e o log; pode ser calibração
da projeção ou o ator fora de quadro. Se active_masks=0, não havia
máscara aplicável à posição do ator.

## Pendente para uma versão jogável

- Importar o estado de atores, objetos e scripts do jogo completo.
- Sincronizar colisões e animação de movimento com o ZV real.
- Transições de sala/andar, inventário, IA, luta, áudio, save/load.
- Integrar as correções completas das versões R36S/Android ao executável
  Wii: elas continuam preservadas no repositório, mas ainda não estão
  rodando nesta prévia técnica.

Créditos: Free in the Dark (yaz0r/FITD, GPL-2.0),
dehalbuq/FITD-R36S, Cyril VOILA e Mark Adler e demais colaboradores.
Sem arquivos comerciais de Alone in the Dark; o usuário fornece
os dados de sua própria cópia legitimamente adquirida.
Nintendo não é afiliada ao projeto.

---

# FITD Wii — Teste 05.3: inspeção visual das máscaras de oclusão

## Por que este teste existe?

No Wii Teste 05.2, os registros comprovaram o carregamento de 1 sala,
5 câmeras, 33 zonas de colisão e todas as máscaras do ETAGE00.PAK.
O diagnóstico mostrou, entretanto, mask_preview=0 durante todo o teste:
as máscaras eram decodificadas e carregadas, mas não chegaram a ser
efetivamente ativadas no renderizador. Além disso, a UNIÃO de máscaras
pode cobrir quase toda a imagem em determinadas câmeras.

O Teste 05.3 facilita a verificação visual SEM modificar arquivos do
jogo e SEM alterar o código das versões Android/R36S/Wii anteriores.

## Como inspecionar as máscaras

1. Extraia o ZIP na raiz do SD, preservando os PAKs originais em
   SD:/apps/fitd-wii/data/, incluindo ETAGE00.PAK.
2. Inicie o app pelo Homebrew Channel e pressione A.
3. Na cena original, aperte **1 uma vez**: a sobreposição será ativada
   imediatamente, sem precisar combinar dois botões.
4. Aperte 1 novamente para a antiga demo GX. Mais uma vez para
   retornar à cena original SEM máscara.
5. Com a cena original ativa, SEGURE **B** e aperte **2** para alternar:
   união de todas as máscaras -> máscara 0 -> máscara 1 -> ... ->
   última máscara -> união.
6. Teste a câmera 3 ou 4 usando +/-: os registros anteriores mostram
   menos pixels cobertos por suas máscaras, facilitando a inspeção.
7. Saia com HOME e envie o diagnóstico.

**Atenção**: na versão 05.3, apertar **1** alterna três estados em vez
de apenas dois: cenário sem máscara, cenário com máscara e demo GX.
O botão B+1 continua alternando a máscara diretamente.
O botão 2 sozinho mantém a função de mostrar/ocultar BODY.
B+2 navega pelas máscaras individuais; sem B não muda a seleção.

## Log

Arquivo principal:

    SD:/fitd-wii-test05-3-diagnostic.txt

Procure por:
- FITD-WII-PREVIEW camera=1 mask_preview=1 ...
- FITD-WII-MASK05 OK ... masks=N ... selected=-1 (união)
- FITD-WII-MASK05 select=0 total=N pixels=... ready=1
- FITD-WII-MASK05 select=1 total=N pixels=... ready=1
- FITD-WII-STAGE05 ... mask_preview=1 mask_ready=1
- finish frames=... endianness_check=PASS

O objetivo é distinguir visualmente cada máscara e verificar como a
GPU GX compõe os pixels originais sobre o BODY. O log confirma apenas
que a camada foi comandada e tinha textura, não a exatidão visual.

## Ainda não implementado

O script do personagem, colisões ativas, escolha automática de máscara
pela posição do ator, projeção cinematográfica exata, câmera dinâmica,
inventário, IA, música, sons e save state ainda dependem da integração
do motor FITD completo. Esta é uma ferramenta de inspeção/diagnóstico,
não uma build jogável.

---

# FITD Wii Teste 05.2 — salas e câmeras ETAGE corrigidas

## Correção do erro do Teste 05.1

O Teste 05.1 passou a descompactar corretamente os dois registros do
ETAGE00.PAK (594 bytes de salas e 3.072 bytes de câmeras), mas a
leitura da tabela interna falhava com parse_error=-2.

Confirmamos nos dados reais que o diretório interno nao tem um numero
de entradas explicito: o primeiro deslocamento indica apenas o
tamanho maximo da tabela. O suposto ultimo indice ja pertence ao
primeiro registro. Em ETAGE00.PAK, sao uma sala e cinco cameras.

O novo leitor reconhece o final da tabela quando encontra um
deslocamento fora dos limites ou nao crescente. Acrescentamos testes
sinteticos dessa estrutura e novas mensagens FITD-WII-STAGE05-INNER.

### Instalação e controles

Extraia o ZIP na raiz do SD, mantendo seus arquivos .PAK originais em
SD:/apps/fitd-wii/data/. Inclua ETAGE00.PAK. Pelo Homebrew Channel,
abra FITD Wii Teste 05.2 e pressione A.

- B + 1: liga/desliga a mascara experimental de primeiro plano.
- B + tecla + ou -: alterna entre salas do ETAGE.
- + ou - sozinho: alterna camera (agora 5 imagens em CAMERA00.PAK).
- Direcional: troca modelos e animacoes.
- HOME: sai e finaliza o log.

O novo diagnostico principal fica em:

    SD:/fitd-wii-test05-2-diagnostic.txt

Ao testar, procure:

    FITD-WII-STAGE05-INNER floor=0 room_first=8 room_count=1 room_status=0 camera_first=24 camera_count=5 camera_status=0
    FITD-WII-STAGE OK floor=0 rooms=1 cameras=5
    FITD-WII-ROOM05 index=0 cams=5 ... hard=33 sce=2
    FITD-WII-MASK05 OK camera=... room=0 ...

A correspondencia entre a mascara desenhada e o objeto real ainda e
experimental. O jogo completo, com movimentacao, IA, interacoes,
audio e save, ainda nao foi integrado ao executavel Wii.

---

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
