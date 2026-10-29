# SK2.17-T — audit de uso e tipos dos filhos STRIP `.eff`

## Resultado medido

O novo `top_strip_eff_animation_audit` cruza `.par` type 6 com referências que têm extensão `.eff` explícita, decodifica os alvos e relaciona os tipos dos subefeitos aos enum e dispatch atuais do código DX9 original. Executado no Client: **1.031 `.par`**, **3.476 sistemas**, **875 referências STRIP→`.eff`**, **568 arquivos `.eff` únicos referenciados/decodificados**, **0 referências não resolvidas** e **0 falhas de decodificação entre os referenciados**. A auditoria FX geral também encontrou 1.227 `.eff` e zero falhas; portanto, nesta cópia do Client não há arquivo `.eff` recusado que precise de triagem individual.

O audit ignora nomes de modelo sem extensão ao procurar `.eff`; por exemplo, não transforma `RectPlane` em uma suposta referência `RectPlane.eff`. Isso evita classificar um modelo primitivo como arquivo de efeito ausente. Os relatórios são gravados em `build/sk217t-strip-eff-audit-final/`.

| Tipo DX9 | Partes em `.eff` ligados a STRIP | Partes com múltiplos quadros de animação | Arquivos `.eff` | Skills alcançadas pelo grafo atual |
|---|---:|---:|---:|---:|
| `EFFECT_FRAMETEX=1` | 2 | 2 | 1 | 2 |
| `EFFECT_MODELUV=2` | 4 | 4 | 2 | 0 |
| `EFFECT_MODELTEXTURE=3` | 967 | 367 | 235 | 34 |

No total, 973 partes têm tipo 1–3; **600 têm no máximo uma entrada** e não avançam uma pista animada. Todos os 973 nomes de modelo são primitivos nativos reconhecidos; não há `.lgo` nem modelo desconhecido nesses registros. Os dados de skills são alcance pelo arquivo de dependências atual e não provam execução em jogo.

## Comparação com o original DX9

| Original DX9 | Porte GL/audit | Conclusão |
|---|---|---|
| Enum e despacho: `I_Effect.h:827-831`; `CMPModelEff::FrameMove` seleciona `MODELUV`, `MODELTEXTURE` e `FRAMETEX` e atualiza por `dt` (`MPModelEff.cpp:568-589`). O audit lê os valores do enum e exige os tokens do dispatch no original; informa linhas no CSV sumário. | `LegacyEffFormat.h` decodifica tipo, arrays e nomes; o porte usa `part.type` para os caminhos `sampleQuad`, `sampleMesh` e texture frames (`HistoricalEffTimeline.h:88-97,138-190`; `EffectSystem.cpp:2110-2119,2174-2178,2241-2264`). | A tabela do relatório deriva do campo binário e compara os códigos, sem manter uma correspondência manual de dados Client. |
| `I_Effect::IsItem()` depende do modelo carregado e, se não houver modelo, reconhece `.lgo` (`I_Effect.h:968-975`). `CEffectModel::IsItem()` testa `_lpVB`/`_lwMesh` (`I_Effect.h:456-462`). Tipos 2/3 só atualizam suas pistas sob `!IsItem`; tipo 1 também atualiza no ramo `IsItem` (`MPModelEff.cpp:568-589`). | O `.eff` não serializa esse resultado runtime. O audit identifica tipo de modelo e classifica como gate falso para primitivo nativo; mantém outros casos como dependentes do loader. | Nos dados Client examinados, todos os 973 modelos type 1–3 são primitivos nativos. O original os busca na malha registrada ou cria via `CreateTob` (`I_Effect.cpp:238-275`; `MPResManger.cpp:1086-1135`), portanto `IsItem` é falso para estes subefeitos. Não encontramos tipo 2/3 cujo comportamento fosse alterado pelo gate omitido no GL. |

## `.eff` recusados e utilidade

O `fx_decode_failures.csv` da auditoria geral contém apenas o cabeçalho. O audit T também terminou com zero falhas de decodificação em referências STRIP explícitas e zero referências explícitas sem arquivo candidato. Portanto, nenhum `.eff` atual foi descartado por falha de decoder. A lista completa por arquivo e subefeito está em `fx_strip_eff_parts.csv`; ausências/recusas referenciadas são separadas em `fx_strip_eff_unresolved.csv`.

## Fatos, inferência e desconhecido

- **FATO:** enum, uso de controlador e condições de `IsItem` foram verificados nos fontes DX9 indicados pelo executável; o relatório escreve linha e arquivo no campo `DX9_source_check`.
- **FATO:** o Client apresenta 875 referências STRIP explícitas para 568 arquivos `.eff` únicos, todos decodificados; não há falhas a considerar “não usadas”.
- **INFERÊNCIA:** os nomes primitivos nativos dos 973 subefeitos executam com `IsItem=false`, pois o original resolve esses nomes para meshes com buffer ou cria um mesh variável. A auditoria não executa o binário DX9, então não prova a execução runtime de cada ocorrência.
- **DESCONHECIDO:** uso por clientes diferentes, assets fora de `Client/effect`, modelos `IsItem` não representados nesta amostra e paridade visual DX9/GL.

## Teste e execução

O teste sintético gera `.par` e `.eff` com os três tipos, verifica a contagem e o status de modelo `.lgo`, e inclui um `.eff` corrompido explicitamente referenciado por um STRIP. O relatório classifica esse fixture como `referenced_eff_decode_failed`, provando que falha referenciada não some da auditoria.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_strip_eff_animation_audit top_strip_eff_animation_audit_tests -j$(nproc)
ctest --test-dir build -R ui05sk217t_strip_eff_animation_audit --output-on-failure
build/top_strip_eff_animation_audit \
  --client Client \
  --out build/strip-eff-animation-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --dx9-header arquivo/fontes/Engine/include/I_Effect.h \
  --dx9-controller arquivo/fontes/Engine/src/MPModelEff.cpp
```

Este audit não altera o renderer nem declara validação visual. O patch incremental `SK217T.patch` contém apenas o utilitário, fixture, CMake e relatórios desta etapa; pode ser revertido com `git apply -R SK217T.patch`.
