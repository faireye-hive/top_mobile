# Migração 04-H — HUD em camadas e correção de textos fantasmas (offline)

Base: `sources_com_04-G.zip` enviado em 2026-10-20. Não altera o protocolo de rede, combate, mapas ou missões.

## Diagnóstico a partir das capturas

- Na 04-G, `ActorSkinUIBridge.cpp::buildHud()` agrupava **todos os fundos** em um batch procedural, desenhava todos os ícones e só então **todos os textos**, num único batch de fonte. A ordem desrespeitava a sobreposição: texto de uma janela inferior aparecia sobre uma janela superior.
- Havia também quatro linhas de habilidades geradas **sempre** por `Play::pushHud`, independentemente de abrir a janela `K`. Esse texto do HUD de depuração podia ser confundido com texto vazando de uma janela.
- `CharacterStatusUI::appendText` já verificava `showState_`, mas os testes anteriores não verificavam a ordem final dos batches enviados ao renderizador.

## Alterações

- **Novo `Engine/ui/HudComposer.h/.cpp`**: monta batches de interface em camadas atômicas por janela: fundo / ícones / texto. Mantém as cores, imagens de itens e HUD procedural existentes; nenhuma chamada direta de OpenGL no compositor.
- **`ActorSkinUIBridge.cpp`**: `buildHud` agora trabalha janela por janela. Status só recebe comandos quando `showState_` é verdadeiro; camada posterior cobre integralmente a anterior. Tooltip e ícones são limitados ao retângulo da janela pelo compositor. O item arrastado vai para o topo.
- **`InventoryUI.h/.cpp`**: permite gerar separadamente os comandos do inventário, bolsa temporária e item arrastado, mantendo a mesma posição física e os métodos antigos (parâmetros novos com valores padrão).
- **`Play.cpp`**: removidas as quatro linhas permanentes de skills da tela. Habilidades continuam utilizáveis pelas teclas `1`–`4` e consultáveis em `K`.
- **`UIManager.cpp`**: alternar `C` interrompe arraste residual do Status.
- **`tests/UI04HTests.cpp`**: regressão de visibilidade, abertura/fechamento, reconstrução de batches por frame, ordem de sobreposição, clipping de texto e posição da bolsa temporária.
- **`CMakeLists.txt`**: inclui o compositor no jogo e os testes 04-H na opção `TOP_BUILD_UI_TESTS`.

## Validação efetuada neste ambiente

- `g++ -std=c++17 -fsyntax-only` nos arquivos de integração (incluindo `ActorSkinUIBridge.cpp`, `ActorSkin.cpp`, `Play.cpp`, `main.cpp`) — **passou**.
- CTest: **5/5** (`ui04d_state_and_input`, `ui04e_modal_regression`, `ui04f_visual_commands`, `ui04g_procedural_regression`, `ui04h_layered_visibility`) — **passou**.
- A compilação/link e a renderização OpenGL completa **não foram executadas** aqui, pois este ambiente não tem os cabeçalhos de desenvolvimento `GL/gl.h`. Validar no Ubuntu.

## Instalação e validação no Linux

Extrair o ZIP preservando a pasta `sources/`. Antes, faça backup de `sources/`.

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Na execução gráfica: (1) iniciar sem abrir janelas — não deve aparecer `Character`, `Base Attributes`, `Reputation`, nem a lista permanente de habilidades; (2) `C` abre/fecha Status; (3) abrir e sobrepor `C`, `K`, `I` e missões para confirmar que o texto da janela de trás não atravessa a da frente; (4) arrastar a loja e fazer uma compra; (5) arrastar itens entre inventário e bolsa temporária; (6) verificar minimapa e barra de skills.

### Sobre o erro `XIO: fatal IO error 2`

Essa mensagem indica que a conexão da aplicação com o servidor X foi interrompida. Não é um erro do CTest nem é prova de falha nos comandos de HUD. Se ocorrer sem fechar a janela ou encerrar a sessão, coletar os logs da sessão X11 e a sequência de eventos para diagnóstico separado.

### O que ainda não foi migrado

O parsing, cache e o raster do minimapa continuam em `ActorSkinUIBridge.cpp`. A refatoração dessa parte e o gerenciamento dinâmico de foco / z-order de cliques das janelas sobrepostas podem ser feitos em migrações posteriores, com testes próprios.
