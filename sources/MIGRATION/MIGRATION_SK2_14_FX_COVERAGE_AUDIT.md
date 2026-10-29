# SK2.14 — Auditoria de cobertura dos efeitos históricos

## Objetivo

Ferramenta **somente leitura**, em C++17 e sem dependência de OpenGL, para
cruzar `.par`, `.eff`, `dependencies.csv` e o código `EffectSystem.cpp`.
O objetivo é encontrar **lacunas prováveis de implementação** e priorizar
correções que atinjam várias skills. Não é um validador de fidelidade visual.

## Compilar

Na raiz do TalesOfPirateDX9:

```sh
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target top_fx_coverage_audit top_fx_coverage_tests -j$(nproc)
ctest --test-dir build -R ui05sk214_fx_coverage_audit --output-on-failure
```

## Executar

Na raiz do repositório, com `Client/effect` disponível:

```sh
./build/top_fx_coverage_audit \
  --client Client \
  --out build/fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv
```

Para importar explicitamente as **seis trilhas CMPStrip diretas** detectadas
pela auditoria SK2.12 (inclusive `swordlight02.par`):

```sh
./build/top_fx_coverage_audit \
  --client Client --out build/fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv
```

Na invocação padrão **a própria ferramenta importa automaticamente esse CSV**
quando o arquivo distribuído existe na pasta indicada. O CSV é derivado da
**auditoria da SK2.12**; não representa uma nova decodificação do bloco CMPStrip
pela SK2.14. O parser `LegacyParAudit.h` ainda examina os sistemas convencionais,
não o bloco adicional de trilhas diretas.

Para investigar regressões na própria tabela de status:

```sh
./build/top_fx_coverage_audit --client Client --out build/fx-coverage --strict
```

`--strict` retorna código 1 ao encontrar arquivos não decodificados,
trilhas importadas que não existem em `Client` ou divergências evidentes
entre a tabela e os ramos do código. Código 2 corresponde a erro de
argumentos/configuração, diretório de saída dentro de `Client` ou entrada
obrigatória inválida. Erros de associação de skill são reportados, mas não
são considerados falhas de decodificação do asset.

## Arquivos gerados

| Arquivo | Finalidade |
|---|---|
| `fx_gap_ranking.csv` | **Comece por aqui**. Lacunas ordenadas por número de skills (se houver dependencies.csv), depois instâncias. |
| `fx_type_coverage.csv` | Tipos `.par` 1–18, quantidades, avaliação manual, presença de ramos e validação. |
| `fx_feature_coverage.csv` | Campos históricos presentes e ocorrência dos nomes no loader/runtime. |
| `fx_eff_primitives.csv` | Modelos de subefeitos `.eff`: primitivo nativo, `.lgo` ou não reconhecido. |
| `fx_skill_gaps.csv` | IDs de habilidades vinculados às pendências via recursos diretos e `.par → .eff`. |
| `fx_table_drift.csv` | Tipo presente nos assets sem entrada de regra ou divergência entre tabela e código. |
| `fx_decode_failures.csv` | Arquivos `.eff` e `.par` que não puderam ser interpretados estruturalmente. |
| `fx_missing_dependencies.csv` | Referências diretas de efeitos não associadas a arquivos existentes; não assumir corrupção. |
| `fx_basename_collisions.csv` | Arquivos distintos com o mesmo nome base; podem deixar vínculo com skill ambíguo. |
| `fx_coverage_summary.json` | Totais de arquivos, instâncias, pendências, status dos dados e decodificação. |

Os `skills_reached` contam **IDs de skills distintos**, não linhas do CSV,
nem número de disparos ou importância visual. Quando o arquivo de
`dependencies.csv` não estiver disponível, o relatório escreve `n/a` para
contagens de skills em CSV e `null` no sumário JSON; **não** confunde ausência
de dados com zero skills afetadas.

