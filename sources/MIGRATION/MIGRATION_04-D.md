# Migração 04-D — Inventário e habilidades

## Escopo

- `ActorSkin` preserva as assinaturas de `BagView`, `SkillView`, `setBagView`, `setSkillView`, `invHit`, `invDropHit`, `setItemDrag`, `bagWheel`, `pageBag`, `skillHit`, `skillWheel`, etc. Os tipos `ActorSkin::BagView` e `ActorSkin::SkillView` são aliases para os tipos de `top::ui`.
- Os dados dos ícones, equipamentos, bolsa temporária, itens da grade, página, ouro, texto da bolsa e arraste foram encapsulados em `top::ui::InventoryUI`. A lógica de seleção de itens, hit/drop e rolagem foi transferida do `ActorSkinUIBridge.cpp` para `InventoryUI.cpp`.
- Lista de habilidades, nomes, pontos, paginação, texto e eventos de seleção/rolagem foram encapsulados em `top::ui::SkillsUI`.
- `UIManager` mantém a posição/visibilidade dos painéis (Migração 04-B) e agora é dono dos dois novos controladores. Fechar a janela do inventário cancela arraste de ítem pendente; os itens continuam sob controle das regras de `Play`.
- `InventoryLayout.h` concentra coordenadas usadas pela renderização de inventário e pelo processamento de cliques; os valores de origem são mantidos. Não há alteração de caminhos de textura, `texture_manifest.csv`, OpenGL ou lógica de combate/equipamento.
- `ActorSkinUIBridge.cpp` continua desenhando ícones e textos através de snapshots expostos pelos controladores, **sem armazenar estados de inventário ou habilidades no `ActorSkin`**. A extração da apresentação visual da ponte é trabalho de etapa posterior.

## Contratos preservados

`invHit`: -1 fora, 0 interior, 1 fechar, 2 título, 3/4 paginação, 5 bolsa temporária, 100+slot equipamento, 200+índice bolsa, 300+índice temporário.

`invDropHit`: 100+slot equipamento, 250 grade principal, 350 bolsa temporária, 0 interior, -1 fora.

`skillHit`: ID da habilidade da linha visível ou -1. `skillWheel`: consome rolagem quando o cursor está no painel visível, mesmo se a página estiver no limite.

## Validar no Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
# Testes independentes (sem Client/):
cmake -S . -B build-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-tests --target top_ui04d_tests -j2
ctest --test-dir build-tests --output-on-failure
# Com Client/ disponível:
./build/top_gl --check
```

Nota: a verificação de compilação completa exige tempo e bibliotecas de sistema, além de executar `--check` com os arquivos históricos `Client/`. Os testes de controlador não exigem assets nem contexto gráfico.
