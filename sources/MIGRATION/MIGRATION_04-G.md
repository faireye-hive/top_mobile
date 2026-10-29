# Tales of Pirates — Migração 04-G (interface procedural, offline)

Base: `TalesOfPirates_Migracao_04-F_Offline.zip` (C++17/Linux/OpenGL).

## Correções de regressão

1. **Ficha `C` — textos persistentes:** `CharacterStatusUI::appendText()` agora retorna imediatamente quando `WindowState::showState_ == false`. Dados de personagem continuam no `UIContentState` para reaparecer ao reabrir.
2. **Loja que não arrastava:** `ShopUI` mantém posição e offsets próprios, expõe `bounds()`, `hit()`, `beginDrag()`, `drag()` e usa a mesma geometria para ícones, preços, fechar e selecionar itens. A posição não é sobrescrita pelos `setWindow()` emitidos em cada quadro. `ActorSkin` continua sendo adaptador de API; o `main.cpp` encaminha `-3` (início do arraste), `-2` (fechar) e `>=0` (índice real do estoque). `Play::shopHit` permanece apenas para compatibilidade/autoteste legado; a detecção de clique do jogo usa `ActorSkin::shopWindowHit()`.

## Interface desenhada diretamente em C++

- `Engine/ui/PanelChrome.h`: estilos e primitives compartilhados (painel, botão, fechar, slot). Tudo renderizado como triângulos 2D via `UIRenderer`; **sem OpenGL direto dentro dos componentes**.
- `ShopUI`, `NpcDialogUI`, `BoatUI`, `SystemUI`, `CharacterSelectUI`: fundos e controles históricos substituídos por `draw(UIRenderer&, ...)`. A seleção de personagens usa silhuetas geométricas no lugar dos retratos históricos nesta etapa.
- `InventoryUI::drawTemp`: substitui a textura do fundo da bolsa temporária por um painel e uma grade geométricos.
- `SystemUI::appendText`: adiciona textos em código aos seis botões; antes eram parte da imagem.
- `GameHUD::drawBase`: desenho em código do quadro do minimapa, HP/SP/EXP e HUD do canto superior esquerdo. O radar em si permanece textura de dados atualizada dinamicamente. Sem radar, fundo procedural em azul.
- `Engine/ui/ActorSkinUIBridge.cpp`: continua como adaptador temporário que compõe os batches da UI; **não** foi convertido ainda o rasterizador da fonte nem o decodificador do minimapa.

Os ícones de itens e skills continuam texturas de *conteúdo de jogo*; os fundos/controles da UI não exigem as imagens antigas de `Client/texture/ui/MUI`. A renderização foi pensada para conservar o posicionamento e as áreas de clique estabelecidas nas migrações anteriores. Não altera regras de combate, missões, rede, dados ou lógica do servidor.

## Testes

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui04d_tests top_ui04e_tests top_ui04f_tests top_ui04g_tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Novos testes `UI04GTests.cpp`: bloqueio dos textos do Status ao fechar, geometria da loja, compra/fechamento na posição deslocada, posição persistente durante `setWindow()`, comandos de ícones no local correto, desenho procedural de todas as janelas, e estado de arraste.

**Validação local da entrega:** suítes unitárias CTest + `g++ -fsyntax-only` de `ActorSkin.cpp`, `ActorSkinUIBridge.cpp`, `Play.cpp` e `Platform/linux/main.cpp`. Este ambiente não possui os cabeçalhos completos de desenvolvimento do OpenGL/GLX, portanto não foi possível compilar e executar o cliente 3D aqui. A compilação integrada e o teste visual precisam ser feitos no Linux do projeto.

### Checagens visuais recomendadas

- Abrir `C`, conferir que os textos somem ao fechar com o botão e ao pressionar `C` novamente.
- Abrir a loja com `E`, arrastar pela barra de título e comprar itens **na nova posição**; fechar no `X` deslocado.
- Reabrir a loja após fechá-la e verificar que continua no local anterior.
- Abrir habilidades, bolsa temporária, missões, diálogo de NPC, estaleiro, sistema e seleção de personagem; conferir ícones e textos.
- Abrir outras janelas sobre a loja, verificando visualmente a sobreposição dos textos. A composição de camadas/textos do HUD ainda não foi inteiramente isolada da ponte e pode precisar de ajustes adicionais na próxima etapa.

## Próxima fase sugerida

**04-H:** extrair composição de `ActorBatch`, fonte e minimapa do `ActorSkinUIBridge.cpp` para serviços independentes, com ordenação explícita por camadas, e amadurecer a estética da UI procedural sem tocar no modo offline.
