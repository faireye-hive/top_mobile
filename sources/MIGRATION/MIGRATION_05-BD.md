# 05-BD — transições de mapa com confirmação segura

O fluxo único `travel()` atende portais, teletransportes de NPC, tickets e atalhos locais. Antes, ele fechava o mapa corrente e alterava o estado do personagem antes de conferir se o destino tinha uma célula válida de terra/água. Um destino inválido podia deixar a sessão parcialmente trocada.

Agora o cliente abre o destino num `MapFile` separado, calcula a chegada e valida a posição respeitando o modo de barco. Só após essa preparação troca os mapas ativos por `MapFile::swap`; em caso de falha, fecha apenas o candidato e mantém o mapa e o personagem atuais. O consumo de tickets continua depois do retorno de sucesso de `travel()`.

Arquivos: `Platform/linux/main.cpp` (`travel`), `Engine/include/top/MapFile.h` e `Engine/src/top/MapFile.cpp`.

Verificação desta etapa: build `top_gl`, 70/70 testes CTest, `top_gl --check` em Garner e `git diff --check`. Isso cobre regressões de compilação/recursos e não substitui a validação interativa de cada portal/ticket nos 52 mapas.
