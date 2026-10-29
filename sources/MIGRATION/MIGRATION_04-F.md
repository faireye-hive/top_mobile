# Tales of Pirates — Migração 04-F (offline)

## Base

- Arquivo de origem: `sources_ultimo01.zip`, fornecido após a Migração 04-E.
- Escopo: organização da renderização da interface em componentes próprios, preservando a camada offline C++17 / OpenGL.
- Nenhuma alteração de protocolo, login, cliente-servidor, combate, scripts Lua, inventário real ou interpretação de mapas.
- A pasta `Client/` e seus recursos históricos **não estão incluídos** neste ZIP. Mantenha sua `Client/` atual.

## Alterações

- Novo `Engine/ui/HudDrawList.h`: modelo passivo de comandos visuais para sprites, ícones, textos e badges de contagem. Não depende de OpenGL nem de texturas carregadas.
- `InventoryUI`: emite sprites e ícones da bolsa, equipamentos, bolsa temporária e arraste; textos de ouro, hover e badges.
- `SkillsUI`: emite ícones e textos/paginação de habilidades.
- `ShopUI`, `NpcDialogUI`, `BoatUI`, `CharacterSelectUI`, `SystemUI`: emitem os sprites históricos das respectivas janelas; seus textos ficam no próprio componente quando aplicável.
- `CharacterStatusUI` e `QuestUI`: emitem textos da ficha e das missões.
- `GameHUD`: emite sprites-base do HUD e minimapa usando as mesmas texturas/coordenadas.
- `ActorSkinUIBridge.cpp`: reduzido de ~1121 para ~764 linhas. Ainda faz a **composição final** de `ActorBatch`, máscaras, fonte e minimapa; recebe os comandos dos componentes.

## Preservação de comportamento

- Os métodos públicos de `ActorSkin` e os eventos de `UIManager` **não foram alterados**.
- A ordem de composição permanece: sprites/base -> molduras procedurais -> ícones -> máscaras/badges/textos.
- As coordenadas, texturas e regiões de atlas continuam as da 04-E, inclusive a posição do cursor sobre a bolsa temporária e o destaque do barco selecionado.
- As regras de `Play.cpp`, `Platform/linux/main.cpp`, transporte offline e a tabela `texture_manifest.csv` não foram modificadas.

## Testes efetuados neste ambiente

Compilação de `top_ui04d_tests`, `top_ui04e_tests` e `top_ui04f_tests`: sucesso.

`ctest`: 3 de 3 testes passaram (04-D, 04-E, 04-F).

Verificação de sintaxe C++17 de `ActorSkinUIBridge.cpp` com `-Wall -Wextra -pedantic`: sucesso.

**Limite da validação:** a máquina usada aqui não possui cabeçalhos de desenvolvimento OpenGL (`GL/gl.h`), e `Client/` não acompanha o ZIP. Portanto, não foi possível gerar e executar `top_gl` nem verificar visualmente o resultado. A confirmação do jogo inteiro deve ocorrer no seu Linux, usando os recursos existentes.

## Compilação e execução no Linux

A partir da raiz `TalesOfPirateDX9`, extraia o ZIP para substituir sua pasta `sources/` (faça backup primeiro):

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

Testes opcionais:

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui04d_tests top_ui04e_tests top_ui04f_tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

## Verificação manual sugerida

1. Abrir/fechar bolsa (`I`), conferir equipamentos, ouro, tooltip e ícones; testar bolsa temporária e arrastar/soltar.
2. Abrir habilidades (`K`), rolar páginas, conferir ícones/textos/SP.
3. Abrir ficha (`C`) e missões, verificar conteúdo, movimentação e sobreposição das janelas.
4. Interagir com NPC e loja e verificar nome, fala, escolhas, preços e ícones.
5. Abrir estaleiro (`N`), seleção de personagem e sistema; conferir imagens e destaque.
6. Confirmar minimapa, barras HP/SP/EXP, botões e atalhos.
7. Rodar `--check` e verificar que os números de objetos, texturas, atores e efeitos continuam consistentes.

## Próximas fases sugeridas

- 04-G: separar a montagem de `ActorBatch`/texto/minimapa da ponte para um renderizador HUD dedicado. Isso **não** foi concluído na 04-F.
- 05-B: revisar transformações de ossos, dummies e equipamentos em comparação com DirectX 9.
- Protocolo de rede: por último e opcional.
