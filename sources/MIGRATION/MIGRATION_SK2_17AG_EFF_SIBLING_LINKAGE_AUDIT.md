# SK2.17-AG — evidência separada para `.eff` e PAR de mesmo nome

## Resultado

O audit `top_fx_coverage_audit` agora registra, em colunas próprias, arquivos `.par` com o mesmo nome-base de cada `.eff` recusado pelo parser estrito, além dos IDs de cena e skills ligados àquele PAR. A relação de nomes iguais fica separada das referências diretas `.par -> .eff`; o audit não infere que o EFF é usado porque um PAR irmão aparece em `sceneffectinfo.txt`.

Execução no Client presente: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas PAR, 2.832 partes EFF, 24 lacunas, zero deriva e zero falhas de estrutura no modo diagnóstico. São **31 recusas estritas**, mas todas as 31 estruturas são recuperadas pelo modo diagnóstico. O arquivo `fx_decode_failures.csv` continua sem falhas estruturais.

## Evidência: DX9 original x OpenGL/audit x mudança

| Original DX9 | Porte/auditor atual | Mudança desta etapa |
|---|---|---|
| `CMPResManger::LoadTotalEffect` enumera todo `*.eff`, chama `LoadEffectFromFile` e indexa cada arquivo pelo nome (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). A rotina não decide se o efeito será exibido. | O porte carrega recursos conforme os vínculos percorridos pelo sistema; a auditoria anterior cruzava links diretos PAR→EFF, IDs de cena desses PARs e habilidades (`sources/Engine/include/top/FxCoverageAudit.h:469-499`). | `fx_eff_usefulness.csv` adiciona `same_basename_par_files`, `same_basename_par_sceneffectinfo_ids` e `same_basename_par_skill_ids`, separados dos vínculos diretos (`sources/Engine/include/top/FxCoverageAudit.h:415-421,500-529`). |
| O loader DX9 enumera arquivos do diretório sem exigir uma referência `.par -> .eff`. | O inventário de assets contém `Client/effect/fboomerff.par` e `.eff`; a linha de cena 919 e o grafo ligam **o PAR** à skill 308 (`Client/scripts/table/sceneffectinfo.txt:551`; `sources/auditoria-historica/dependencies.csv:491`). Os três sistemas de `fboomerff.par` usam `RectPlane`, como medido no `build/sk217ag-par-audit/par_systems.csv:884-886`; não há link direto desse PAR para `fboomerff.eff`. | A nova linha do Client para `fboomerff.eff` registra o PAR irmão, ID 919 e skill 308, mantendo `usage_evidence=no_direct_reference_found` e `runtime_use_proven=no` (`build/sk217ag-eff-linkage/fx_eff_usefulness.csv:2`). |

## Fato, inferência e desconhecido

- **FATO:** `fboomerff.par` está relacionado estaticamente à cena 919 e à skill 308. A relação é do PAR. Seus sistemas não referenciam diretamente `fboomerff.eff`.
- **FATO:** o DX9 enumera/indexa globalmente os EFFs encontrados, incluindo arquivos sem link PAR direto. No GL, a leitura dos EFFs é guiada pelos vínculos do runtime.
- **INFERÊNCIA:** o PAR irmão sugere que os dois arquivos podem fazer parte do mesmo pacote de conteúdo; nomes coincidentes não provam dependência técnica entre eles.
- **DESCONHECIDO:** se `fboomerff.eff` é solicitado por alguma chamada dinâmica em execução DX9 ou OpenGL, e se é visível numa sessão. Falta captura/tracing das chamadas de carregamento/execução ou rastreamento completo dos consumidores por nome.

Portanto, não descartei o decoder desse EFF nem classifiquei o arquivo como inútil. A mudança é somente de auditoria, não altera carregamento nem renderização. Não houve comparação visual.

## Teste

A fixture adiciona `child.eff` com um `child.par` irmão listado na tabela de cenas e verifica que a evidência do PAR irmão aparece em colunas separadas. O teste direcionado passou:

```bash
ctest --test-dir build -R '^ui05sk214_fx_coverage_audit$' --output-on-failure
```

Também foi executado o audit completo do Client:

```bash
build/top_fx_coverage_audit --client Client --out build/sk217ag-eff-linkage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

O alvo do auditor compilou. A compilação OpenGL completa e CTest completo não foram executados nesta etapa; o build anterior existente inclui `top_gl`, mas isso não é uma confirmação da compilação atual. Nenhuma alteração de comportamento de jogo foi feita.

## Arquivos e reversão

- Auditor: `sources/Engine/include/top/FxCoverageAudit.h`
- Fixture: `sources/tests/FxCoverageAuditTests.cpp`
- Estado: `sources/ESTADO_DO_PORTE.md`
- Patch incremental: `SK217AG.patch`

Para reverter o patch após aplicá-lo: `git apply -R SK217AG.patch`.
