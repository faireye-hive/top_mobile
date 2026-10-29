# SK2.17-Z — correção da classificação do audit de `.eff` filhos STRIP

## Resumo

O audit ainda classificava como lacuna os 875 sistemas STRIP com `.eff`, alegando que quadros, textura e UV permaneciam aproximados. A comparação com DX9 e com o porte mostrou que essas timelines já existem em duas opções experimentais separadas e têm fixtures próprios. Esta etapa corrige a classificação e a nota do audit; não altera o renderer.

## Evidência: DX9 x porte x classificação

| Fato observado | DX9 original | Porte GL | Classificação após Z |
|---|---|---|---|
| Quadro discreto | `CMPModelEff::FrameMove` acumula `dt`, avança no máximo um quadro e zera o residual (`arquivo/fontes/Engine/src/MPModelEff.cpp:514-540`). | O grupo de timelines é avançado para type 6 com `dt` (`sources/Engine/src/top/EffectSystem.cpp:1081-1088`); a opção de quadro é `TOP_HISTORICAL_STRIP_EFF_FRAME_STEP` (`sources/Engine/include/top/HistoricalEffTimeline.h:11-14`). | Recurso implementado em modo opt-in; não entra como lacuna de implementação.
| Controladores UV/textura | DX9 chama `GetLerpCoord`, `GetLerpTexture` ou `GetLerpFrame` segundo tipo de filho (`arquivo/fontes/Engine/src/MPModelEff.cpp:568-589`). | O grupo avança os relógios de coordenada/região/textura (`sources/Engine/include/top/HistoricalEffTimeline.h:86-97`), e o renderer seleciona as amostras no modo de clock (`sources/Engine/src/top/EffectSystem.cpp:2126-2129,2192-2198,2255-2258,2321-2342`). A opção é `TOP_HISTORICAL_STRIP_EFF_UV_CLOCK` (`HistoricalEffTimeline.h:17-20`). | Também implementado atrás de switch; a paridade visual continua sem validação.
| Uso no Client | Os arquivos são subefeitos controlados pelo `CMPModelEff`. | Auditoria Client: 875 sistemas, 500 PARs, 242 skills alcançadas para esta feature. | Mantém-se na tabela de cobertura, mas sai do `fx_gap_ranking.csv`; o tipo STRIP permanece parcial por outros itens, incluindo proprietários de `CMPStrip` direto.

## FATO, inferência e desconhecido

**FATO:** os dois switches e as rotas de timeline/clock estão no porte; os testes R/S e suas integrações estão registrados no CMake. O teste do audit verifica a contagem da feature e que ela não aparece no ranking.

**INFERÊNCIA:** a contagem da feature é uma medida do alcance de dados, não evidência de execução de cada habilidade no jogo.

**DESCONHECIDO:** sem comparação visual com DX9 não se afirma fidelidade de pose, duração observada, UV ou textura em runtime. As opções continuam desligadas por padrão.

## Testes

Build Debug do audit e `ctest --test-dir build --output-on-failure -R ui05sk214_fx_coverage_audit`. Nenhuma alteração de formato, API ou comportamento de jogo nesta etapa.
