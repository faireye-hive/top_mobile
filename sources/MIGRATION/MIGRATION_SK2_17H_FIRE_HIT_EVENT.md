# SK2.17-H — FIRE path-end hit event

## Escopo

Recuperar o evento `hitEffect` dos sistemas FIRE que percorrem um caminho e possuem mapa carregado. O comportamento fica desligado por padrão e depende também do pool histórico já existente:

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_FIRE_HIT_EVENT=1 \
./build/top_gl --profile
```

Desligar com `TOP_HISTORICAL_FIRE_HIT_EVENT=0`. Reverter o patch desta etapa com `git apply -R SK217H.patch`.

## DX9 original x porte atual x mudança

| Original DX9 (fato) | Porte antes desta etapa (fato) | Mudança desta etapa |
|---|---|---|
| `CMPPartSys::FrameMove` avança `CEffPath` antes de `FrameUpdate` e atualiza `_vPos` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1243-1256`). | `EffectSystem.cpp` já calcula `pathPosition(profile, pathAge)` para posicionar sistemas, mas só RANGE2 consumia `hitEffect` (`sources/Engine/src/top/EffectSystem.cpp:1200-1223`). | FIRE usa o comprimento acumulado dos segmentos dividido por `pathVelocity` para identificar o primeiro fechamento do caminho (`sources/Engine/include/top/HistoricalFireHit.h:21-44`). |
| `CEffPath::FrameMove` percorre segmentos e volta ao índice zero quando termina; `IsEnd()` sinaliza esse fechamento (`arquivo/fontes/Engine/include/MPModelEff.h:220-231,245-248`). | Não havia evento de impacto FIRE no `EffectSystem`; seu `hitEffect` era lido do `.par`, mas ignorado para type 2. | Ao fechar o primeiro ciclo, a instância cria o `.par` apontado por `hitEffect`, fixa a posição no terreno e registra o evento uma só vez (`sources/Engine/src/top/EffectSystem.cpp:1247-1306`). |
| `_FrameMoveFire` exige emissor ativo e `m_strHitEff`; com mapa e caminho, ao `IsEnd()` amostra `GetGridHeight((int)x*2,(int)y*2)`, troca altura negativa por `0.1`, chama `Stop()` e envia a mensagem (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3531-3550`). | O pool histórico respeitava apenas `playTime`; sem esse valor as emissões poderiam continuar após o evento (`sources/Engine/include/top/HistoricalParticlePool.h:42-75`). | O instante do primeiro ciclo também passa como limite de emissão ao pool. Partículas já nascidas continuam até o fim da vida, como no `Stop()` de SNOW/FIRE (`sources/Engine/src/top/EffectSystem.cpp:1267-1281,1780-1795`; original `MPParticleSys.cpp:1121-1137`). O porte consulta somente o nome autorado e não cria substituto. |
| Com mapa mas sem caminho, o original apenas chama `Stop()` e não envia `hitEffect` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3551-3554`). | — | Mantido: sem caminho não há evento sintético. |

## Evidência dos dados e dos `.eff`

O audit de PAR gerado a partir de `Client/effect` decodificou 15 sistemas no `01010002.par`. Cinco são FIRE com caminho, 100 pontos, `pathVelocity=15`, `play=0` e `hitEffect=fire(a).par`. As referências de Client associam `01010002.par` aos canais Icy Arrow e Magma Bullet (`sources/auditoria-historica/dependencies.csv:19,206`; catálogo `Client/scripts/table/sceneffectinfo.txt:35`). O arquivo/controlador `fire(a).par` não aparece entre os 1.031 `.par` decodificados nem no diretório `Client/effect`; `CMPResManger::SendResMessage` retorna sem criar instância quando `GetPartCtrlID` não encontra o nome (`arquivo/fontes/Engine/include/MPResManger.h:367-375`; `arquivo/fontes/Engine/src/MPResManger.cpp:2065-2099`). Assim, no Client examinado esta etapa pode reproduzir o `Stop()` de emissão no fim do caminho, mas não produz um impacto visível: o nome autorado continua sem recurso resolvível. Não foi criado efeito substituto.

O comportamento do terreno é traduzido para o espaço GL: DX9 usa X/Y como plano e Z como altura; o porte usa X/Z como plano e Y como altura. A consulta direta `MapFile::range2GroundHeightAt(int(x)*2,int(z)*2)` corresponde ao `GetGridHeight` subgrid registrado em `sources/Engine/include/top/MapFile.h:48-53`. Não se espelha D3D no renderer; somente o evento e a coordenada são adaptados.

A revisão anterior dos `.eff` que falhavam não os descartou por ausência de referências: após permitir leitura opt-in dos três parâmetros brutos de modelo, o audit atual registra 1.227 `.eff`, 2.832 partes e zero falhas. Dos 31 arquivos recusados antes, 30 tinham 32 referências `model_eff` em `.par` decodificados. `fboomerff.eff` não tinha ligação direta encontrada, mas o DX9 enumera e indexa todos os `.eff` em `CMPResManger::LoadTotalEffect` (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1404`); uso em runtime continua inconclusivo. Portanto, nenhum deles foi classificado como dispensável só por não aparecer numa referência direta.

## Limites

- A regra implementada cobre o ramo DX9 com mapa, caminho e `hitEffect`, que é o ramo comprovado pelos cinco sistemas do Client. Para este Client o recurso de impacto está ausente, então só a parada de emissão pode ser exercitada.
- O ramo sem mapa do DX9 testa limites do emissor e envia `*_pcPath->GetEnd()` mesmo sem proteger `_pcPath` contra nulo (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3557-3563`). Não foi reproduzido sem um recurso/runtime que demonstre esse caso.
- A simulação GL observa o primeiro ciclo por relógio derivado da distância e da velocidade do caminho; diferenças de quadro entre 60 Hz e o `FrameMove` DX9 ainda precisam de captura visual.
- Não houve comparação visual com DX9; esta etapa não declara fidelidade visual.

## Testes executados

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK217HFireHitTests.cpp \
  -o build/sk217h_fire_hit_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217h_fire_hit_asan
```

Resultado observado: `top_gl` compilou, CTest passou **105/105**, e o teste focal passou sob ASan/UBSan. O LSan foi desativado no teste focal. Não foi executada validação visual no Client.

Checklist visual: comparar Icy Arrow e Magma Bullet no DX9 e GL, com pool/evento ligados; observar ponto de término do caminho, altura do impacto, uma única ocorrência por sistema e parada de novas emissões. Repetir com `TOP_HISTORICAL_FIRE_HIT_EVENT=0` como controle. Só habilitar por padrão após confirmação visual.