O cruzamento só considera as entradas de dependência relacionadas a efeito:
`scene_par`, `par_status`, `par`, `eff`. Texturas, modelos de personagens,
IDs de grupo e outras entradas são ignorados nessa etapa para evitar falsos
positivos. A resolução de nomes numéricos aplica `0` à esquerda até oito
dígitos e atravessa os links `.par → .eff` (com proteção contra ciclos).

## Como interpretar os estados

- `approximate`: o código tem uma representação simplificada. **Não há paridade DX9**.
- `partial`: há um caminho dedicado, mas mecanismos do DX9 ainda podem faltar.
- `generic_fallback`: não foi encontrado um tratamento especializado, e a
  renderização cai em uma alternativa genérica ou simplesmente não produz a
  geometria esperada.
- `missing_specialization`: existe menção ao tipo no código, porém nenhuma
  implementação histórica completa foi verificada. Exemplo: SHADE (13).
- `native_primitive` / `external_lgo`: formato do nome/modelo reconhecido;
  não significa que a geometria foi renderizada nem comparada ao DX9.
- `unrecognized_model`: o nome não corresponde aos primitivos nativos nem a
  `.lgo`; pode exigir investigação ou ser nome histórico válido suportado por
  outro caminho.
- `code_evidence`: presença de condição/campo no *texto-fonte*, **não uma
  execução real**. A inspeção remove `//`, mas não entende todas as macros,
  chamadas indiretas, classes ou comentários em bloco; alterações profundas
  do código exigem reavaliar a tabela `fx_port_coverage.csv`.
- `visual_validation=not_visually_verified` **sempre**, porque a ferramenta
  nunca executa uma cena DirectX/OpenGL nem compara imagens.

A classificação técnica em `fx_port_coverage.csv` é deliberadamente
**curada por análise de código** e necessita revisão quando migrarmos um novo
mecanismo. O programa não promove automaticamente um tipo a `fiel` só porque
encontrou `if (profile.type == X)` no renderizador.

## Limitações e uso responsável

1. **Não tenho sua pasta `Client/effect` completa neste ambiente**; não
   executei a auditoria sobre todos os 1.031 `.par` e 1.227 `.eff` de fato.
2. O inventário de seis CMPStrip diretos foi produzido anteriormente pela
   SK2.12 e deve ser atualizado quando novos controladores diretos aparecerem.
3. Arquivos `.eff` são examinados estruturalmente, mas os `.lgo` não são
   decodificados nem testados graficamente nesta ferramenta.
4. Referências por basename podem ser ambíguas, por isso são registradas em
   `fx_basename_collisions.csv` (e todos os candidatos são associados, sem
   fingir que sabemos qual variante o runtime selecionaria).
5. A ferramenta **não altera arquivos** de `Client`, do renderer, nem saves.
6. `fx_gap_ranking.csv` é uma lista de *candidatos a correção*, não uma lista
   de defeitos visuais comprovados. Reproduções DX9 são necessárias para
   certificação.

### Casos interessantes para inspecionar

- **Illusion Slash (81):** `swordlight02.par` consta no inventário CMPStrip
  direto, mesmo tendo zero sistemas convencionais no decoder `.par`.
- **Berserk (84)** e **Taunt (242):** as atualizações SK2.7–SK2.12 mudaram
  tanto timing quanto composição. A cobertura estrutural é diferente da
  fidelidade do efeito observado.
- **Frozen Arrow (93) e Dual Shot (90):** rastrear `.par`, `.eff` e subefeitos
  para encontrar recursos que ainda usem fallback.

## Validação desta entrega

- Construção **Debug** dos alvos `top_fx_coverage_audit` e
  `top_fx_coverage_tests` executada sem GL.
- Teste `ui05sk214_fx_coverage_audit` executado e aprovado.
- Testes incluem tipos FIRE, ARRAW e SHADE, número de sistemas, CSV de
  skills, importação direta CMPStrip, normalização numérica, alerta de drift,
  opção `--strict` e proibição de gravar dentro de `Client`.
- Smoke-test adicional feito com apenas `04000009.eff` e `swordlight02.par`
  fornecidos anteriormente. **Não** deve ser confundido com uma execução no
  Client completo.
