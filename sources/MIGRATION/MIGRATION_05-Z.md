# Tales of Pirates OpenGL — Migração 05-Z

## Objetivo

Etapa histórica de **gemas, combinação e forja**, sem substituir o renderizador rápido da 05-X6. A 05-Y ainda é a base de instâncias por UID. `Client/scripts/offline/calculate/variable.lua` contém as tabelas `GemVar` e `Item_CanJinglian_ID`; `Client/scripts/offline/script/calculate/forge.lua` (idêntico ao `GameServer/resource/script/calculate/forge.lua` enviado) fundamenta os custos e as probabilidades. Nenhum script é executado, o cliente lê somente as declarações literais `GemVar`.

## 05-Z — Regras implementadas

- Parser somente leitura para os registros `GemVar`: ID, nível máximo, efeito por nível, atributo de destino e tipos de equipamentos compatíveis. **65 gemas históricas** são reconhecidas na cópia do `variable.lua` enviada. A ausência da tabela impede aplicar/forjar gemas, em vez de conceder bônus desconhecidos.
- Cada instância mantém `gemIds[3]`, `gemLevels[3]`, `socketCount` (0..3), `stoneLevel` para gemas soltas, UID, bônus `extra` e `forgeLevel` (legado 05-Y, limitado a 15). Gemas sem nível registrado na 05-Y continuam **inativas**: não presumimos nível/encaixes que nunca foram salvos.
- No cálculo de `gearBonuses`, cada pedra válida adiciona o efeito do `GemVar` multiplicado por seu nível, além dos atributos estáticos e `extra` da instância correta. `ITEMATTR_VAL_MNATK` beneficia ataque mínimo **e** máximo conforme `check_item_final_data` do `forge.lua`. Mapeamentos suportados: STR, AGI, DEX, CON, STA, ATK, DEF, HIT, FLEE, CRT, HP, SP, PDEF. Tipo incompatível não dá bônus.
- Forja com **equipamento + gema tipo 49 + Refining Gem tipo 50** por índices da bolsa. Necessita encaixe aberto, nível suficiente de cada pedra, tipo de item compatível, ouro e espaço no encaixe. Custo: `(1 + soma dos níveis já instalados) × 100.000` ouro. Probabilidades históricas: níveis 1–3 = 100%, 4 = 45%, 5 = 35%, 6 = 25%, 7 = 15%, 8 = 5%, 9 = 2% (sem bônus de frutas/estados). Em sucesso, incrementa nível da gema instalada; em falha, mantém o equipamento. Em ambos os casos, desconta ouro e **uma unidade de cada material**.
- Combinação: **Gem Composition Scroll tipo 47 + duas pedras de mesmo ID, tipo e nível**, tipos 49 e 50. Custo fixo 5.000 de ouro. Chances tipo 49 (níveis 1–9): 100/90/80/70/60/50/40/30/20%; tipo 50: 100/100/100/95/90/85/80/75/70%. Em sucesso, a pedra da esquerda preserva UID e aumenta um nível; o pergaminho e a direita são consumidos. Em falha, as duas pedras e uma unidade do pergaminho são consumidos.
- Transações validam índices distintos e propriedades **antes** de cobrar ouro ou remover materiais; a cópia escolhida é preservada por UID.

## Save: formato aditivo, sem alterar Y1/T1/W1

Depois dos campos 05-Y são gravados os campos opcionais `|Z1:...|ZT1:...`, para bolsa normal e temporária. Cada registro contém `itemId,uid,stoneLevel,socketCount,gemLevel0,gemLevel1,gemLevel2`. Na leitura, a correspondência é por `(itemId, UID)` e só é aplicada se a estrutura for consistente com `gemIds` de Y1. Saves 05-Y e anteriores continuam funcionando: as propriedades novas recebem valores neutros; não existe forja inferida retroativamente. Ao salvar com a 05-Z, executáveis antigos ignoram ou perdem o formato Z, então faça backup de `Client/user/`.

## Controles offline de prototipagem (temporários)

