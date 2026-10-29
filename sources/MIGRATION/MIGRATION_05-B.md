# Tales of Pirates — Migração 05-B: Jogabilidade offline (etapa 1)

Base: 04-J.1. Objetivo: priorizar regras de jogo em vez de continuar só refatorando UI.
Sem protocolo de rede, sem alterar mapa, física, NPC scripts ou assets históricos.

## Mudanças implementadas

1. **Inventário e pilhas**: lê `iteminfo.txt`, coluna 20, como limite máximo de pilha por item. Espadas com limite 1 deixam de ser agrupadas. Consumíveis com 99 usam pilhas de até 99 e dividem excedentes entre slots disponíveis. A inserção é atômica: se não houver espaço, o inventário não é alterado. Compra, coleta, devolução da bolsa temporária e recompensas usam a mesma regra. Não modifica formato do save `id,count`, apenas permite várias entradas do mesmo ID.

2. **Equipamento**: lê a coluna 28 (`item can equip location`) para identificar slots válidos. Espadas curtas com `9,6` podem ocupar as duas mãos, em entradas independentes do inventário. Equipamentos sem bônus de ataque/defesa continuam equipáveis. O slot `0` de chapéu passa a ser exibido na grade de equipamentos. Tirar a arma secundária não apaga a principal. **Ajustes das matrizes 3D e pontos de fixação não foram realizados**.

3. **Skills**: a janela `K` mostra somente habilidades efetivamente aprendidas. As skills continuam sendo atribuídas ao clicar na lista; os slots **F1–F4** da barra inferior agora exibem o ícone da habilidade associada, quando aprendida. Continua com quatro atalhos de combate, apesar de oito slots gráficos (F5–F8 ainda não implementados). Não foram alterados efeito, alcance, SP e cooldown das skills.

4. **NPC/Trade**: `E` sobre vendedor abre primeiro o diálogo; um botão **Trade** abre a loja. Após selecionar Trade, o inventário abre automaticamente ao lado da loja. NPCs com missão e loja oferecem as duas ações. A localização de compra continua sendo controlada pelo `Play` offline.

5. **Compatibilidade de saves**: continua lendo as pilhas `id,count` históricas, sem exigir migração. Esta fase não introduz identificadores persistentes de instância; armas com o mesmo ID e atributos diferentes ainda não são suportadas individualmente.

## Arquivos alterados

- `Engine/include/top/Play.h`, `Engine/src/top/Play.cpp`: regras, habilidades e NPCs.
- `Engine/include/top/InventoryStackRules.h`: novo helper genérico para inserção atômica.
- `Engine/include/top/ActorSkin.h`, `Engine/ui/ActorSkinUIBridge.cpp`: ícones da barra e abertura da bolsa junto à loja.
- `Engine/ui/UIManager.h`, `Engine/ui/screens/NpcDialogUI.{h,cpp}`, `Engine/ui/screens/InventoryLayout.h`: apresentação.
- `Platform/linux/main.cpp`: roteiro NPC → Trade e inventário, texto da ajuda.
- `tests/UI05BTests.cpp`, `CMakeLists.txt`: testes novos.

## Testes feitos neste ambiente

- `Play::selfTest()` linkado em executável mínimo (sem OpenGL) com dados históricos extraídos de `Client.zip`: **aprovado**. Inclui novos casos de inventário e NPC vendedor.
- `ctest`: **7 de 7 aprovados** (`ui04d`, `ui04e`, `ui04f`, `ui04g`, `ui04h`, `ui04i`, `ui05b`).
- Sintaxe C++17 dos arquivos centrais modificados: **aprovada**.
- Compilação de todo o `top_gl`: **não realizada**, pois faltam cabeçalhos `GL/gl.h` neste ambiente. É necessário validar no Linux.

## Instalação e verificação no Linux

Extraia o ZIP completo ou incremental na raiz `TalesOfPirateDX9/` (ambos trazem `sources/`).

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

## Testar visualmente

- Com inventário aberto, compre duas espadas iguais: devem ocupar **duas células** da bolsa e permitir equipar em **ambas as mãos**.
- Compre consumíveis empilháveis: a contagem segue o limite histórico.
- Equipe um chapéu: aparece no **slot 0** do inventário. A posição do modelo 3D ainda pode precisar de correção.
- Fale com um vendedor usando `E`: diálogo → `Trade` → loja e bolsa abertas lado a lado.
- Abra `K`: aparecem só as habilidades aprendidas. Clique numa habilidade aprendida: ícone aparece na barra F1–F4 e a tecla `1`–`4` continua ativando-a.

## Próximos passos

- Identidade persistente de itens únicos, melhorias e encantamentos, caso os saves passem a exigir isso.
- Correção das matrizes e dummies de chapéus/armas; efeitos visuais e scripts históricos.
- Drag & drop específico para atribuir skills a F1–F8, e associação de itens consumíveis aos atalhos.
- Protocolo de rede opcional, por último.
