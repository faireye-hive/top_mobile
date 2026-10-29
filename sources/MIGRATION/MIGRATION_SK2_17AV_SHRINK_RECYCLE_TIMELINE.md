# SK2.17-AV — timeline real de emissão e reciclagem do SHRINK

## Lacuna medida

O audit do Client encontrou 42 sistemas type 12 (SHRINK), em 32 arquivos `.par`, alcançados por 53 skills. A etapa P já aproximava posição, movimento e limite de colisão, mas a reconstrução por idade não preservava o timer compartilhado, a ordem de slots nem o renascimento dos slots mortos. A auditoria AV mede agora `shrink_pool_recycle` a partir dos próprios dados: 42 sistemas, 32 arquivos e 53 skills.

## Original DX9 x porte GL x mudança

| Assunto | Original DX9 | Porte antes | Mudança AV |
|---|---|---|---|
| Nascimento | `_CreateShrink` soma o `dt` ao `_fCurTime` compartilhado; quando alcança `_fStep`, zera o timer e cria a partícula (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2430-2475`). | O GL calculava um nascimento escalonado por `particle * step`, sem persistir timer/generation (`sources/Engine/src/top/EffectSystem.cpp:2250-2256`; helper stateless `HistoricalShrink.h:45-87`). | `TOP_HISTORICAL_SHRINK_TIMELINE=1` mantém o timer por sistema/perfil, visita slots por índice e tenta emitir uma vez por slot morto a cada update. O timer descarta excesso quando zera, conforme o original. |
| Movimento e reciclagem | `_FrameMoveShrink` atualiza os vivos; move pelo vetor normalizado vezes velocidade e `dt`; morrendo dentro da caixa estrita de ±0,5 (`MPParticleSys.cpp:2497-2528`; `PointPointRange` em `arquivo/fontes/Engine/include/MPParticleSys.h:54-58`). Slots mortos tentam recriar (`MPParticleSys.cpp:2565-2573`). | O helper anterior calculava uma só trajetória a partir da idade e não mantinha ciclo de slots. | A timeline persiste posição, direção, vida, quadro e geração por slot. Um slot criado não se move no mesmo update; slot que morre por colisão ou fim de quadros só tenta nascer no update seguinte. |
| Relógio das curvas | `GetCurFrame(dt)` avança no máximo um quadro e zera o resíduo (`MPParticleSys.h:116-129`); a vida aleatória define o tempo por quadro (`MPParticleSys.cpp:2464-2470`). | As curvas usavam idade total normalizada, sem preservar o descarte de resíduo por update. | A timeline guarda `frame` e `frameTime`, reproduz um avanço máximo por chamada e fornece a amostra normalizada para a curva GL. |
| Tradução DX9 → GL | A lógica permanece em coordenadas DX9 X/Y horizontais, Z vertical; a rotação/matrizes de D3D são do caminho original. | O renderer OpenGL desenha no mundo X/Z horizontal, Y vertical. | Só os dados de posição passam à geometria GL como `(x, altura, z)`. Nenhuma matriz D3D, estado de blend, UV, winding ou culling é copiado ou alterado. |

## Fatos, inferências e desconhecidos

- **FATOS:** `_CreateShrink` usa timer compartilhado e `Randf` para spawn/vida; `_FrameMoveShrink` percorre slots em ordem, move os vivos, encerra pela caixa por eixo e recria slots mortos quando `_bStop` está desligado. `GetCurFrame` descarta o excesso do tempo ao avançar um quadro.
- **FATO de implementação:** a nova timeline recebe os deltas reais de `EffectSystem::step(dt)`. A posição em torno do centro é mantida em coordenadas DX9 até virar vértices GL no renderer.
- **INFERÊNCIA:** o seed determinístico por slot/generation torna as comparações reproduzíveis; não reproduz o fluxo global de `rand()` do DX9, compartilhado com outros sistemas.
- **DESCONHECIDO:** a equivalência dos relógios de `EffectSystem::step` e `CMPPartSys::FrameMove` durante o cruzamento de `delay`, o estado global de RNG, e a aparência em execução. Não houve comparação visual com DX9.

## `.eff` rejeitados e utilidade

Na auditoria atual, o decoder estrito rejeita 31 arquivos, mas o modo diagnóstico recupera a estrutura dos 31; não há falha estrutural. Trinta têm referências PAR diretas. `fboomerff.eff` não tem referência direta encontrada, mas tem PAR homônimo associado à cena 919/skill 308; o loader DX9 também enumera e indexa o catálogo inteiro (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). A execução não prova que o efeito foi exibido, então não descartei nem ignorei esse arquivo.

## Ativação, comparação e reversão

Para comparar o SHRINK, desligue a variável nova e depois reinicie com:

```bash
TOP_HISTORICAL_SHRINK_TIMELINE=1 \
./build/top_gl --profile
```

A variável nova liga a timeline e sua geometria histórica; fica desligada por padrão. Para desligar, remova-a ou defina-a como `0` e reinicie. A comparação visual ainda é necessária antes de promover qualquer modo a padrão. Reversão: `git apply -R SK217AV.patch`.

## Testes e resultados

As fixtures verificam timer compartilhado, criação sem movimento no mesmo update, descarte do tempo residual, movimento por `velocity * dt`, colisão e nova geração do slot; a integração compara os vértices GL com uma timeline determinística avançada pelos mesmos deltas.

Executado: configurei e compilei em Debug, incluindo `top_gl`; CTest passou **136/136**. O teste unitário e a integração passaram sob ASan/UBSan com `ASAN_OPTIONS=detect_leaks=0` (LSan não executado). O audit leu 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 26 gaps, 307 skills com gaps, zero deriva e zero falhas estruturais. Não houve comparação visual; SHRINK permanece aproximado, não validado visualmente.
