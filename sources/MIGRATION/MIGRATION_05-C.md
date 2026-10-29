# Tales of Pirates — Migracao 05-C: inventario original 24–48 (offline)

Base: sources da 05-B (a mesma versao compilada pelo usuario).

## Funcionalidades

- A bolsa comeca com 24 espacos logicos, dos quais 16 sao exibidos na janela por vez (grade 4x4).
- Scroll vertical por linha de 4 espacos; 24 -> 2 passos, 48 -> 8 passos, mesmo com espacos vazios.
- Barra de rolagem desenhada inteiramente por codigo C++17. Mouse wheel e as areas clicaveis da coluna de scroll usam o mesmo offset da grade e dos hits dos itens.
- Expansao historica progressiva usando os itens reais do `Client/scripts/table/iteminfo.txt`:
  - 3088: 24 -> 28
  - 3089: 28 -> 32
  - 3090: 32 -> 36
  - 3091: 36 -> 40
  - 3092: 40 -> 44
  - 3093: 44 -> 48
- Clique em um item de expansao da bolsa para aplica-lo. Um item de etapa errada **nao** e consumido.
- O save offline ganhou um campo final opcional `|capacidade`; saves antigos continuam iniciando com 24 espacos. Arquivos nao sao convertidos ou apagados.
- Regras de compra, loot e recompensas de missao usam o limite atual, sem perder itens em caso de bolsa cheia.
- Quando a loja esta aberta, a roda do mouse sobre o inventario rola o **inventario**, em vez de mandar a rolagem para a loja.
- A selecao dos itens preserva os indices originais para duplo clique, arraste e equipamentos.

## Limitacoes atuais

- Os itens 3088–3093 nao sao distribuidos automaticamente: precisam existir no inventario por algum mecanismo de obtencao offline futuro. Esta etapa implementa corretamente o efeito quando estiverem presentes.
- A grade ainda compacta os objetos apos remocao; um sistema completo de celulas persistentes e movimento item->celula sera outro trabalho. Itens equipados sao exibidos nos slots do equipamento, sem ocupar a grade de itens soltos.
- A barra permite clicar nas regioes de rolagem existentes; arrastar o *thumb* como um controle continuo nao foi implementado.
- Posicionamento 3D de armas e chapeus e efeitos de itens continuam pendentes.
- Nenhum codigo do protocolo de rede foi incluido.

## Arquivos alterados

Veja o diff completo no pacote. Novos: `Engine/include/top/InventoryCapacity.h`, `tests/UI05CTests.cpp`.

## Testes

```sh
cmake -S sources -B build-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-tests --target top_ui05c_tests top_ui04d_tests top_ui05b_tests -j4
ctest --test-dir build-tests --output-on-failure
```

A compilacao Linux do executavel precisa dos pacotes de desenvolvimento GL, X11 e zlib, disponiveis no computador que ja compila `top_gl`.

## Instalacao

O ZIP completo e o incremental usam a mesma raiz `sources/`, para permitir extrair na raiz do repositorio.

```sh
unzip -o TalesOfPirates_05-C_Alteracoes.zip
find sources -type f -exec touch {} +  # somente se houver aviso de clock skew
rm -rf build
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

## Roteiro recomendado

05-D: modelos, esqueletos e *dummies* dos equipamentos, com foco na arma secundaria e chapeu; depois efeitos visuais, cooldowns e UI de habilidades.
