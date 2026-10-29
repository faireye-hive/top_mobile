# Migração 05-O — Animação histórica dos monstros

## Defeito identificado

`ActorSkin::open` carregava as ações do `CharacterAction.tx` diretamente pelo **ID do personagem**.
O `characterinfo.txt` possui um campo separado chamado **Action ID** (coluna 20, contagem a partir de 0), que define o banco de ações compartilhado. Muitos monstros não têm banco com o próprio ID. Nesse caso, o código ficava na posição inicial do `.lab`, que pode representar a criatura deitada ou em uma pose inadequada.

Exemplos verificados em scripts originais: Forest Spirit 103 -> 114, Grass Tortoise 119 -> 84, Mystic Flower 185 -> 75, Slowpoke Snail 127 -> 71, Weird Monster 6 -> 71.

## Correção

- Carrega `characterinfo.txt` antes do arquivo de ações.
- Busca as poses com o valor de **Action ID**; mantém o ID do personagem como fallback para clientes modificados.
- Seleciona a pose de espera e caminhada alternativas (4 e 6) se as poses 1 e 5 estiverem ausentes. Ataques podem usar 8 ou 9 se 7 não existir.
- As animações de morte e sentar executam **uma única vez**, ficando no quadro final em vez de reiniciar.
- `--check` exibe quantos monstros utilizaram bancos válidos, remapeamentos, fallbacks ou nenhuma pose.

## Limites

Não corrige rotações de malhas com rigs/modelos incompatíveis, nem valida visualmente toda criatura dos 52 mapas. O fallback em casos sem pose é preservado. Não altera a lógica do A* da 05-N nem a interface/rede.

## Compilação no Linux

```bash
cmake -S sources -B build && cmake --build build -j4
./build/top_gl --check
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Novo teste: `ui05o_monster_action_binding`.
