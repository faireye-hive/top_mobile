# 05-BF — movimento do sistema de partículas SHRINK

A leitura histórica dos 19 tipos em `MPParticleSys.h` revelou uma inversão no renderizador: `SHRINK` (tipo 12) estava agrupado com `BLAST`/`BLAST3` e se expandia para fora. O DirectX cria cada partícula numa posição aleatória dentro da faixa e move-a em direção ao ponto médio dessa faixa; o OpenGL agora reproduz essa convergência, escalonando os nascimentos por `_fStep` e encerrando a partícula ao alcançar o centro. Os tipos 3 e 11 continuam expansivos.

O fixture de `UI05GTests.cpp` ganhou um segundo `.par` binário v15 do tipo 12 e verifica que a dispersão diminui com o tempo. O controlador de fogo existente continua exercitando atraso, caminho e offset, portanto a correção não removeu esses cenários.

Referência: `arquivo/fontes/Engine/src/MPParticleSys.cpp` (`_CreateShrink`, `_FrameMoveShrink`) e `arquivo/fontes/Engine/include/MPParticleSys.h` (enumeração dos tipos). Implementação: `Engine/src/top/EffectSystem.cpp`.

Validação: `top_ui05g_tests`, `top_gl --check` e 70/70 testes CTest.

Limite: isso corrige um dos 19 tipos, não fecha a equivalência visual/temporal dos demais nem os setores restantes do porte.
