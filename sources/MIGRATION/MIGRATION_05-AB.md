# Tales of Pirates offline — Migração 05-AB

## Objetivo

Restaurar a inspeção de equipamentos por **hover** (passar o mouse) do Tales of Pirates original, ilustrada pela janela escura de informações da arma *Tooth of the Cursed*. Até a 05-AA o inventário exibia somente o nome do item na barra superior. Sem tooltip, não era possível conferir diretamente as gemas, atributos individuais e encaixes da 05-Y/Z/AA.

## O que mudou

- `Play::pushActors` prepara linhas de informação de **cada instância**, provenientes do `iteminfo.txt`, dos metadados `Stack` persistidos por UID e do mesmo `forge_rules::effect` utilizado no cálculo dos atributos equipados.
- UI mostra título do item, ID/UID, se está equipado, quantidade, ataque básico, defesa, durabilidade máxima, requisitos de classe, personagem e nível; atributos base, bônus individuais, nível de forja, encaixes e gemas instaladas com bônus compatíveis; nível de gemas na bolsa e preço de catálogo. Valores inexistentes são omitidos.
- Nomes de classes são associados aos IDs 0..18 históricos de `Client/scripts/offline/script/calculate/JobType.lua`. Os personagens 1..4 correspondem a Lance, Carsise, Phyllis e Ami. Valores `-2` nas tabelas históricas são apenas preenchimento e não são exibidos.
- Durabilidade máxima do `iteminfo` é apresentada dividida por 50, como no método `CItemCommand::AddHint` de `fontes/Client/src/UIItemCommand.cpp`. A **durabilidade atual** por instância ainda não está persistida; a UI não a inventa.
- A busca de slots equipados agora usa `wornUid_`. Duas armas de mesmo item ID podem mostrar tooltip e ícone da cópia correta, com gemas e bônus próprios. A API antiga sem UIDs continua compatível.
- Popup surge sobre a bolsa, equipamento e bolsa temporária, acima dos painéis, com recuo nas bordas da janela. Não aparece durante arrasto ou sobre janela modal de confirmação. Usa o backend HUD OpenGL já existente (sem novos assets, texturas ou bibliotecas).
- O inventário preserva todos os IDs, posições, cliques, drag-and-drop e saves anteriores. **Nenhuma alteração de save.**

## Limitações técnicas / fidelidade

- **Não reproduz** a rolagem histórica exata de uma arma de servidor que nunca foi importada para o save offline. Por exemplo, um item com `iteminfo` de ataque 184..226 pode diferir de uma cópia histórica 187..229. A UI exibirá as informações realmente disponíveis na versão offline, não somará bônus presumidos.
- Atualmente **não há contador persistente de desgaste atual** dos itens: somente a durabilidade máxima é mostrada. Nem todos os campos que eram preenchidos pelo servidor estão disponíveis offline.
- Textos longos são abreviados para caber na largura do popup. Quando o número de linhas excede o espaço vertical, a última linha avisa que há outros atributos.
- Nomes e rótulos do tooltip usam caracteres ASCII, compatíveis com o atlas de fonte existente.

## Testes

Adicionado `ui05ab_inventory_item_tooltip_and_uid`, com verificação de duas espadas de mesmo ID e UIDs diferentes, escolha correta do item equipado, hover da bolsa, drag, ocultação do tooltip durante drag, reposicionamento ao lado da tela, e compatibilidade com a API antiga sem UID.

Executados no ambiente de desenvolvimento: **42 de 42 testes CTest passaram**, incluindo as suítes antigas recompiladas e o novo teste. `Play.cpp`, `InventoryUI.cpp`, `ActorSkinUIBridge.cpp`, `main.cpp` e o novo teste passaram em verificação de sintaxe C++17 com `-O2`. O ambiente não dispõe do pacote de headers OpenGL (`GL/gl.h`), portanto o **link gráfico integral deve ser conferido no Linux do usuário**.

## Instalação

Extraia o projeto completo `TalesOfPirates_05-AB_Completo.zip` na raiz, preservando `sources/`, ou sobreponha `TalesOfPirates_05-AB_Alteracoes.zip` à **05-AA**. Não substitua a pasta local `Client/` de recursos, mapas e saves.

```bash
find sources -type f -exec touch {} + # Opcional: resolve o aviso Clock skew após extrair ZIP
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile
```

Passe o mouse sobre o ícone de uma espada ou equipamento na bolsa (I) ou nos slots equipados. Se houver gems/forge/socket persistidos pela 05-Z/AA, aparecerão logo abaixo dos atributos. A bolsa temporária (Alt+D) também exibe a ficha.

Testes:

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Resultado esperado: **42/42**.
