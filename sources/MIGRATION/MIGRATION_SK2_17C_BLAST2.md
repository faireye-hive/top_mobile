# SK2.17-C — BLAST2: estados históricos DX9

**Base**: SK2.17-B validada pelo usuário (99/99 testes no Ubuntu, jogo abriu).

## Por que esta migração existe

O tipo **10 (BLAST2)** era um quad genérico no OpenGL, mas o cliente DX9
implementa `_CreateBlast2` / `_FrameMoveBlast2` com um controlador único:

- Todos os slots nascem juntos com a mesma direção inicial `_vFontDir`.
- O quadro 0 avança com `frameTime/3` e deslocamento `3 * velocidade * dt`.
- Ao entrar no quadro 1, são aplicados deslocamentos laterais usando
  `vecFrameAngle[0].x`: primeiro `+valor`, depois `-valor`, depois a posição
  central, em três atualizações sucessivas.
- Em `wCurFrame==1`, o DX9 pula a integração da velocidade e a atualização de
  tamanho/cor, preservando a última amostra da curva.
- Após esse estágio, a velocidade segue com aceleração; ao chegar ao último
  quadro, o controlador **para** (não reinicia em loop).

Fonte de referência: `fontes/Engine/src/MPParticleSys.cpp`, funções
`_CreateBlast2` e `_FrameMoveBlast2`, e o `GetCurFrame` de `MPParticleSys.h`.

## Implementação

- Novo `Engine/include/top/HistoricalBlast2.h` (avaliador determinístico).
- Integração em `HistoricalParticleExtended::at(10,...)` e `EffectSystem`.
- Usa `profile.frameCount` e `profile.rotations[0]`, antes ignorados pelo
  comportamento genérico de BLAST2.
- A amostra do controlador é compartilhada entre slots por quadro para evitar
  recalcular desnecessariamente a máquina de estados.
- Modo **experimental** ativado junto com `TOP_HISTORICAL_PARTICLE_EXTENDED=1`.
  Com essa variável em 0, a SK2.17-B permanece disponível para comparação.
- Nenhuma alteração nas escalas das skills, no efeito Frozen Arrow, ou em
  Taunt/Berserk/Illusion Slash.

## Como executar

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Para comparar novas partículas preservando o gelo do Frozen Arrow:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
./build/top_gl --profile
```

Desative **somente** `TOP_HISTORICAL_PARTICLE_EXTENDED=0` para comparar com
SK2.17-A/B sem os controladores estendidos (incluindo BLAST2).

## Validação e limitações

- **15/15 testes selecionados de SK2.14–SK2.17-C aprovados em Debug**,
  incluindo o teste de estados BLAST2 e sua integração com `EffectSystem`.
- Base de testes é headless; **renderização real OpenGL requer o Ubuntu**.
- Os estados são avaliados em relógio fixo de 60 Hz. O DX9 avança com o
  `dt` real e usa `GetCurFrame`, portanto a paridade exata varia com o FPS.
- Em `.par` normal, `_vFontDir` tem valor inicial `(0,1,0)` no DX9. Alguns
  efeitos especiais de fonte podem sobrescrever esse valor via API; tal
  parâmetro não é serializado no formato `.par` e não está reproduzido aqui.
- O controlador não substitui geometria especial, dummy binding, shader
  histórico nem resolve colisão com terreno de RANGE2.
