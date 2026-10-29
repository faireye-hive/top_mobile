# SK2.17-B — Partículas históricas: FIRE/SNOW e modos adicionais

**Base:** `sources_251146.zip` (SK2.17-A). Patch relativo à raiz do repositório,
com caminhos iniciados por `sources/`.

## Achados confirmados no DX9

Origem: `fontes/Engine/src/MPParticleSys.cpp` e `MPParticleSys.h`.

* `_FrameMoveFire`: deslocamento = `normalize(_vDir) * _fVecl * dt +
  (sinal aleatório) * _vAccel * dt`. A SK2.16-B havia esquecido `_fVecl`
  no modo experimental; agora é aplicado sem coeficientes arbitrários.
* `_FrameMoveSnow`: impulso `+/- _vAccel * dt` é sorteado a cada quadro.
  **Não** é uma aceleração balística constante.
* `_FrameMoveBlast`: aceleração acumula na velocidade. A integração anterior
  permanece; sua sequência exata de quadros e o gerador RNG ainda diferem.
* `_CreateRound` / `_FrameMoveRound`: círculo com ângulos `2*pi*n/count`,
  raio `_fRange[1]`, translação `range/2`, velocidade angular `_fVecl`.
* `_CreateBlast3` / `_FrameMoveBlast3`: grupo lançado com vetor `(1,1,-1)`
  rotacionado no plano horizontal, acrescido de `_vDir`, com `_fVecl`.
* `_CreateWind` / `_FrameMoveWind`: emissão por `_fStep`, pontos de partida
  em `Randf(range)`, rotação no eixo Z com `_fVecl` e deslocamentos Y/Z.
* `_CreateRange`: posições fixas amostradas em `Randf(range)`.
* `_CreateRange2`: Z inicial igual a `range[2]` e deslocamento por direção
  normalizada com `_fVecl`; evento de impacto no terreno ainda pendente.
* `_CreateArraw`: controlador com uma partícula na âncora `range/2`;
  sua geometria de linha ainda não possui equivalência visual completa.

## Como executar

Todas as mudanças gráficas ficam **opt-in**. Para os novos modos:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
./build/top_gl --profile
```

Para incluir SNOW/FIRE/BLAST (SK2.16 + correções de FIRE/SNOW da SK2.17-B):

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
./build/top_gl --profile
```

Para comparação, reinicie o executável com `TOP_HISTORICAL_PARTICLE_EXTENDED=0`.
O modo novo não modifica o código da SK2.16-D que seleciona a textura do
Frozen Arrow. Para manter o gelo visível, preserve `TOP_HISTORICAL_MODEL_TEXTURE=1`
e `TOP_FROZEN_RESULT_GROUND=1` durante testes visuais.

## Testes

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R 'ui05sk2(14|15|16|17)' --output-on-failure
```

Com a base fornecida, os 13 testes selecionados passaram em Debug:
* 2 novos: avaliação matemática dos modos e integração real ao `EffectSystem`;
* 11 de auditoria, CMPStrip, filtros, vida, movimento, texturas e random divisor.

## Limitações reais

* O DX9 original depende de **FPS e RNG**. O jitter é uma aproximação
  **determinística de 60 Hz**, não uma reprodução bit a bit de `rand()`.
* O avaliador ainda não reproduz a ordem exata de reemissão dos slots
  inativos em `_FrameMoveSnow`/`_FrameMoveFire`.
* O modo BLAST2 (tipo 10) precisa reconstruir os estados de quique/deslocamento
  lateral de `_FrameMoveBlast2` antes de ser considerado migrado.
* Em RANGE2 faltam o teste de colisão no solo, emissão do efeito de impacto
  e alguns vínculos com controladores aninhados.
* Em WIND, ARRAW, ROUND e BLAST3, geometria e filhos `.eff` específicos ainda
  precisam ser comparados ao DX9. Os modos novos cobrem trajetória e ciclo
  de vida, **não equivalência visual total**.
* A renderização OpenGL real deve ser validada pelo usuário no Ubuntu;
  neste ambiente foram possíveis compilação headless, testes e regressões.
