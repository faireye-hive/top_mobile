# 05-BI — altura marítima dos barcos

O cliente DirectX não coloca barcos no relevo do fundo do mar. `CCharacter::_UpdateHeight()` usa `SeaHeight / 100` quando `chTerritory == SEA`; no `characterinfo.txt`, por exemplo, o `Attack Boat 1` (ID 305, ligado ao `shipinfo` 0001) tem `SeaHeight = -10`, portanto sua origem fica em −0,10 m em relação à superfície. O cliente OpenGL usava zero fixo, perdendo o deslocamento autorado por cada personagem-barco.

Agora `ActorSkin::setBoatMode` relaciona `shipinfo.cha list ID` com `characterinfo` e lê território/altura das colunas 26/27. A altura retornada alimenta o casco e a câmera; o relevo sob o barco continua excluído para não colocar o casco no fundo do mar. O `--check` valida o Guppy em −0,10 m e abre os 17 registros de barco.

`ActorSkin::open` recria os corpos e volta ao modo terrestre. As reaberturas do roster agora preservam o ID do casco e reinstalam o barco depois da carga, tanto nas transições entre mapas quanto no streaming de monstros/NPCs ao caminhar. Sem isso, a chegada podia ser validada como navegável, mas o personagem deixava de ser barco no mapa de destino ou quando a população local era atualizada. O `--check` cobre o ciclo de recriação e restauração.

Validação desta etapa: build `top_gl`, `top_gl --check`, CTest e `git diff --check`.
