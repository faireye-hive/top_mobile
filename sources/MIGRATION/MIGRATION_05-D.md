# Tales of Pirates OpenGL — Migracao 05-D (poses por equipamento)

Base: pacote **05-C completo**. Objetivo: recuperar animacoes historicas quando se equipa uma arma; nenhum recurso de rede ou componente grafico da UI foi alterado.

## Alteracoes de verdade

- `Engine/include/top/WeaponPose.h` (novo): reproduz as 15 regras de `__wield_state` do `fontes/Client/src/CharacterModel.cpp` DX9. Slots de mao esquerda/direita = 6/9. `iteminfo.txt` coluna 10 guarda o **tipo** da arma. Modelos duais e arma de duas maos recebem modos distintos.
- `Engine/src/top/ActorSkin.cpp`: carrega as 7 variantes das poses 1 (parado), 5 (correndo), 7 (atacando), 16 (sentado), 17 (morto) de `CharacterAction.tx`. Cada grupo de 54 acoes usa o deslocamento documentado em `CharacterPoseInfo.txt` do fonte historico. Depois que um equipamento valido e carregado, seleciona o grupo animado correspondente. Faz fallback por acao para a variante sem arma quando falta uma pose ou ela nao cabe no `.lab`.
- `Engine/include/top/ActorSkin.h`: adiciona `ActorReport::playerPoseMode` para facilitar a verificacao.
- `Platform/linux/main.cpp`: identificador de versao 05-D no console e `--check` verifica a troca dos modos de pose ao equipar faca simples e duas facas/espadas.
- `CMakeLists.txt` e `tests/UI05DTests.cpp`: teste independente das regras, da indexacao de grupos e das 7 tabelas de 54 acoes. O teste 05-C tambem esta registrado nesta base.

## O que NAO foi alterado

- Posicao do chapeu: ja estava correta no modelo 3D; a correção citada pelo usuario era apenas do slot na bolsa.
- HUD, atributos, empilhamento, barra de skills, protocolo de rede.
- O posicionamento/dummies das armas 3D continuam usando o codigo anterior. A 05-D troca as poses do esqueleto, **nao** redesenha equipamentos nem implementa efeitos de itens.
- Nao estao implementadas transicoes suaves entre poses nem todas as acoes 2/3/8/9/10 ou variantes de skills.

## Instalar e verificar na pasta do projeto

O ZIP completo e o incremental usam prefixo `sources/`. Extraia na **raiz**, nao dentro de `sources/`:

```bash
cd ~/DEV/TOP2/TalesOfPirateDX9
unzip -o TalesOfPirates_05-D_Alteracoes.zip
find sources -type f -exec touch {} +
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

Identificador esperado na saida: `top_gl offline - migracao 05-D (poses por equipamento; bolsa 24-48)`.

Testes:

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui05b_tests top_ui05c_tests top_ui05d_tests -j4
ctest --test-dir build-ui-tests -R 'ui05(b|c|d)_' --output-on-failure
# inspecionar se CMake leu a versao correta:
grep -n top_ui05d_tests sources/CMakeLists.txt
```

## Validacao neste ambiente

- Teste C++17 de sintaxe `ActorSkin.cpp`: passou.
- `top_ui05b_tests`: passou.
- `top_ui05c_tests`: passou.
- `top_ui05d_tests`: passou.
- Cliente completo: nao compilado aqui (dependencias `GL/gl.h`, `GL/glx.h` indisponiveis).
- Verificacao integrada `--check` com arquivos `.lab`, `.lgo` e mapas: **pendente no Linux do usuario**.

A migracao foi feita sobre o fonte do pacote 05-C, nao sobre arquivos locais do usuario. Se surgirem diferencas, comparar com o diff ou reenviar o estado mais recente.
