# PORT-00 — auditoria executável de cobertura

O verificador lê as tabelas e os scripts em `Client/`, procura referências estáticas no código C++ de `sources/Engine/` e escreve relatórios locais. Ele não executa Lua nem altera recursos, saves ou mecânicas.

```sh
python3 sources/Tools/port_coverage_audit.py --audit-port
python3 sources/Tools/port_coverage_audit.py --audit-port --csv-dir /tmp/top-port-audit
```

`--client-root` seleciona outra cópia do cliente. A saída CSV/JSON contém:

- `items.csv`: registros de item e callback encontrado, um por ID válido;
- `item_actions.csv`: callbacks distintos, quantidade de itens e referências C++ estáticas;
- `skills.csv` e `skill_signatures.csv`: IDs e agrupamento por campos funcionais 2–18;
- `production_recipes.csv`: receitas Manufacturing, Crafting e Cooking, com saídas ou materiais ausentes do `iteminfo.txt` sinalizados;
- `garner_npc_actions.csv`: ações diretas encontradas nos scripts dos NPCs de Garner, com nível revisável no ledger;
- `lua_functions.csv`: nomes distintos definidos nos scripts e arquivos em que aparecem;
- `lua_duplicate_files.csv`: arquivos com SHA-256 idêntico;
- `summary.json`: totais e ações por frequência.

## Interpretação de cobertura

`D0` a `D4` seguem as definições em `AUDITORIA_PORTE_100_DX9_OPENGL_2026-10-26.md`. A ferramenta não infere um nível concluído a partir de busca textual: sem revisão manual, linhas de item/skill começam em `D1` (dados carregáveis) e funções Lua em `D0`. Encontrar o nome de um callback no C++ é evidência para revisão, não prova de execução funcional. Funções por ID podem herdar o nível de uma linha `item_action`; skills podem herdar o nível de `skill_signature`.

Registre classificações revisadas em `port_coverage_overrides.csv`. Valores aceitos em `entity_type`: `item`, `item_action`, `skill`, `skill_signature`, `lua_function` e `npc_action`. Para ações de NPC de Garner, use a chave composta `NpcScriptNN::funcao::acao` (por exemplo `NpcScript01::r_talk04::ReAll`). Para marcar um nível diferente de `D0`, inclua `evidence` com caminho e símbolo/fluxo verificável. Use `approximation=true` quando a regra for provisória.

## Limites dos dados desta cópia

Na execução de 26/10/2026, `iteminfo.txt` tinha 6.345 linhas, incluindo o cabeçalho. 6.343 registros tinham ID numérico; uma linha de conteúdo está corrompida/sem ID e é contabilizada como `malformed_item_rows_skipped`. O inventário do pacote registra 6.344 registros, portanto essa linha precisa ser recuperada da fonte antes de ser auditada por ID. Não é incluída artificialmente como item.

As assinaturas de skills são agrupamentos de colunas históricas e servem para triagem; semânticas Lua, efeitos visuais, fórmulas, transições de estado e saves exigem conferência própria. A extração de nomes Lua é estática e não substitui parser/runtime.

A auditoria de receitas inclui 1.224 registros ativos nesta cópia: 280 Manufacturing, 904 Crafting e 40 Cooking. Quatro receitas de Crafting apontam para produtos ausentes do catálogo (IDs 2984, 3464, 1114 e 1113). A saída preserva esses registros e sinaliza `output_in_iteminfo=no`; não cria atributos substitutos.
