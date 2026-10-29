# SK2.17-BG — mapeamento Randf do BLAST

## Lacuna medida e evidência

O FX coverage audit atual lista `type:3` em 423 sistemas, 118 PARs e 101 IDs de skill estáticos. A lacuna residual do ranking cita o fluxo de RNG como aproximação; esse vínculo estático não comprova execução em runtime. Reexecutei `build/top_fx_coverage_audit --client Client --out build/sk217bg-fx-audit`: 1.031 PARs, 1.227 EFFs, 3.476 sistemas, 2.832 partes, 25 lacunas/307 skill IDs, drift 0 e falhas estruturais 0; o topo continua `type:6` (1.041 instâncias/242 skills), e BLAST `type:3` permanece com 423/101.

| Original DX9 | Porte OpenGL antes de BG | Mudança BG |
|---|---|---|
| `_CreateBlast` gera cada componente de velocidade com `Randf(_fVecl)`, aplica `Rand(2)` nos sinais horizontais, sorteia posição por `Randf(_fRange[i])` e vida por `Randf(_fLife/_iRoadom,_fLife)` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3844-3877`; helpers em `MPParticleSys.h:6-44`). | `historical_particle_motion::at` usava amostras float contínuas para range, magnitude e sinais de BLAST (`sources/Engine/include/top/HistoricalParticleMotion.h`, caminho type 3). O pool calculava vida contínua por slot (`HistoricalParticlePool.h:138-147`). | Quando `TOP_HISTORICAL_PARTICLE_RANDF=1`, posição, magnitude e sinais de BLAST usam o mapeamento `Randf`/módulo por canal determinístico; o helper de vida usa o mesmo mapeamento e divisor autorado. Sem a variável, mantém o cálculo anterior. |

### Tradução feita

`Randf(float)` do DX9 chama `rand()%N` e representa a fração em passos de 0,001; `Rand(2)` vira `deterministicRandModulo(2)`. Isso traduz o domínio e a quantização para um fluxo determinístico próprio do porte, sem copiar D3D nem alegar reproduzir o estado global do CRT. O mapeamento de coordenadas e o passo de BLAST não foram alterados nesta etapa.

- **FATO:** os callsites e a fórmula do helper estão no DX9 nos trechos citados.
- **FATO:** antes, o ramo GL type 3 multiplicava `speed`/`range` por amostras contínuas e escolhia sinal por comparação, como mostram os caminhos type 3 de `HistoricalParticleMotion.h` e `HistoricalParticlePool.h`.
- **INFERÊNCIA:** usar o mesmo mapeamento discreto tende a representar melhor os valores autorados do que o float contínuo.
- **DESCONHECIDO:** sequência exata de `rand()` por sessão, pois vários componentes do Client/DX9 reseedam e consomem o estado global; cadência/resultado visual seguem sem comparação DX9/OpenGL.

## Switch e reversão

`TOP_HISTORICAL_PARTICLE_RANDF=1` fica desligado por padrão. Remova a variável ou defina `0` e reinicie para voltar à amostragem anterior. Para ativar o restante do controlador de partículas, mantenha os switches já usados pelo perfil e acrescente:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_PARTICLE_RANDF=1 \
./build/top_gl --profile
```

Para comparar o caminho anterior, repita o mesmo comando com apenas `TOP_HISTORICAL_PARTICLE_RANDF=0`. Reinicie entre execuções e compare os mesmos BLAST, local, orientação e FPS no Client DX9 e no OpenGL.

Reversão exclusiva desta etapa: `git apply -R SK217BG.patch`.

## Verificação dos `.eff` estritos

A mesma reauditoria encontrou 31 recusas strict, todas `decoded_with_raw_model_params`; `fx_decode_failures.csv` contém somente o cabeçalho (zero falhas estruturais). Trinta têm referência direta a PAR. Para testar os EFF que falham no modo estrito, a comparação visual deve incluir `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1`; esse switch já existe no porte e mantém os bits crus dos três parâmetros de modelo, que só alimentam geometrias que os consomem. `effect/fboomerff.eff` não tem referência direta, mas o DX9 o enumera/carrega/indexa e há `fboomerff.par` de catálogo associado à skill 308; `runtime_use_proven=no`. Assim, nenhum caso foi descartado como inútil. A auditoria completa fica em `build/sk217bg-fx-audit/fx_eff_usefulness.csv`.

## Testes e limitações

A fixture `SK217AParticleRandomTests.cpp` verifica a vida BLAST entre 0,5 e 0,999 para vida autorada 2/divisor 4 e que as amostras de posição/velocidade ficam na grade de 0,001, atrás do switch. O gate continua desligado por padrão. Configuração Debug e build completo incluindo `top_gl` passaram; CTest completo passou (138/138), e o teste focal passou (1/1). O audit FX atual terminou com código 0; o fixture do scanner RNG passou (1/1). O helper é matematicamente testado; nenhuma comparação visual foi feita e a sequência global DX9 não é reproduzida. ASan/UBSan focal passou com `ASAN_OPTIONS=detect_leaks=0` e `UBSAN_OPTIONS=halt_on_error=1`; o binário em `/tmp` não pôde executar por `Permission denied`, então foi compilado e executado sob `build/`.