**As telas históricas de Blacksmith/refinamento ainda não foram portadas.** Para usar a lógica implementada é necessário identificar índices de inventário (base zero, na ordem da bolsa). Não usamos os IDs da tabela diretamente como índices:

```bash
./build/top_gl --forge-equip 0 --forge-gem 1 --forge-refiner 2 \
  --combine-scroll 3 --combine-left 4 --combine-right 5
```

- `Alt+O`: **GM offline**, abre 1 encaixe no equipamento em `--forge-equip` (ou 1..3 com `--gm-sockets N`). Isto serve exclusivamente para teste; não implementa perfuração histórica/material de abertura de encaixes.
- `Alt+L`: **GM offline**, adiciona 1.000.000 de ouro (ou `--gm-gold N`), para testar os custos sem farm.
- `Alt+F`: prepara diálogo **Confirmar/Cancelar** para forjar o equipamento do índice especificado. Custo exibido antes de confirmar. A confirmação valida também o UID para não forjar uma cópia errada depois de rearranjo.
- `Alt+T`: prepara diálogo **Confirmar/Cancelar** para combinar gemas dos três índices especificados.
- `Alt+U` com `--gm-item ID` permite adicionar itens para os testes. IDs presentes em `iteminfo.txt`: `879` Furious Gem (tipo 49); `885` Refining Gem (tipo 50); `886` Gem Composition Scroll (tipo 47). Ao adicionar vários tipos, reinicie o executável com outro `--gm-item ID`, conservando o mesmo personagem/save. A gema e o refiner recém-criados começam no nível 1.

**Exemplo de forja de primeiro nível:** coloque uma espada no índice 0, Furious Gem no 1 e Refining Gem no 2, e execute com `--forge-equip 0 --forge-gem 1 --forge-refiner 2`. Use `Alt+O` para abrir encaixe no modo GM, `Alt+L` para ouro e `Alt+F` para confirmar. A gema Furious é compatível apenas com determinados tipos de arma, como espadas grandes (tipo 2), conforme `GemVar`.

## Limitações e próximos sistemas

Esta etapa não reproduz **perfuração de encaixes, interface gráfica de forja, frutas e estados modificadores de chance, cálculo integral de efeitos visuais de gemas** nem processos de equipamento/crafting do GameServer distintos da forja Lua testada. Os recursos não foram removidos, apenas seguem pendentes. O nível de forja do Y1 é um indicador compatível limitado a 15; os níveis reais por encaixe ficam no Z1. Nenhum item é criado ou alterado de forma automática.

## Mudanças de código

- `Engine/include/top/ForgeRules.h` — leitura literal de GemVar, compatibilidade, efeitos e tabelas de custo/sucesso.
- `Engine/include/top/ForgeInstancePersistence.h` — formato Z1 / ZT1.
- `Engine/include/top/Play.h`, `Engine/src/top/Play.cpp` — objetos por instância, atributos de gemas, forja, combinação, saves, GM offline e testes integrados.
- `Platform/linux/main.cpp` — atalhos e confirmações, parâmetros `--forge-*`, `--combine-*`, `--gm-sockets`, `--gm-gold`.
- `tests/UI05ZTests.cpp`, `CMakeLists.txt` — teste 40 `ui05z_gem_forge_rules_and_persistence`.

## Validação no ambiente de preparação

- Testes C++17 puros `top_ui05z_tests`, `top_ui05v_play_tests`: recompilados e aprovados com tabela histórica `Client/` ligada ao diretório dos testes.
- CTest: **40/40 testes aprovados**; os demais testes antigos reutilizaram executáveis da 05-Y não afetados pelas mudanças.
- Leitura de catálogo real: 65 registros de `GemVar` reconhecidos a partir do `variable.lua` histórico.
- `g++ -std=c++17 -O2 -fsyntax-only` nos arquivos principais Play.cpp e main.cpp: passou.
- **Compilação gráfica OpenGL e validação visual ainda dependem do Linux do usuário.**

## Compilar/validar

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```
