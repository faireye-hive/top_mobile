# SK2.17-BA — controladores de efeitos colocados no mapa

## Escopo e resultado

Esta etapa conecta os registros tipo 1 do `.obj` aos controladores catalogados em `sceneffectinfo.txt`. A opção `TOP_HISTORICAL_MAP_SCENE_EFFECTS=1` está desligada por padrão. Ela coleta apenas colocações dentro do raio já usado para objetos de cena, instancia o `.par` quando o ID existe no catálogo e mantém a instância durante o streaming da área. Uma colocação que sai da área é removida; uma troca de mapa limpa as instâncias ao reabrir `EffectSystem`.

O caminho traduz posição/altura para o mundo GL X/Y/Z e yaw DX9 para rotação ao redor do eixo vertical GL Y. O ciclo de emissão usa os dados do `.par`, sem emitir partículas substitutas. A opção força o ciclo persistente exigido pelo `SetPlayType(0)` para os controladores de cena, mas os modelos/partículas continuam usando os decoders, geometrias e traduções OpenGL já existentes.

## Comparação: original DX9 x porte atual x mudança

| Original DX9 | Porte GL antes desta etapa | Mudança BA |
|---|---|---|
| O `.obj` guarda tipo/ID nos 2 bits superiores; posição é lida em centicentímetros (`arquivo/fontes/Client/src/SceneObjFile.h:15-31`; `SceneObjFile.cpp:477-493`). Para tipo 1, o cliente cria `AddSceneEffect(id)` e aplica altura, posição e yaw (`arquivo/fontes/Client/src/GameAppMsg.cpp:128-136`). | `SceneWorld::build` descartava todo `kind != 0` (`sources/Engine/src/top/SceneWorld.cpp`, ramo anterior à mudança no loop de registros). | `SceneWorld` agora coleta tipo 1 sem exigir entrada em `sceneobjinfo.txt`, converte x/y e altura por 100 e guarda yaw como `sYawAngle/100` radianos (`sources/Engine/src/top/SceneWorld.cpp`, ramo `kind == 1`). O key é seção + índice local para conciliar streaming. |
| `AddSceneEffect` cria o efeito, chama `Emission(0, origin, nullptr)` e o valida (`arquivo/fontes/Client/src/SceneCreateNode.cpp:325-338`). A emissão normal usa `Play(0)` e `SetPlayType(0)` (`arquivo/fontes/Client/src/EffectObj.cpp:1334-1351`). | O GL tinha catálogo de `sceneffectinfo.txt`, mas nenhum vínculo do `.obj` com instâncias persistentes de cena. | `EffectSystem::setMapSceneEffects` resolve o ID diretamente em `SceneEffectCatalog`, carrega o PAR nomeado e registra origem absoluta, yaw e chave de colocação (`sources/Engine/src/top/EffectSystem.cpp`, método novo). Isso evita misturar o ID de cena com o namespace de `magicsingleinfo`. |
| `SetPlayType(0)` zera o play time dos sistemas, ativa `SetLoop(true)` e usa `PLAY_LOOP` nos modelos (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:392-405`). Os efeitos válidos recebem `FrameMove` por tick (`arquivo/fontes/Client/src/SceneFrameMove.cpp:119-130`). | Instâncias emitidas pelo GL eram temporárias e expiravam conforme a duração estimada do efeito. | Instâncias de mapa não expiram por duração de skill. Avançam junto com `EffectSystem::step`, preservam o ciclo de controladores conhecido e são retiradas quando deixam o conjunto de colocações visíveis ou quando o mapa é reaberto (`sources/Engine/src/top/EffectSystem.cpp`; chamada de sync em `sources/Platform/linux/main.cpp`). |
| O yaw aplicado pelo objeto é `_nYaw/100` radianos (`arquivo/fontes/Client/src/EffectObj.cpp:1399-1402`). O sistema de partículas armazena o yaw no eixo Z do mundo MindPower (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1582-1592`). | A cena GL usa X/Z no plano horizontal e Y como altura. | A conversão é eixo DX9 Z-up → eixo GL Y-up: yaw gira deslocamentos locais no plano GL X/Z e alimenta o transform dos modelos filhos pelo eixo vertical GL. A winding, blending D3D e texture stages não foram alterados nesta etapa. |

