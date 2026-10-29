# SK2.16-A — Ciclo de nascimento e vida das partículas (experimental)

## Base e objetivo

Patch incremental sobre **SK2.15-C**. Os testes de filtros de textura do usuário
(`nearest=255`, `linear=128`, `legacy=128`) estão OK e não são modificados.

O DX9 original (`fontes/Engine/src/MPParticleSys.cpp`) tem caminhos distintos:

- `_CreateSnow` / `_CreateFire`: uma partícula nasce quando `_fStep` autoriza,
  recebe vida aleatória entre `life / _iRoadom` e `life` (em geral `_iRoadom=2`),
  avança em seus próprios quadros e recicla slots mortos até a emissão parar.
- `_CreateBlast`: inicia o grupo de partículas de uma vez. Cada partícula
  envelhece de maneira independente e uma execução em loop reinicia o grupo.
- O porte anterior usava `effect.age` para cor, tamanho e animação, com
  `particle * step` **somado à idade** (mesmo antes do nascimento), sem
  estado de nascimento/morte por slot.

## Implementação

`HistoricalParticlePool.h` introduz um avaliador independente de FPS que
retorna para cada slot `alive`, `age`, `life`, `normalized`, `generation`.

`EffectSystem::build()` o usa em **SNOW=1, FIRE=2 e BLAST=3** para:

1. Ignorar partículas que ainda não nasceram ou já morreram;
2. Calcular tamanho, cor, rotação e animações aninhadas a partir da idade
   **individual**, e não da idade total do efeito;
3. Respeitar `delayTime`, `step`, `life` e, parcialmente, a parada de
   emissão via `playTime`;
4. Preservar o caminho anterior para MODEL, STRIP, SHADE, RIPPLE e os
   demais tipos, bem como transparência, filtro, escala e geometria.

**Opt-in:** o modo novo fica DESATIVADO por padrão.

```bash
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 ./build/top_gl --profile
```

Voltar ao estado anterior sem recompilar:

```bash
TOP_HISTORICAL_PARTICLE_POOL=0 ./build/top_gl --profile
```

## Atenção: fidelidade parcial

O DX9 usa `rand()` e `_pfDailTime`, inclusive um acumulador `_fStep`
cuja emissão pode variar com o frame rate. Para oferecer amostragem estável
em diferentes FPS, o código novo usa vidas pseudoaleatórias determinísticas
por slot e geração e escalona o primeiro nascimento por `slot * step`.
Isso é uma **aproximação estruturada**, não reprodução exata do RNG nem do
escalonador original. Os cálculos históricos de aceleração/velocidade,
model-range e a política completa de Stop()/Play() ainda precisam de trabalho.
O teste não prova fidelidade visual com o D3D.

Não alteramos o Illusion Slash (81): ele continua com impacto configurado em
2,5× e o crescimento da meia-lua permanece pendente. Taunt e Berserk exigem
verificação visual porque usam efeitos que podem incluir FIRE/SNOW/BLAST.

## Instalação

Na raiz do repositório, tendo SK2.15-C aplicada:

```bash
git apply --check SK216A.patch
git apply SK216A.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R 'ui05sk2(10|11|12|14|15|16)' --output-on-failure
```

Testes específicos:

```bash
ctest --test-dir build -R 'ui05sk216a' --output-on-failure
```

Validação local: **11/11 testes selecionados passaram** na compilação Debug,
incluindo dois novos (ciclo de vida puro + integração no EffectSystem).
Não executei o OpenGL completo nem comparação visual com o DX9 neste ambiente.

## Experimento recomendado

Compare, com a mesma câmera e mesmas condições, Taunt (242), Berserk (84),
Frozen Arrow (93) e uma skill que produza FIRE/SNOW/BLAST. Se um efeito piorar,
use `TOP_HISTORICAL_PARTICLE_POOL=0` para isolar a mudança.
