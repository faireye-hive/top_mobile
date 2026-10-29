# SK2.16-B — movimento DX9 de SNOW/FIRE/BLAST (experimental)

## Objetivo e base

Incremental sobre SK2.16-A. Nao modifica o resultado aprovado do Taunt ou Berserk por padrao; nao muda qualquer tamanho ou efeito da Illusion Slash.

No DX9 original (`fontes/Engine/src/MPParticleSys.cpp`) encontramos:
- `Randf(range)` produz deslocamento positivo dentro da faixa do emissor, e nao uma amostra centrada multiplicada por 0.22;
- `_CreateFire` normaliza `_vDir` e `_FrameMoveFire` avanca usando a direcao, sem `velocity * age * .12` nem uma subida arbitraria de `.2`;
- `_CreateSnow` cria velocidades independentes por eixo entre `velocity/2` e `velocity`, com mudancas de sinal nos eixos horizontais; `_CreateSnow` recusa componente vertical nula;
- `_CreateBlast` nasce em grupo, desloca `m_vOldPos` e acumula `m_vAccel` no tempo.

Implementamos os calculos de **posicao** em `HistoricalParticleMotion.h` (ordem D3D X/Y horizontal, Z vertical; traducao feita no `EffectSystem`): amostragem independente por particula e geracao, direcao de FIRE normalizada, velocidade de SNOW e integracao de aceleracao do BLAST.

## Como habilitar

A SK2.16-B permanece desligada por padrao. Para comparar:

```sh
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 TOP_HISTORICAL_PARTICLE_MOTION=1 \
./build/top_gl --profile
```

Para desativar somente a SK2.16-B:

```sh
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=0 ./build/top_gl --profile
```

**Importante:** o movimento novo exige o pool individual de SK2.16-A ativo. Se `TOP_HISTORICAL_PARTICLE_POOL=0`, a SK2.16-B nao interfere em nenhuma particula.

## Frozen Arrow: investigacao separada do movimento

O `Client/effect/icearrowh.par` historico possui:

- STRIP tipo 6, referencia a `icearrowh.eff`, atraso 0.1 s, `playTime=2.8`, tamanho 0.7;
- duas camadas FIRE tipo 2, com texturas `smoke1` e `eff0232`;
- `icearrowh.eff` possui seis subefeitos `ice.lgo` com chave inicial de alfa/escala Z nula, crescendo depois nos primeiros 0.1–0.35 s. Seu carregamento depende de `Client/model/effect/ice.lgo` e das texturas/materiais presentes no Client.

A SK2.16-B **nao afirma recuperar o gelo do chao**: isso depende sobretudo do STRIP/aninhamento do `ice.lgo` e dos estados de renderizacao, que nao fazem parte do movimento FIRE/SNOW/BLAST.

Para diagnosticar a presenca das camadas, executar uma Frozen Arrow no Ubuntu:

```sh
TOP_TRACE_SKILL_FX=1 TOP_TRACE_FROZEN_RESULT=1 \
TOP_HISTORICAL_BLEND=1 ./build/top_gl --profile 2>&1 | tee sk216b_frozen.log

grep -E '\[frozen-result\]|nested-eff=icearrowh|skill=93.*kind=result' sk216b_frozen.log
```

Esperado com todos os recursos disponiveis: `model=icearrowh.eff tracks=6` (o parser possui seis camadas; conteudo real depende da disponibilidade de `ice.lgo` e das texturas). Se `tracks=0`, a falha deve ser investigada **antes** de mudar blend/escala.

## Limites de fidelidade

`rand()` e os sinais da aceleracao em FIRE e SNOW mudam por quadro no DX9. A integracao deterministica por `effect.seed` usa aceleracao media nula nesses dois emissores; para BLAST utiliza o modelo continuo `v*t + a*t*t/2`. Portanto, e uma aproximacao estruturada, nao o estado aleatorio identico ao DX9 nem uma promessa de paridade visual. Geometria e escalas previamente existentes foram preservadas.

## Testes feitos

- Novos testes puros de movimento e coordenadas;
- novo teste integrado `EffectSystem`, com controlador `.par` sintetico;
- CTest em Debug: **13/13 aprovados** para SK2.10 a SK2.16-B.

Nao executado neste ambiente: o OpenGL real, sprites vistos pelo usuario ou o `Client` com modelos externos `.lgo` completos. O pacote nao altera saves nem sistemas de jogo.
