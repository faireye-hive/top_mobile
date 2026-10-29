# SK2.17-BM — distribuição `Randf` no nascimento de WIND

## Escopo e evidência

O FX coverage audit atual mediu WIND (tipo 7) em **4 sistemas, 3 arquivos PAR e 2 IDs estáticos de skill**. O audit registra `assessment=approximate` e `visual_validation=not_visually_verified`. Esses vínculos medem recursos presentes, não execução em jogo.

| Original DX9 | Porte OpenGL antes desta etapa | Mudança desta etapa |
|---|---|---|
| `_CreateWind` soma `Randf(_fRange[0..2])` à posição base ao emitir (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3254-3262`). `Randf(float)` usa partes inteira/fracionária e quantização em milésimos (`arquivo/fontes/Engine/include/MPParticleSys.h:26-37`). | `historical_particle_extended::at()` tratava WIND e RANGE pela mesma função de spawn contínuo `range[i] * random01(...)` (`sources/Engine/include/top/HistoricalParticleExtended.h:85-98,127-129`, antes da mudança). | Apenas tipo 7, quando `TOP_HISTORICAL_PARTICLE_RANDF=1`, chama `deterministicRandf` por eixo, preservando o caminho contínuo quando a opção está ausente/0 (`sources/Engine/include/top/HistoricalParticleExtended.h:85-98`). RANGE (tipo 14) permanece inalterado. |
| O helper DX9 para máximo inteiro/fração está em `MPParticleSys.h:26-37`. | O helper GL `deterministicRandf` deriva draws por seed/lane e delega a `dx9Randf` (`sources/Engine/include/top/HistoricalParticleRandom.h:24-52,67-72`). | Tradução: reproduz a distribuição/quantização `Randf` sobre o stream determinístico por partícula já usado no GL. Não copia `rand()` do CRT do Windows nem a ordem global dos draws. |
| WIND atualiza aceleração e ângulo com `dt`, gira a aceleração em Z e soma à posição inicial (`MPParticleSys.cpp:3402-3415`). | O GL já tem trajetória WIND no controlador estendido (`HistoricalParticleExtended.h:97-105`). | Esta etapa só ajusta a amostragem do nascimento; movimento, orientação e conversão de eixos permanecem como antes. Sem mudança de matriz D3D, blend, culling ou winding. A conversão espacial continua na fronteira de renderização GL existente. |

## Classificação

- **FATO:** o DX9 chama `Randf` para os três eixos no nascimento de WIND e o helper usa a grade de 0,001 em seu ramo fracionário (`MPParticleSys.cpp:3260-3261`; `MPParticleSys.h:26-37`).
- **FATO:** o helper DX9 usa `rand()` global; no GL, `deterministicRandf` usa o gerador por seed/lane (`HistoricalParticleRandom.h:55-72`).
- **INFERÊNCIA:** o mapeamento reproduz a distribuição e a quantização das chamadas de `Randf`, mas não a sequência de posições do DX9, porque não reproduz o estado/ordem do CRT global.
- **DESCONHECIDO:** ordem real dos draws no Client em runtime, aparência comparada quadro a quadro e se todos os dois IDs estáticos de skill executam WIND no jogo.
- **DESCONHECIDO:** equivalência visual com DX9. Nenhuma comparação visual foi feita; WIND segue aproximado.

## Testes e auditoria

A fixture `ui05sk217b_extended_particle_modes` verifica o caminho legado quando o switch está desligado e a quantização/mapeamento determinístico do spawn WIND quando ligado. Build Debug completo e CTest executados nesta árvore: `top_gl` compilado e **139/139 testes aprovados**. A fixture também executou sob ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`).

Auditoria Client em `build/sk217bm-fx-audit/`: 1.031 PAR, 1.227 EFF, 3.476 sistemas PAR, 2.832 partes EFF; 25 lacunas agregadas/307 IDs estáticos com alguma lacuna; zero drift e zero falhas estruturais. Há 31 recusas estritas EFF, todas recuperadas pelo decode diagnóstico; 30 têm referência PAR direta, enquanto `effect/fboomerff.eff` não tem referência direta e tem PAR homônimo catalogado (ID 919/skill 308). O uso em runtime desse caso não está provado; nenhum EFF foi descartado. A classificação e os limites das recusas estão em `fx_eff_usefulness.csv` e `fx_coverage_summary.json`.

## Como ligar, desligar e reverter

A opção reutiliza o switch existente e permanece desligada por padrão:

```bash
TOP_HISTORICAL_PARTICLE_RANDF=1 ./build/top_gl --profile
```

Para desligar, remova a variável ou use `TOP_HISTORICAL_PARTICLE_RANDF=0`. Isso também desliga os mapeamentos Randf históricos anteriores que compartilham esse switch. Para reverter apenas esta etapa depois de aplicar o patch em uma base limpa compatível: `git apply -R SK217BM.patch`.

## Próximos pontos medidos

A cobertura global segue com 25 lacunas agregadas/307 IDs estáticos com lacuna. O ranking ainda reporta SNOW (566 sistemas/110 skills) e BLAST2 (1 sistema/0 skills rastreáveis) como aproximados; essas contagens não substituem inspeção por subfeature e vínculo de skill. Os 31 EFF rejeitados estritamente não são prova de recursos inúteis: 30 são referenciados diretamente, e o restante tem evidência de catálogo, mas runtime desconhecido. O próximo passo deve escolher uma linha/subfeature do ranking atual e comparar código DX9, porte e os recursos envolvidos antes de alterar runtime. Validação visual com o cliente DX9/OpenGL continua pendente.
