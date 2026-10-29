# SK2.17-AR — evidência do catálogo DX9 para falhas EFF

Data: 2026-10-25. Etapa de auditoria somente leitura; não muda o carregamento/renderização do jogo.

## Comparação DX9 × auditoria OpenGL

| Original DX9 | Auditoria/porte OpenGL antes desta etapa | Mudança SK2.17-AR |
|---|---|---|
| `CMPResManger::LoadTotalEffect` enumera `*.eff` com `FindFirstFile/FindNextFile`, chama `LoadEffectFromFile` e indexa cada arquivo em `_mapEffect`: `arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`. | `top_fx_coverage_audit` classificava por arestas PAR→EFF, IDs de catálogo e skill; não dizia se um arquivo sem aresta direta ainda era enumerado pelo loader original (`sources/Engine/include/top/FxCoverageAudit.h:558-634`, lógica de vínculos e relatório de utilidade anterior). | O auditor agora localiza e verifica no corpo de `LoadTotalEffect` o wildcard, enumeração, tentativa de carga e indexação (`sources/Engine/include/top/FxCoverageAudit.h:78-110`). `fx_eff_usefulness.csv` acrescenta `dx9_catalog_load_evidence` por falha estrita (`:633-651`), e o JSON registra as linhas-fonte detectadas (`:666-674`). Não infere execução visível em runtime. |

## Resultado medido no Client

Com `Client/` atual e `arquivo/fontes/Engine/src/MPResManger.cpp`, o audit verificou as quatro operações no original (linhas 1360, 1364, 1388 e 1398). Dos 1.227 `.eff`, 31 falham no modo estrito. As 31 linhas estão sob o diretório `Client/effect/` enumerado pelo loader e agora recebem `enumerated_loaded_indexed`; o modo diagnóstico estrutura as 31. Portanto, cada uma é carregada/indexada pelo DX9. A hipótese de que um `.eff` sem dependência PAR direta seja dispensável não se aplica a esse catálogo.

Trinta das 31 também têm referência direta por PAR/sceneffectinfo. `fboomerff.eff` não tem essa aresta, mas tem relação adicional com `fboomerff.par`, sceneffect 919 e skill 0308 Bomb Throwing. Isso é evidência de associação do recurso PAR irmão, não de que o modelo interno do EFF seja exibido nessa skill. O audit mantém `runtime_use_proven=no` para não confundir carga/indexação com execução visual.

A leitura diagnóstica encontrou 98 parâmetros de modelo base não finitos distribuídos nos 31 arquivos e nenhum parâmetro animado não finito. A classificação não remove dados e não muda o switch existente `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS`. O suporte ao modo estrito continua separado da evidência de carregamento DX9.

## Auditoria atualizada e teste

Saída: `build/sk217ar-eff-catalog-audit/`.

- 1.031 PAR, 1.227 EFF, 3.476 sistemas PAR e 2.832 partes EFF.
- 31 falhas estritas EFF, 31 recuperadas pelo parser diagnóstico, zero falhas gerais.
- Loader DX9 verificado: wildcard na linha 1360, enumeração na 1364, carga na 1388 e índice na 1398.
- 26 lacunas no ranking, 307 skills alcançadas, `drift=0`.
- Fixture sintética cobre EFF inválido sem referência direta na raiz (DX9 enumera/carrega/indexa) e EFF inválido em subdiretório (fora do wildcard de nível superior). Build Debug completo terminou com código 0; CTest completo passou **136/136**; `ui05sk214_fx_coverage_audit` passou. O mesmo fixture compilado com ASan/UBSan e `ASAN_OPTIONS=detect_leaks=0` terminou com código 0.

Comandos de teste executados:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/FxCoverageAuditTests.cpp \
  -o build/sk217ar_fx_coverage_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217ar_fx_coverage_asan
```

Comando do audit usado:

```bash
./build/top_fx_coverage_audit \
  --client Client --out build/sk217ar-eff-catalog-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp \
  --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv
```

**FATOS:** o loader enumera/carrega/indexa os 31 arquivos no diretório-alvo; as 31 foram recuperadas em diagnóstico; 30 têm aresta PAR direta. **INFERÊNCIA:** decodificar os campos estritos das 31 é relevante para poder reproduzir cada recurso carregado, mas não se afirma que todos sejam renderizados. **DESCONHECIDO:** quais EFF foram instanciados/exibidos em cada sessão do Client. Não houve comparação visual DX9/OpenGL.