## Auditoria de `.eff` recusados

Auditoria reexecutada em `Client/` com `top_fx_coverage_audit`: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, zero falhas estruturais, 31 falhas apenas no modo estrito e zero drift.

- 30 dos 31 arquivos recusados no modo estrito têm uma ou mais referências diretas de controladores PAR; em conjunto, 30 nomes EFF e 32 referências. Não são dispensáveis com a evidência disponível.
- `effect/fboomerff.eff` é o único sem aresta direta PAR→EFF. O catálogo contém `fboomerff.par` como ID 919, associado à skill 308 (“Bomb Throwing”); seus sistemas PAR decodificados não referenciam o EFF irmão. O relatório marca `runtime_use_proven=no`.
- O loader DX9 enumera e carrega os `.eff` da pasta principal (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Isso prova indexação no catálogo, não execução/apresentação de cada arquivo. Por isso `fboomerff.eff` permanece **uso runtime desconhecido**, não classificado como inútil.
- As 31 recusas são recuperadas pela leitura diagnóstica que permite somente os parâmetros brutos de modelo. A build OpenGL já pode usar esses campos atrás de `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1`; a auditoria também mantém os 31 recuperáveis. Não foi criado tratamento por nome de arquivo.

## Fatos, inferências e desconhecidos

**FATO:** o audit de mapas contabiliza 47 `.obj` válidos, 10.770 colocações tipo 1 e 21 IDs. Há 10.760 colocações catalogadas; IDs 21 (6 colocações) e 22 (4) não têm definição explícita em `sceneffectinfo.txt`. O audit de controladores encontrou 19 PARs catalogados, 33 sistemas nos tipos 1, 2, 3, 4 e 6; tipo 6 aparece em 7.117 colocações. Os resultados estão registrados em `build/sk217ay-scene-effect-map-audit/` e `build/sk217az-scene-effect-controller-audit/`.

**INFERÊNCIA:** `SetPlayType(0)` torna esses efeitos persistentes enquanto o controlador reportar reprodução ativa; o porte usa a chave da colocação e o streaming local para não manter simultaneamente os 10.760 registros na memória/renderização.

**DESCONHECIDO:** a equivalência visual do yaw, escala, posição de pivô, momento exato de início de cada seção e todos os ciclos de tipos não cobertos pelos controladores atuais. Não houve comparação visual DX9/OpenGL. IDs sem catálogo permanecem invisíveis. Este recurso não prova uso runtime de cada `.eff` nem descarta `fboomerff.eff`.

## Testes e execução

Execução concluída nesta árvore: configuração e build Debug completos incluindo `top_gl`; CTest **138/138 aprovados** (0 falhas). O alvo de integração SK2.17-BA e o fixture UI05X3 também passaram com ASan/UBSan: **2/2**, `ASAN_OPTIONS=detect_leaks=0` e `UBSAN_OPTIONS=halt_on_error=1`. Isso verifica a lógica automatizada, não a aparência no cliente. Não executei comparação visual com o DX9.


Fixtures sintéticas verificam: opção desligada por padrão; decodificação de ID/posição/altura/yaw; exclusão do passe animado; ciclo de controlador de mapa além da vida individual de uma partícula; remoção ao sair do conjunto ativo. A integração roda sem contexto GL e não comprova aparência visual.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Ative para a comparação no cliente:

```bash
TOP_HISTORICAL_MAP_SCENE_EFFECTS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
./build/top_gl --profile
```

Desative removendo `TOP_HISTORICAL_MAP_SCENE_EFFECTS` ou definindo `=0`. Para reverter o patch incremental: `git apply -R SK217BA.patch`. O efeito permanece experimental até comparação visual do usuário.
