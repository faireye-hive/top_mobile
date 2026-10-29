# Tales of Pirates — 05-W2: câmera clássica e interações por arrastar

Base: **05-W1**. Este patch mantém o motor offline, as animações, o combate,
os mapas e os dados históricos inalterados.

## Motivação

A captura do cliente original mostra uma visão elevada do terreno. A câmera
05-W1 utilizava lift 0.36–0.60 (20–31 graus acima do horizonte), expondo o
céu e partes vazias dos mapas. A 05-W2 inicia com lift 1.85 (~62 graus),
limitado a 1.35–2.40 (~53–67 graus), distância 9–22 m. Saves das opções
antigas com lift < 0.9 passam automaticamente ao novo preset. A rotação
horizontal invertida é preservada.

## Mouse e interação

- **K** abre as habilidades. **+**, à direita da linha, usa um ponto para subir
  a habilidade aprendida (limite provisório offline de nível 10).
- Arraste o ícone da habilidade até qualquer um dos **oito slots F1–F8**.
  Atribuições não ocorrem ao clicar na linha ou aprender um livro. Clique curto
  na barra lança a skill; arrastar entre slots troca as duas posições.
  Teclas **1–8**, **F1–F8** e cliques nos botões da barra ativam os oito slots.
- Arraste uma pilha da bolsa sobre a **janela da loja** para solicitar venda.
  Arraste-a para uma área desobstruída do **mundo** para solicitar descarte.
  Em ambos os casos aparece **Confirmar / Cancelar**. Nenhum item ou ouro muda
  antes da confirmação. Clique fora do botão Confirmar equivale a Cancelar.
- O uso de consumíveis ou equipamento permanece por clique normal.
  Arrastar para equipamento ou bolsa temporária mantém a ação anterior.
  Itens equipados precisam ser removidos antes de vender/descartar.
- Os índices e IDs são conferidos novamente ao confirmar, evitando utilizar
  acidentalmente uma pilha diferente se a bolsa mudar durante o diálogo.

## Persistência e compatibilidade

A barra foi ampliada de 4 para 8 slots; os IDs são gravados no mesmo campo
existente, acrescentando os quatro valores novos. Saves 05-W1 contendo quatro
valores são aceitos sem migração destrutiva. O personagem novo não recebe
atalhos automáticos. A reatribuição não zera um cooldown de uma skill já
presente em outro slot.

## Testes

`ui05w2_classic_camera_drag_and_confirm` verifica enquadramento, conversão
de preferências antigas, áreas do botão + e confirmação. `Play::selfTest()`
verifica o remapeamento manual F8→F3→F8 e a restauração do atalho no save.

A validação de lógica em C++17 foi executada localmente; **31/31 CTests**
passaram. Não há headers `GL/gl.h` neste ambiente, por isso o executável
`top_gl` deve ser compilado e verificado visualmente no Linux do usuário.

## Limitações

Esta é uma aproximação geométrica do ângulo original, **não uma cópia da
matriz de câmera histórica**. Nenhuma colisão entre câmera e cenário foi
adicionada. Drag-and-drop de itens e habilidades utiliza UI procedimental
existente e não os sprites de interface idênticos ao jogo original. Os
requisitos históricos completos de cada nível de skill ainda são parciais.
