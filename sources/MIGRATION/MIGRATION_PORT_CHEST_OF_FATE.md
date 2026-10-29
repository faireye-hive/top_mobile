# PORT — Chest of Fate (2916)

## Regra histórica

`ItemUse_LoveBox` exige um espaço livre. Os tipos 3/4 (Phyllis/Ami) usam o ramo feminino: `math.random(1,35)==35` cria o Love Amulet of a Princess (2902), com `VAL_STR` sorteado em 1..200; caso contrário, sorteia 1..12 para Rose (3343), Honey Chocolate (3077) ou um emote uniforme entre 4264..4273. Os tipos 1/2 (Lance/Carsise) usam o ramo masculino: rolagem inicial 1..35 sem efeito, depois 1..7; 7 cria Love Amulet of a Prince (2903), com `VAL_STR` 1..1.000, e os demais resultados seguem a rolagem 1..12 dos mesmos itens comuns. O callback também consome uma rolagem 1..7 sem efeito ao final do ramo feminino. Todas as entregas usam qualidade 4.

## Implementação OpenGL

O handler valida o tipo, reproduz a ordem das rolagens, simula a remoção do baú e a inserção de um único resultado antes de efetivar. Nas duas peças especiais, grava o atributo STR histórico; atualiza o evento de item e salva.

## Limite conhecido

Status `PORT_APPROX`: o sorteio usa o RNG local e a instância `MakeItem` é materializada pelos campos disponíveis no save OpenGL, sem reproduzir a sequência do servidor.
