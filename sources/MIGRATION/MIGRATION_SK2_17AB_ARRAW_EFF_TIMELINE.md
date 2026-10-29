# SK2.17-AB — relógio histórico do ARRAW e do `.eff` filho

## Resultado

`TOP_HISTORICAL_ARRAW_TIMELINE=1` ativa o avanço discreto por chamada de `EffectSystem::step(dt)` para as curvas do controlador ARRAW e para o `CMPModelEff` filho. A opção também exige `TOP_HISTORICAL_PARTICLE_EXTENDED=1`; ambas ficam desligadas por padrão. Os demais tipos não mudam.

O audit Client desta etapa mediu **8 sistemas ARRAW em 8 `.par`**, todos com um modelo `.eff` nomeado. As 8 referências alcançam **0 skills** no grafo estático disponível: isso prova presença nos dados, não uso em runtime. A nova feature saiu do ranking de lacunas após o código ser detectado; o tipo continua `approximate` por falta de validação visual e pela cadência real do cliente ainda não estar comparada.

## Original DX9 x porte OpenGL

| Original DX9 | Porte GL antes de AB | Mudança AB |
|---|---|---|
| O enum define `PARTTICLE_ARRAW=8`, e `SetOpertion()` escolhe `_FrameMoveArraw` (`arquivo/fontes/Engine/include/MPParticleSys.h:193`; `arquivo/fontes/Engine/src/MPParticleSys.cpp:388-390`). `_CreateArraw` força um slot, copia a vida e reinicia o quadro (`MPParticleSys.cpp:3007-3021`). | O tipo 8 usava idade analítica para amostrar as curvas e para chamar `legacy_eff::framePhase(part, modelAge)` (`sources/Engine/src/top/EffectSystem.cpp:1862-1875,2190-2206`). | Com os dois switches ligados, o porte avança o estado de curva uma vez por `step(dt)` (`EffectSystem.cpp:1102-1111,1863-1875`) e usa um `historical_eff_timeline::Group` próprio para o filho (`EffectSystem.cpp:1074-1075,2153-2169`). O estado do filho não é compartilhado com RIPPLE ou STRIP. |
| `_FrameMoveArraw` coloca o único slot no centro de `range`, chama `GetCurFrame` uma vez e, ao terminar, reinicia e retorna antes de recalcular tamanho/cor (`MPParticleSys.cpp:3023-3051`). | A idade analítica podia atravessar vários quadros entre atualizações. | O timeline ARRAW reutiliza o relógio discreto existente de `HistoricalModelTimeline`; a interpolação dos quadros continua feita pelo avaliador de curvas do porte. O caso de grande `dt` avança no máximo um quadro e descarta o residual, como `GetCurFrame` (`HistoricalModelTimeline.h:24-47`). |
| Para o filho `.eff`, `_FrameMoveArraw` chama `CMPModelEff::FrameMove(dwDailTime)` uma vez por atualização quando o recurso é um efeito (`MPParticleSys.cpp:3065-3070`). O `CMPPartSys::Create` liga um modelo resolvido pelo índice de efeitos a um `CMPModelEff` (`MPParticleSys.cpp:614-628`). | O porte já carregava e desenhava `profile.modelTracks`, mas usava `legacy_eff::framePhase` por idade total (`EffectSystem.cpp:2140-2206`). | O grupo de quadros/relógios avança por `activeDelta`, que exclui o atraso antes do início do sistema (`EffectSystem.cpp:1081-1085,1102-1111`). O filho continua sendo convertido em geometria OpenGL pelo caminho `legacy_mesh::emit`; não há cópia de matriz ou estado D3D. |
| `Play(iTime)` define `_bLoop=(iTime==0)` e passa `!_bLoop` ao filho (`MPParticleSys.cpp:964-979`). `MPEffectCtrl` chama `CMPPartCtrl::Play(0)` no caminho examinado (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:545-548`). | O filho do tipo 8 era sempre avaliado pela política analítica genérica de ciclo (`LegacyStripSystem.h:16-22`). | O grupo histórico reinicia depois que seus subefeitos terminam, seguindo o caminho `Play(0)` observado. **Inferência limitada:** não foi provado que todo caminho externo de execução ARRAW também passe `iTime=0`; não há link direto dos oito recursos a skills no CSV de dependências. |
| A renderização ARRAW do filho atualiza tamanho/cor, mas não calcula uma rotação do pai (`MPParticleSys.cpp:3047-3070`). | O GL podia aplicar as curvas de rotação do pai também ao filho aninhado (`EffectSystem.cpp`, caminho comum de `ParentTransform`). | Somente no novo modo opt-in, o pai ARRAW não aplica uma rotação que o ramo DX9 não calcula (`EffectSystem.cpp:2207-2215`). A transformação final continua sendo geometria/vertices GL, com a conversão de eixos já existente. |

### Tradução DX9 → OpenGL

Esta etapa não copia `D3DXMATRIX`, `D3DRS`, texture stage nem estados de blend. O controlador temporal apenas escolhe a amostra dos dados `.par/.eff`; o renderer continua convertendo a transformação do filho em vértices OpenGL. Não houve mudança de winding, culling ou regra de blend.

## Auditoria dos `.eff` que falharam anteriormente

**FATO:** a execução atual do `top_fx_coverage_audit` leu 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 subefeitos, com `drift=0`, `failures=0`. `build/sk217ab-fx-audit/fx_decode_failures.csv` contém somente o cabeçalho; nenhum `.eff` falhou nesta execução.

O relatório SK2.17-G registrou 31 arquivos que falhavam na leitura estrita antes daquela etapa. A leitura diagnóstica permissiva, limitada aos três parâmetros brutos de modelo, estruturou os 31; **30/31 tinham 32 referências diretas em `.par` válidos**. `fboomerff.eff` não tinha referência direta localizada, mas o DX9 também pré-carrega/indexa todos os `.eff` enumerados pelo loader (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Portanto, não foi provado que `fboomerff.eff` seja inútil.

O audit atual chama `inspectLegacyEff(..., true)` ao contar EFFs (`sources/Engine/include/top/FxCoverageAudit.h:254-256`); isso explica `failures=0`, mas **não** prova que o runtime padrão aceite todos os três floats não finitos. O runtime mantém a leitura bruta atrás de `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1` (`sources/Engine/src/top/EffectSystem.cpp:71,101,498-499`), desligada por padrão. A mudança G encontrou 30 arquivos com referências `.par`; não há evidência para removê-los como inúteis. O uso dinâmico do arquivo sem aresta direta continua desconhecido.

**INFERÊNCIA:** os 30 recursos ligados a `.par` podem ser solicitados quando o sistema correspondente é executado; o vínculo estático não prova execução real. **DESCONHECIDO:** se `fboomerff.eff` é solicitado por nome em runtime fora dos `.par` inspecionados. Não classifiquei nenhum recurso como “não usado” sem evidência.

## Testes e execução

Fixture de integração cria um `.par` ARRAW com filho `.eff` sintético de três quadros. Em `dt=0.29`, compara o centroide da geometria entre caminho analítico e timeline discreto. A regressão FX inclui um `.eff` corrompido referenciado para verificar que o audit ainda registra a falha; remove o arquivo antes do subteste estrito anterior. Nenhum teste antigo foi removido ou enfraquecido.

```bash
cmake -S sources -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217ab-fx-audit
```

Para a comparação visual, acrescente à configuração histórica já usada:

```bash
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_ARRAW_TIMELINE=1 \
./build/top_gl --profile
```

Para voltar ao caminho atual, remova `TOP_HISTORICAL_ARRAW_TIMELINE` ou defina-o como `0`; deixe o outro switch conforme sua comparação. Depois de aplicar este patch numa cópia da base correspondente, reverta-o com `git apply -R SK217AB.patch`.

## Verificações, limitações e honestidade

- Build Debug completo executado, incluindo `top_gl`: sucesso.
- CTest completo: **128/128 passaram**, com asserts ativos.
- Audit Client executado após a mudança: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 23 lacunas, 307 skills com lacunas, `drift=0`, `failures=0`.
- Não foi feita comparação visual com DirectX 9. ARRAW continua aproximado até essa revisão; o grafo de dependências não associa os oito recursos a skills.
- Os testes focal de integração ARRAW e do FX audit passaram também sob ASan/UBSan com `ASAN_OPTIONS=detect_leaks=0`; LSan não foi executado.
- `SK217AB.patch` passou `git apply --check`, aplicação e reverse-check em uma árvore temporária montada com as versões pré-etapa dos arquivos alterados. Não foi usado um ZIP limpo nesta verificação.

Checklist visual: comparar o mesmo ARRAW no DX9 e GL, na mesma cena/tempo, com switches desligados e depois ligados; observar centro, tamanho, alfa, orientação do filho e transições de quadro. Registrar separadamente se o pai reinicia e se o `.eff` filho repete.
