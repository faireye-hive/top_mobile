# Migracao 04-E — Modais e interfaces offline

Base: `TalesOfPirates_Migracao_04-D.zip` (fonte OpenGL/C++17 Linux). **Nao** altera o cliente DirectX 9 antigo, os scripts de `Client/` ou o protocolo de rede.

## Alteracoes

- `Engine/ui/screens/{NpcDialogUI,ShopUI,BoatUI,SystemUI,CharacterSelectUI}.{h,cpp}`: cinco controladores de apresentacao e eventos. O estado e os valores de exibição deixam de pertencer a `ActorSkin`.
- `Engine/ui/screens/ModalLayout.h`: retangulos dos modais usados pelo HUD e pelo bloqueio dos cliques na UI.
- `Engine/ui/UIManager.{h,cpp}`: os novos controladores sao membros de `UIManager`, que coordena a abertura do menu de sistema (fechando NPC e loja, como antes).
- `Engine/include/top/ActorSkin.h`: remove `npcWindow_`, `shopWindow_`, `characterWindow_`, `systemWindow_`, `boatWindow_` e os campos associados. Permanece com os estados de modelo/barco (`boatMode_` e `boatId_`) porque sao parte da representacao do personagem, nao flags de janela.
- `Engine/ui/ActorSkinUIBridge.cpp`: os metodos publicos antigos continuam como adaptadores; HUD usa os dados fornecidos pelos componentes independentes. Renderizacao final dos sprites e da maior parte dos textos permanece no bridge para a etapa **04-F**.
- `Engine/src/top/ActorSkin.cpp`: ao embarcar, fecha `ui_.boat` em vez de alterar uma flag propria.
- `Platform/linux/main.cpp`: usa `ActorSkin::offlineTravelHit` para determinar onde foi clicado na grade historica de quatro destinos; regras offline de troca de mapa e salvamento continuam iguais.
- `CMakeLists.txt`: inclui os novos `.cpp` e o teste `top_ui04e_tests`, disponivel com `-DTOP_BUILD_UI_TESTS=ON`.

## Contratos preservados

- `setNpcWindow`, `setTeleportChoices`, `teleportHit`, `setShopWindow`, `setCharacterWindow`, `characterHit`, `toggleSystemWindow`, `closeSystemWindow`, `systemWindow`, `systemHit`, `toggleBoatWindow`, `closeBoatWindow`, `boatHit` continuam disponiveis com suas assinaturas originais.
- `systemHit`: `0` sem acao, `-1` fechar, `1..6` opcoes.
- `boatHit`: `-1` sem acao, `0` fechar, `1..17` casco.
- `characterHit`: `0` sem selecao, `1..4` personagem.
- `teleportHit`: funcao legacy 2x2 mantida; `offlineTravelHit` encapsula a area de quatro colunas que ja existia no `main.cpp`.
- `Play.cpp` segue como fonte da verdade para lojas, quests, conversa com NPC, combate e inventario. Nao ha dependencia de rede.
- `texture_manifest.csv`, nomes e caminhos de recursos e formatos de asset nao foram alterados.

## Integracao (na raiz do seu repositorio, que contem `sources/` e `Client/`)

> Faca backup ou commit antes. Extraia o arquivo no diretorio raiz e permita a substituicao dos arquivos existentes em `sources/`. O ZIP inclui **o codigo-fonte completo**, nao apenas os arquivos alterados.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

Testes de UI independentes de `Client/` (opcionais):

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui04d_tests top_ui04e_tests -j2
ctest --test-dir build-ui-tests --output-on-failure
```

### Aviso de clock skew

As datas futuras no terminal sao relacionadas a timestamp de fonte/ZIP e nao representam erro de compilacao. Este ZIP usa datas anteriores ao dia da entrega para evitar esse aviso. Se ele persistir apos extracao, verifique o relogio do sistema (`timedatectl`) e atualize timestamps dos fontes extraidos quando necessario (`find sources -type f -exec touch {} +`).

## Validacao neste ambiente

- `top_ui04e_tests`: compilado e executado com sucesso.
- `top_ui04d_tests`: compilado e executado com sucesso em uma rodada anterior. O CTest executou ambas as suites (2/2); uma compilacao limpa posterior da suite 04-D foi interrompida por limite de execucao do ambiente.
- Verificacao `g++ -std=c++17 -fsyntax-only` passou para `ActorSkin.cpp`, `ActorSkinUIBridge.cpp`, `Play.cpp` e `main.cpp` apos as mudancas.
- A compilacao e o link completos de `top_gl` **nao** foram realizados aqui por falta de cabecalhos OpenGL de desenvolvimento (`GL/gl.h`). Execute o build e `--check` na sua instalacao Linux com `Client/` presente.

## Teste manual recomendado (client offline)

1. Inicie em Argent City (`garner`), confira HUD e NPCs.
2. Fale com NPC, aceite/recuse missao, abra loja, compre item e feche a janela.
3. Abra o menu Esc, altere video/audio, volte para selecao de personagem e selecione as quatro opcoes.
4. Pressione N para abrir o estaleiro, teste escolha de cascos e fechamento. Embarque/desembarque com B.
5. Valide teleporte offline com NPC, confirme destino e cancelamento; observe tecla 1..4 e que o mundo nao recebe cliques indevidos na UI.
6. Repita inventario (I), status (C), skills (K), janela temporaria (Alt+D), arrastar e soltar e paginacao.

## Etapas futuras

- **04-F:** mover o desenho de sprites e textos de NPC, loja, barco, menu de sistema e selecao de personagem do `ActorSkinUIBridge.cpp` para os respectivos componentes. Validar UI em diferentes resolucoes.
- **05-B:** alinhar transformacoes de esqueleto, bone-dummy e item-dummy das armas com o DirectX 9 original; especialmente espadas deslocadas das maos.
- **Rede:** opcional e **por ultimo**; continuar priorizando modo offline.
