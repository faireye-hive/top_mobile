# Tales of Pirates OpenGL — Migração 05-Y

## Contexto

A 05-X6 permanece como base gráfica. O modo do monitor indicado pelo usuário é `1920x1080 74.97* Hz`: o teto de aproximadamente 75 FPS com VSync não era um limitador interno do jogo. Sem VSync, a 05-X6 chegou a 165–220 FPS naquele percurso; esta etapa não altera o renderizador.

## 05-Y: identidade por instância de equipamento

O cliente antigo utiliza `CItemAttr`/`ItemAttrType.h` para atributos individuais. O antigo `CForgeRecord` e `forge.lua` tratam separadamente as regras de refinamento, materiais, gemas e probabilidades; não são reproduzidas automaticamente nesta etapa.

- Cada `Play::Stack` possui `uid` único por cópia física, `forgeLevel` (0–15), três IDs de gemas (0 indica ausente) e `equipment_stats::Bonus extra` separado dos atributos estáticos de `iteminfo.txt`.
- `worn_` continua contendo `slot -> itemId` para `ActorSkin::setEquipment`, mas a nova `wornUid_` registra `slot -> uid` para associar os bônus à cópia correta. Duas espadas iguais podem ter modificadores distintos, sem confundir a mão direita com a esquerda.
- `gearBonuses()` soma bônus estáticos do item com os **bônus extras já armazenados** na instância equipada. Não rola bônus aleatórios a partir da tabela e não fabrica efeitos para gemas ou níveis de forja.
- `addBag()` cria novos UIDs para entradas inseridas. Equipar/desequipar, mover itens não empilháveis para a bolsa temporária e de volta, e descartar/recolher preservam atributos individuais e UID.
- Descarte de uma pilha sempre deixa apenas **uma unidade** no chão, evitando duplicação de quantidades.
- Os dados de loot no chão não são persistidos no disco entre execuções; esta é uma limitação anterior e permanece.

## Compatibilidade do save

Os campos históricos permanecem exatamente na ordem anterior. Após o SP atual (`|capacidade|spAtual`) foram adicionados campos opcionais:

- `|Y1:` registros da bolsa principal em ordem, com `itemId,count,uid,forgeLevel,gem0,gem1,gem2,` e dezoito inteiros de bônus extras.
- `|T1:` mesmo formato para a bolsa temporária.
- `|W1:` associação `slot,uid` de equipamentos vestidos.

Saves 05-X6 ou anteriores **são legíveis**; se não existir o bloco Y1, o motor cria UIDs próprios e associa os slots antigos por ordem estável de cópia/ID. Metadados incompatíveis com o `itemId,count`, inválidos ou com IDs de instância duplicados não são aplicados às cópias erradas. A linha é lida integralmente (até 128 KiB), evitando o truncamento antigo de `fgets(buf,4096)` em bolsas expandidas.

**Atenção:** abrir e salvar uma partida 05-Y em um executável anterior elimina os metadados novos. Faça cópia de `Client/user/` antes de trocar entre versões.

## Escopo intencional

Esta é a **infraestrutura de instâncias individuais**, ainda não um sistema completo e jogável de combinação/refinamento. Não há botões de forjar, consumir gemas ou sortear atributos de item; os campos ficam neutros para itens existentes, e seus valores individuais serão preenchidos pelas regras históricas em migrações futuras. Isso evita inventar resultados que dependiam do GameServer/Lua original.

## Arquivos alterados

- `Engine/include/top/ItemInstancePersistence.h`: serialização e validação dos metadados adicionais.
- `Engine/include/top/Play.h`: novos campos de instância, UID e estado de equipamento.
- `Engine/src/top/Play.cpp`: save/load, atributos, equipamento, transferências, descarte/recolhimento, testes integrados.
- `tests/UI05YTests.cpp`: valida IDs iguais, dados independentes, corrupção e reconstrução de slots.
- `CMakeLists.txt`: registra CTest 39 `ui05y_item_instance_persistence`.
- `Platform/linux/main.cpp`: banner de 05-Y.

## Testes realizados

- CMake: build de todas as 39 suítes headless e `ctest --output-on-failure`: **39/39 aprovados**.
- `Play::selfTest`: duas armas do mesmo ID com STR extra diferente, saves Y1/T1/W1, bolsa temporária, descarte/recolhimento, pilhas sem duplicação, save grande com 48 itens e migração de save legado.
- `UI05YTests`: serialização/decodificação de atributos, gemas, forja e rejeição de registros inválidos.
- `g++ -std=c++17 -O2 -fsyntax-only` do Play foi verificado.

O ambiente de preparação não possui os headers de desenvolvimento `GL/gl.h` para compilar `top_gl`. É preciso validar a compilação e jogar no Linux do usuário. Não há alteração intencional de FPS, câmera ou renderização.

## Instalar e testar

```bash
# extrair ZIP completo ou incremental na raiz do projeto, mantendo sources/
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

**Próximo passo proposto — 05-Z:** usar os dados do `forge.lua`, `ItemAttrType.h` e das tabelas do cliente para implementar validação histórica dos materiais, gemas e refino, agora sem confundir equipamentos idênticos.
