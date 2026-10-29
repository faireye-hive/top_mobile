# SK2.17-BL — vínculo de CMPStrip direto com CMagicCtrl

## Resultado

Ampliei o auditor `top_direct_strip_anchor_audit` para cruzar cada PAR com trailer `CMPStrip` contra a coluna de partículas de `MagicSingleinfo`. O cruzamento é gerado da tabela real, sem lista de nomes mantida à mão. Nenhuma mudança de runtime foi feita: não há evidência de que qualquer um dos seis recursos de strip passe pelo ramo de loop `true` do `CMagicCtrl`.

## Original DX9 × porte OpenGL × conclusão

| Original DX9 | Porte OpenGL atual | Conclusão sustentada |
|---|---|---|
| `CEff_ParamSet::_ReadRawDataInfo` lê a quantidade e os nomes dos PARs filhos (`arquivo/fontes/Engine/include/MPEffectCtrl.h:153-179`). `CMagicCtrl::Create` acrescenta `.par`, copia o controlador e, se o primeiro modelo é item, chama `SetStripItem(..., true)` (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:401-420`). `CMPPartCtrl::SetStripItem` encaminha esse valor para `CMPStrip::SetLoop` (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:333-339`). | O caminho de `EffectSystem` de strip direto emite os perfis de jogador e passa `false` a `legacy_direct_strip::append` (`sources/Engine/src/top/EffectSystem.cpp:1676-1701`). A rota DX9 `CEffectObj::SetEffect` chama `SetStripItem` sem argumento de loop, cujo padrão é `false` (`arquivo/fontes/Client/src/EffectObj.cpp:1048-1060`; `arquivo/fontes/Engine/include/MPParticleCtrl.h:331`). | Auditoria no Client encontrou seis trailers em seis PARs, todos catalogados em `sceneffectinfo`, e zero correspondências na coluna de filhos PAR de `MagicSingleinfo`. O `true` existe no original, mas não foi associado a esses seis recursos. Não propagar `true` globalmente. |

## Evidência do Client

Execução de `top_direct_strip_anchor_audit` com os arquivos DX9 e GL acima:

- 1.031 PARs examinados; seis registros diretos CMPStrip em seis arquivos; zero falhas de parsing.
- Os seis estão associados estaticamente a entradas `sceneffectinfo` (IDs 136, 217, 218, 333, 805 e 806).
- `magic_single_catalog_loaded=true` e `magic_single_linked_direct_strip_records=0`.
- O audit observou o callsite `SetStripItem(..., true)` no DX9, nas linhas 313 e 419 de `MPEffectCtrl.cpp`.
- As relações de catálogo/dependência são estáticas; não provam invocação durante uma sessão DX9. O resultado não demonstra que `CMagicCtrl` nunca seja usado para outros PARs.

Arquivos: `build/sk217bl-direct-strip-audit/direct_strip_anchor_summary.json` e `direct_strip_anchor_coverage.csv`.

## `.eff` recusados em modo estrito

Na auditoria Client atual anterior, `fx_decode_failures.csv` tinha apenas o cabeçalho: zero falhas estruturais. O inventário histórico de 31 rejeições do parser estrito correspondia a parâmetros de modelo não finitos; os 31 foram recuperados pelo parser diagnóstico com os parâmetros crus. Trinta têm referência direta em PAR; o 31º, `effect/fboomerff.eff`, tem evidência de catálogo DX9 e um PAR irmão associado estaticamente a uma skill. Portanto, há indícios de utilidade para todos e **nenhum foi classificado como recurso morto**. Ainda não há prova de uso runtime individual. O recurso `fboomerff.eff` não deve ser removido sem captura adicional. Consulte `sources/MIGRATION_SK2_17BG_BLAST_RANDF.md` para o cruzamento e o switch `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS`.

## Fato, inferência e desconhecido

- **FATO:** parser DX9 da tabela, caminho de criação `CMagicCtrl`, valor `true` no setter e caminho GL com `false` estão citados acima. A auditoria atual mediu seis recursos e zero vínculos com `MagicSingleinfo`.
- **INFERÊNCIA:** os seis recursos catalogados em `sceneffectinfo` usam a família de vínculo de efeito de cena para a amostra auditada; isso não comprova execução em runtime.
- **DESCONHECIDO:** uso runtime por asset, owners e ordem de chamadas em sessão real, e paridade visual DX9/OpenGL. Os seis não foram descartados; apenas não há associação estática com a lista de PARs filhos de `CMagicCtrl` no catálogo Client examinado.

## Testes e execução

- Configuração Debug e build completo, incluindo `top_gl`, exit 0.
- CTest focal `ui05sk217v_direct_strip_anchor_audit`: passou.
- CTest completo: 139/139 passou.
- Comparação visual: não realizada.
- Esta etapa só amplia auditoria e fixture; não requer variável `TOP_HISTORICAL_*` e não altera estado de runtime.
