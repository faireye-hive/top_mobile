# SK2.17-G — leitura opt-in dos parâmetros brutos de modelo `.eff`

## Resumo

O decoder pode agora consumir os campos brutos `height/topRadius/bottomRadius` da versão 2+ de `I_Effect` sem exigir que sejam finitos. A opção de runtime é `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1`, desligada por padrão; com a variável ausente ou `0`, permanece a validação estrita anterior. Os audits de cobertura/blend usam a leitura permissiva para medir recursos, sem alterar o runtime.

## Comparação: DX9 original x porte OpenGL

| Original DX9 | Porte GL antes de G | Mudança G |
|---|---|---|
| `I_Effect::SaveToFile` serializa `m_nSegments`, `m_rHeight`, `m_rRadius`, `m_rBotRadius` como bytes/float sem validação: `arquivo/fontes/Engine/src/I_Effect.cpp:516-520`. | O decoder lia os três floats com `Reader::f32`, que rejeita `NaN/Inf`: `sources/Engine/include/top/LegacyEffFormat.h:104-109,216-223` (antes da mudança). | `Reader::rawF32` preserva os bits apenas nesses três campos e somente com `allowRawModelParams=true`: `sources/Engine/include/top/LegacyEffFormat.h:110-119,216-224`. A API do inspector mantém modo estrito como default: `sources/Engine/include/top/LegacyEffectInspector.h:34-35,77-79`. |
| `I_Effect::LoadFromFile` lê os parâmetros sem verificar finitude: `arquivo/fontes/Engine/src/I_Effect.cpp:662-669`. Com `_iUseParam<=0`, usa esses valores para criar parâmetros apenas quando `IsCylinderMesh(m_strModelName)`: `arquivo/fontes/Engine/src/I_Effect.cpp:704-712`. | O GL cria Rect/RectPlane sem consultar parâmetros; Cone/Cylinder valida topologia e finitude antes de gerar vértices: `sources/Engine/include/top/LegacyEffectGeometry.h:47-65`. | O runtime só seleciona leitura bruta quando `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1`: `sources/Engine/src/top/EffectSystem.cpp:65-68,93-96,491-493,614-616`. A validação GL de Cone/Cylinder continua intacta. |
| `CMPResManger::LoadTotalEffect` enumera arquivos `*.eff`, chama o loader e indexa pelo nome: `arquivo/fontes/Engine/src/MPResManger.cpp:1351-1365,1383-1405`. | A leitura estruturada servia aos recursos aninhados, mas arquivos que falhavam voltavam ao fallback conservador de nomes ASCII. | O opt-in recupera campos estruturais desses `.eff` sem aceitar `NaN/Inf` em keyframes, UVs, rotações, tempos ou demais floats do documento. |

Essa é uma tradução do formato binário, não uma cópia do estado D3D9. Não há estado de blend, texture stage, matriz ou convenção de winding alterado nesta etapa.

## Auditoria dos arquivos antes recusados

Contra o `Client/` presente neste workspace, o audit atual encontrou 1.031 `.par` válidos e 1.227 `.eff`. Antes de G, 31 `.eff` versão 7 estavam em `fx_decode_failures.csv`. Todos os 31 passaram a consumir sua estrutura completa na leitura diagnóstica que relaxa apenas a finitude dos parâmetros de modelo; a implementação G manteve a finitude dos outros campos e passou pelos fixtures correspondentes.

- **30/31 têm ligação direta em dados:** 32 registros `model_eff` em `.par` válidos referenciam 30 nomes distintos; todos têm status `found` em `par_dependencies.csv`. O arquivo `jbg05.eff` é referenciado por três `.par`.
- Os `.par` da família `jbbg/jbg/jbrg/jbyg/jj*` aparecem em `Client/scripts/table/sceneffectinfo.txt`; por exemplo, `jbbg01.par` na linha 890, `jjbg01.par` na linha 894 e `jbg01.par` na linha 902. Isso prova vínculo nos dados do Client, não prova que uma skill foi executada em jogo.
- **`fboomerff.eff` é inconclusivo quanto a uso visível:** não há aresta `model_eff` para ele nos `.par` válidos nem referência textual exata nos scripts examinados. `fboomerff.par`, por outro lado, é listado em `sceneffectinfo.txt:551`, e seus três sistemas usam `RectPlane`, não `fboomerff.eff` (`build/sk217g-par-audit-updated/par_dependencies.csv:1766-1771`). O loader DX9, contudo, carrega todos os `.eff` da pasta e os indexa pelo nome; sem rastrear toda chamada dinâmica a esse índice, não é correto classificá-lo como definitivamente sem uso.
- Como os 31 arquivos têm o mesmo formato v7 e o mesmo problema de parâmetros brutos, o decoder continua genérico. Não foi criado fallback de geometria nem tratamento especial por nome de asset.

## Fato, inferência e desconhecido

**FATO:** o código DX9 serializa e lê os parâmetros sem teste de finitude. O diagnóstico em cópia temporária mostrou que relaxar apenas os floats de modelo permite estruturar os 31 documentos sem bytes finais; relaxar apenas flags não permite. 30 arquivos têm referências `model_eff` em `.par` válidos. O runtime GL valida os parâmetros que alimentam Cone/Cylinder antes de gerar geometria.

**INFERÊNCIA:** os parâmetros não finitos encontrados em partes `Rect`, `RectPlane` e modelos externos são campos não consumidos pela geometria GL equivalente, pois os ramos correspondentes não leem `ModelParams`. O comportamento resultante precisa de inspeção visual; não se declara equivalência.

**DESCONHECIDO:** se cada `.par` ligado é executado em uma sessão real e se `fboomerff.eff` é solicitado dinamicamente pelo índice DX9. A estrutura agora é decodificada, mas as geometrias `.lgo` e a fidelidade visual dos efeitos continuam tarefas separadas.

## Testes e comandos

Fixture/regressão em `sources/tests/UI05ITests.cpp`: modo estrito continua desligado por padrão; a leitura bruta opt-in preserva `NaN/Inf` dos parâmetros de modelo; `NaN` em tempo de keyframe e arquivo truncado continuam recusados como estrutura completa. Nenhum teste anterior foi enfraquecido.

```bash
cmake -S sources -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/fx-coverage
build/top_blend_audit --client Client --out build/blend-audit
```

Para comparar no jogo, mantenha as variáveis previamente usadas e acrescente:

```bash
TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_RANGE2_COLLISION=1 \
TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT=1 \
./build/top_gl --profile
```

Desligue com `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=0` ou remova a variável. Para reverter este patch depois de aplicá-lo: `git apply -R SK217G.patch`. A opção permanece experimental até comparação visual do usuário com DX9.

## Verificações executadas

- Configuração/build em Debug: `top_gl` e os alvos de teste compilaram sem `NDEBUG`.
- Teste focal `top_ui05i_tests`: passou após a fixture distinguir parâmetros brutos de keyframes inválidos.
- `top_fx_coverage_audit` no Client: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 19 gaps, 307 skills com gaps, zero falhas de decodificação e drift zero. Os 193 registros de partes adicionais são dados decodificados, não evidência visual.
- `top_blend_audit` no Client: 6.308 registros, 6.308 aplicados, zero fallback, zero inválidos e zero falhas de decode.
- Build completo Debug, incluindo `top_gl`: sucesso.
- CTest completo: **104/104 passaram**. Nenhum teste existente foi removido ou enfraquecido.
- `top_ui05i_tests` também passou em build separado com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`).
- Não houve comparação visual com DX9.
