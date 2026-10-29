# Tales of Pirates OpenGL — Migração 05-E (ações históricas do personagem)

**Base:** ZIP `TalesOfPirates_05-D_Completo.zip`, previamente validado no Linux.

## Objetivo

Retomar a fidelidade dos personagens sem mexer na interface: o ataque do jogador passa a usar os três golpes históricos (7, 8, 9) em sequência e as habilidades consultam o campo **Play Action** do `Client/scripts/table/skillinfo.txt` (coluna **47**, índice começando em zero). As IDs são ações-base e recebem o deslocamento de 54 poses do modo de armamento configurado na 05-D.

## Alterações

- `Engine/include/top/ActionPose.h` **(novo):** interpretação de listas como `7,8,9`, seleção cíclica e validação de IDs (1..54), isolados dos assets.
- `Engine/src/top/ActorSkin.cpp`: preserva as 54 ações-base de cada um dos sete modos do `CharacterAction.tx` (até então só 1, 5, 7, 16 e 17 eram armazenadas); seleciona ataque e cast em tempo de execução; reinicia o quadro quando troca de ação; fallback para a ação normal do modo, grupo zero ou ação 11 de habilidade.
- `Engine/include/top/ActorSkin.h`: recebe a ação-base atual separadamente de `setPlayerPose`, com diagnóstico `playerActionBase`.
- `Engine/src/top/Play.cpp` e `Engine/include/top/Play.h`: lê `Play Action`, alterna ataques 7/8/9, usa a ação indicada na habilidade preparada e apresenta a pose de cast por 0,45 s ao executar uma habilidade ativa. São preservados custos, recargas, dano e regras anteriores. `Play::selfTest` verifica os três golpes, a pose de cast e o campo histórico para Bare Hand.
- `Platform/linux/main.cpp`: marca a versão do executável como `05-E` no console.
- `tests/UI05ETests.cpp` (novo) e `CMakeLists.txt`: regressão isolada dos IDs, modos e parsing.

## Limites desta etapa

1. O **efeito visual** de uma habilidade ainda utiliza os sprites/partículas genéricos já disponíveis em `EffectSystem.cpp`: migrar os `.eff`/`.par` específicos de cada skill fica para uma etapa posterior.
2. As animações são lidas de `CharacterAction.tx`, mas alguns modelos `.lab` não contêm todas as variantes. Nesse caso é aplicada uma pose alternativa.
3. O momento do dano continua usando o temporizador de combate existente, calculado pela pose-base 7. As poses 8, 9 e de skills passam a existir visualmente, mas sincronizar rigorosamente cada quadro-chave/tempo de dano **ainda é pendente**.
4. Não foi alterado o chapéu, inventário, missões, NPCs, barcos, áudio, rede ou interface.

## Instalação (raiz do projeto)

```bash
cd ~/DEV/TOP2/TalesOfPirateDX9
# Faça backup de sources antes de substituir.
unzip -o TalesOfPirates_05-E_Alteracoes.zip
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

Procure no console: `top_gl offline - migracao 05-E (ataques 7-9 e poses de skills; bolsa 24-48)`.

Testes:

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui05b_tests top_ui05c_tests top_ui05d_tests top_ui05e_tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

## Validação feita na criação do pacote

- Verificação de sintaxe em C++17: `ActorSkin.cpp`, `Play.cpp`, `Platform/linux/main.cpp` — passou.
- CTest completo disponível no ambiente: **10/10** passou (04-D..04-I, 05-B..05-E).
- `Play::selfTest()` compilado fora de OpenGL e executado usando os scripts históricos do `Client.zip`: **passou**.
- **Não** houve compilação nem validação visual do `top_gl` completo neste ambiente: faltam os cabeçalhos OpenGL/GLX. A confirmação dessa parte depende do seu Linux.

## Futuro

A próxima etapa pode tratar os efeitos específicos dos scripts `.eff` por skill e a sincronização de impacto por ação (7/8/9), sem retornar à refatoração da UI.
