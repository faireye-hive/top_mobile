# Migração 05-BO — música histórica por área no cliente offline

## Problema

O `AudioSystem` selecionava a música apenas pelo nome do mapa, com uma tabela fixa para cinco mapas. O cliente DirectX chama `PlayMusic(pArea->nMusic)` em `WorldScene.cpp` quando muda `getIsland()` do tile atual. O ID vem de `scripts/table/areaset.txt` (`AreaSet.nMusic`), portanto uma única cidade pode trocar a música ao entrar em outro setor.

## Implementação

- `AudioSystem::open` lê `areaset.txt` e relaciona ID da área ao ID da faixa, validando que a faixa existe em `musicinfo.txt`.
- `AudioSystem::setArea` toca a faixa do setor e evita reiniciar a música quando o caminho é o mesmo. `setMap` continua como fallback para áreas ausentes/desconhecidas.
- `main.cpp` seleciona a faixa do setor de nascimento, do destino após transição e do tile atual durante o jogo. Assim, caminhar entre Argent e seus arredores troca a música sem trocar de mapa ou depender de servidor.
- `AudioSystem::selfTest` confirma mapeamentos históricos representativos: Argent (área 1 → música 2), Thundoria (8 → 3) e Shaitan (13 → 4).

Não foi copiado nenhum arquivo de `arquivo`: a tabela já existia em `Client/scripts/table/areaset.txt`, que é a mesma fonte lida pelo cliente novo.

## Verificação e limites

- Build `top_gl`: aprovado.
- `./build/top_gl --check`: aprovado; a auditoria confirmou as tabelas e dependências de áudio.
- CTest: 70/70 aprovados.
- A troca foi verificada pelo caminho de dados e integração no loop, não por captura de áudio em uma sessão interativa. Sons ambientais espaciais e vozes continuam fora deste incremento.
