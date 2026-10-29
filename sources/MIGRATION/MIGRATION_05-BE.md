# 05-BE — temporização e offset dos emissores CMPPartSys

O leitor `.par` já entendia os limites de versão do `CMPPartSys`, mas descartava `_vOffset`, `_fDelayTime`, `_fPlayTime` e o `CEffPath` opcional (versões 5–9). O OpenGL agora preserva o offset (mapeando o eixo Z vertical legado para Y vertical do mundo GL), aguarda o atraso, mantém o evento pela emissão mais a vida da partícula e percorre os segmentos e velocidades do caminho histórico. O cursor repete no último ponto conforme `CEffPath::FrameMove`. A duração da trajetória de `Flight` continua independente para não alterar a velocidade do projétil.

O fixture de `UI05GTests.cpp` agora usa um registro binário v15 com offset, atraso, duração, caminho de três pontos e extensões de versão finais. Ele verifica ausência antes do atraso, posição deslocada ao longo do caminho e sobrevivência da cauda após o tempo de voo.

Referência: `arquivo/fontes/Engine/include/MPParticleSys.h` (`_vOffset`, `_fDelayTime`, `_fPlayTime`), `arquivo/fontes/Engine/src/MPParticleSys.cpp` (`UpdateDelay`, `FrameMove`, `LoadFromFile`). Implementação: `Engine/src/top/EffectSystem.cpp`.

Limite: emissão recorrente/aleatória, attachment a dummy, modelo animado, strip e todos os 19 tipos de partícula ainda precisam de equivalência individual e comparação visual. Esta entrega não declara portado o conjunto completo de partículas.
