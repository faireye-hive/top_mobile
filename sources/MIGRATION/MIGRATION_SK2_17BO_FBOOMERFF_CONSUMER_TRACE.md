# SK2.17-BO — rastreamento estático de consumidores de `fboomerff.eff`

## Decisão

A classificação anterior de `fboomerff.eff` era “uso em runtime desconhecido”. O cruzamento adicional não encontrou consumidor estático conhecido para esse `.eff` no Client atual. O PAR homônimo é ligado à skill 308 como `impact_target`, mas os três sistemas do PAR usam `RectPlane` e texturas (`fire_02`, `smoke`, `poisonshadow`), sem declarar `fboomerff.eff` como modelo. A varredura de código e tabelas (`.cpp/.h/.txt/.bin/.clu/.lua`) encontrou a ocorrência na entrada de `sceneffectinfo.txt`, que aponta para o `.par` irmão; não encontrou o nome como modelo consumido por outra entrada.

**Conclusão limitada:** não é necessário adicionar caminho especial de decoder para `fboomerff.eff` no percurso estático conhecido do GL. O renderer GL carrega PARs e só chama o leitor `.eff` para nomes de modelo `.eff` encontrados nos recursos/tabelas (`sources/Engine/src/top/EffectSystem.cpp:486-500,515-570,579-597`). O arquivo permanece no Client e no inventário; não afirmo que nenhuma chamada dinâmica DX9 possa solicitá-lo. A enumeração/preload global do DX9 continua comprovada, embora isso não prove execução: `arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`.

## Original DX9 × dados do Client × porte GL

| Evidência | Resultado e leitura |
|---|---|
| O DX9 resolve nomes vindos de `CMPPartSys::_strModelName` e chama `GetEffectID` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:292-301`). Outros consumidores recebem nomes das tabelas `CEffectScript::LoadParamFromName`/`EFF_Param` (`MPEffectCtrl.cpp:33-49,245-251`; `MPEffectCtrl.cpp:353-367`). | A busca literal com binários incluídos, limitada a `.cpp/.h/.txt/.bin/.clu/.lua`, encontrou `fboomerff` apenas na linha de catálogo `sceneffectinfo.txt:551`; não encontrou ocorrência no código nem nas tabelas de nomes de modelo. O grafo FX também registra zero arestas PAR→`fboomerff.eff` (`fx_eff_usefulness.csv`). Isso é evidência estática, não captura de chamadas em jogo. |
| `sceneffectinfo.txt:551` associa ID 919 a `fboomerff.par`; `dependencies.csv:491` associa esse PAR à skill estática 308, canal `impact_target`. | `top_par_audit` decodificou `fboomerff.par` v15, 3/3 sistemas. Eles são type 15, modelo `RectPlane`, texturas `fire_02`, `smoke` e `poisonshadow`; o CSV `par_dependencies.csv` não contém referência EFF. O audit encontrou as três texturas. O vínculo PAR→skill não transfere automaticamente uso para o `.eff` homônimo. |
| O DX9 indexa todo arquivo `.eff` enumerado e tenta `LoadEffectFromFile`, mesmo sem vínculo de PAR (`MPResManger.cpp:1359-1405`). | O GL resolve `sceneEffect->particle` como PAR e os modelos definidos pelas tabelas/PARs; não enumera todos os `.eff` do diretório (`EffectSystem.cpp:495-500,515-570,579-597,600-621`). | Tradução de arquitetura: o preload global do DX9 vira carregamento sob demanda no GL. Para este arquivo sem consumidor estático conhecido, o modo GL não chama seu decoder. Isso não altera formatos nem API pública. |

## Fato, inferência e desconhecido

- **FATO:** `fboomerff.eff` tem zero referências PAR diretas no audit; `fboomerff.par` tem três sistemas type 15 que usam `RectPlane` e as três texturas listadas; não há nome literal do `.eff` nos arquivos de código/tabelas abrangidos pela busca.
- **FATO:** a skill 308 aponta para PAR `fboomerff.par` pelo canal `impact_target`; isso está em `dependencies.csv:491`, não é vínculo direto ao `.eff`.
- **FATO:** o DX9 enumera e tenta carregar todo `.eff`; o GL carrega o recurso quando um modelo `.eff` é referenciado no caminho conhecido.
- **INFERÊNCIA:** para o grafo estático atual, `fboomerff.eff` é um órfão de runtime GL e não precisa de suporte específico de decode. Não se deve usar o irmão PAR/skill como prova de dependência.
- **DESCONHECIDO:** se uma chamada DX9 em runtime constrói esse nome por outra fonte ou mecanismo não coberto; comportamento ao tentar carregar esse arquivo no executável DX9; resultado visual.

## Execução e artefatos

- `top_fx_coverage_audit` atual: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes; zero falhas estruturais; saída `build/sk217cn-fx-audit/`.
- `top_par_audit --client Client --out build/sk217co-fboomerff-par-audit`: 1.031/1.031 PAR decodificados, 3.476 sistemas e zero falhas de PAR.
- Os testes `ui05sk23_all_par_audit` e `ui05sk214_fx_coverage_audit` passaram 2/2 nesta etapa; a execução do `top_par_audit` e do `top_fx_coverage_audit` também retornou exit code 0. Esta etapa é somente leitura e não altera runtime.
- Não executei o cliente DX9/OpenGL, capture de `GetEffectID`, nem comparação visual. A ausência de uso runtime absoluto não está provada.

## Encaminhamento

Manter o asset e não adicionar exceção/decoder específico para ele no GL enquanto não surgir consumidor estático/runtime. O modo geral `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1` continua opt-in para os arquivos com dependências diretas. Uma futura captura do DX9 pode confirmar ou refutar chamadas indiretas; sem ela, a classificação é “sem consumidor estático conhecido”, não “nunca usado”.
