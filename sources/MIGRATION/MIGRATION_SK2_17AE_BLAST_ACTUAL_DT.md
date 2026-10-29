# SK2.17-AE — BLAST aceleração com `dt` efetivo

## Resumo

O FX coverage audit mede 208 sistemas BLAST com aceleração não nula, em 66 arquivos `.par` e 64 habilidades ligadas pelo grafo. Esta etapa adiciona um replay discreto que usa os deltas passados a `EffectSystem::step(dt)`. A opção é independente e desligada por padrão.

## Comparação: original DX9 × porte OpenGL × mudança

| Regra | Original DX9 | Porte antes de AE | Mudança |
|---|---|---|---|
| Ordem de integração | `_FrameMoveBlast` obtém quadro/vida (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3891-3917`), calcula `m_vVel = m_vOldPos * dt`, move `m_vPos += m_vVel` e só então soma `m_vAccel * dt` a `m_vOldPos` (`:3923-3926`). | `HistoricalParticleMotion::at` avalia `v0*t + 0.5*a*t²` (`sources/Engine/include/top/HistoricalParticleMotion.h:74-81`). O modo O corrige o degrau presumindo 60 Hz. | A timeline por slot soma quadrados dos deltas efetivos e aplica à pose `-0.5*a*Σ(dt²)`, corrigindo o termo contínuo (`sources/Engine/include/top/HistoricalBlastMotion.h:49-88`). |
| Vida e reciclagem | `GetCurFrame` encerra partícula no limite do quadro e, quando todas morrem, reinicia grupo se `_bLoop`, ou para o sistema (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3896-3908`). | A timeline GL usa `historical_particle_pool::at` para inferir slot vivo e geração por tempo, vida sorteada determinística e contagem (`sources/Engine/include/top/HistoricalParticlePool.h:150-177`). | A correção variável mantém acumulador separado por perfil/slot e o descarta em slot morto ou nova geração (`sources/Engine/include/top/EffectSystem.h:212`; `sources/Engine/src/top/EffectSystem.cpp:1096-1115`). |
| Coordenadas | DX9 guarda vetor/posição/aceleração no espaço do motor Direct3D e renderiza com transformação de mundo da engine. | O módulo histórico GL converte a pose do perfil para os eixos do renderer (`sources/Engine/src/top/EffectSystem.cpp:2120-2123`). | A correção é somada à pose antes dessa conversão. Não há cópia de D3DX nem alteração de winding, blend ou API gráfica. |
| Relógio | A cada `FrameMove`, DX9 usa o delta corrente em cada integração (`MPParticleSys.cpp:3923-3926`). | O modo O modelava deltas nominais de 1/60 s; a aproximação contínua era default. | `TOP_HISTORICAL_BLAST_ACTUAL_DT=1` acumula os deltas ativos recebidos em `EffectSystem::step`; exige também `TOP_HISTORICAL_PARTICLE_POOL=1` e `TOP_HISTORICAL_PARTICLE_MOTION=1` (`EffectSystem.cpp:1050-1055,1096-1115`). |

## FATOS, INFERÊNCIAS E DESCONHECIDOS

**FATOS:** No original, o movimento precede a atualização da velocidade por aceleração (`MPParticleSys.cpp:3923-3926`). A timeline GL aplica a correção por slot/generation após avaliar a pose analítica (`HistoricalBlastMotion.h:78-88`; `EffectSystem.cpp:2098-2115`). A fixture usa deltas `.2, .1, .2` s e confirma diferença de centro `−0.5 × 2 × (.2²+.1²+.2²)` contra o modo contínuo.

**INFERÊNCIA:** Para aceleração constante e a mesma sequência de passos, a soma corresponde à integração explícita DX9 antes da conversão de eixos. A identidade matemática não prova que estado DX9 e amostragem do perfil GL coincidam em qualquer cenário de loop.

**DESCONHECIDO / LIMITAÇÃO:** O porte reconstrói geração e vida pelo avaliador determinístico do pool; não reproduz o `rand()` global do cliente. A timeline recebe `dt` por chamada GL, que pode diferir do `dwDailTime` do executável DX9. Loop, atraso parcial, pausa e grande salto de frame não foram comparados visualmente. Este modo permanece aproximado e experimental, não validado como fiel.

## Auditoria e testes

O audit Client mediu 208 sistemas, 66 `.par`, 64 skills no feature `blast_discrete_acceleration`; 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 partes `.eff`, sem deriva nem falha estrutural no modo diagnóstico. O JSON separa **31 falhas estritas**, todas recuperadas pelo modo diagnóstico: 30 EFF têm referência direta em PAR e ID em `sceneffectinfo`; `fboomerff.eff` não tem referência direta encontrada, mas seu desuso não está provado porque o DX9 carrega globalmente os `.eff`. Essa cobertura quantifica dados e vínculos, não prova execução no jogo. A linha do audit aponta para `historical_blast_motion::actualDtEnabled`.

Testes O verificam gate desligado por padrão, acumulação de `dt²`, reset em nova geração e parada em slot morto. A integração contrasta modo nominal 60 Hz com deltas `.2/.1/.2`. Build Debug completo incluindo `top_gl`; CTest **129/129**; os dois testes focalizados passaram; a fixture de integração passou com ASan/UBSan (`detect_leaks=0`). Audit Client repetido após a alteração: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 23 lacunas, zero deriva e zero falhas. Nenhuma comparação visual foi executada.

## Ativação e reversão

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_BLAST_ACTUAL_DT=1 \
./build/top_gl --profile
```

Desative removendo ou definindo `TOP_HISTORICAL_BLAST_ACTUAL_DT=0` e reinicie. O modo nominal O pode ser testado separadamente com `TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION=1`. Reversão desta etapa: `git apply -R SK217AE.patch`.
