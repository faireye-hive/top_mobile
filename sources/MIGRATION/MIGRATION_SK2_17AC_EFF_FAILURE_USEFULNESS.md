# SK2.17-AC — triagem individual das falhas de `.eff`

## Objetivo e decisão

O `top_fx_coverage_audit` agora cruza falhas do parser estrito com referências explícitas `.par -> .eff`, IDs de `Client/scripts/table/sceneffectinfo.txt` e o grafo de dependências existente. Para arquivos que só passam no modo permissivo, o CSV registra modelos e a contagem de parâmetros não finitos por parte. O relatório não conclui uso em runtime a partir de presença no catálogo.

**Decisão para os 31 arquivos da lista antiga:** não descartar nem ignorar nenhum nesta etapa. Os 30 arquivos com referência direta têm também ID em `sceneffectinfo.txt`; o trigésimo primeiro (`fboomerff.eff`) não tem referência direta localizada, mas o carregador DX9 enumera e indexa todos os `.eff` do diretório. Não há prova de que `fboomerff.eff` seja solicitado durante uma sessão, portanto seu uso dinâmico permanece desconhecido.

## Evidência: DX9 original x porte atual x mudança

| Assunto | Original DX9 | Porte/auditor atual | Mudança desta etapa |
|---|---|---|---|
| Parâmetros do modelo | `I_Effect::LoadFromFile` lê segmentos, altura e raios com `fread` sem validação de finitude (`arquivo/fontes/Engine/src/I_Effect.cpp:662-669`). Para parâmetros animados, lê os valores e chama `Create()` (`:688-701`). | O leitor estrito usa `Reader::f32`, que recusa float não finito (`sources/Engine/include/top/LegacyEffFormat.h:103-109,217-223`); a opção de compatibilidade usa `rawF32` (`sources/Engine/src/top/EffectSystem.cpp:71-74,99-102`). | O auditor executa os dois modos; falha estrita e decodificação permissiva ficam separadas e não alteram o decoder/runtime. |
| Carregamento de efeitos | `LoadTotalEffect` percorre `*.eff`, chama `LoadEffectFromFile` e insere o nome no mapa (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). | O GL resolve recursos em caminhos acionados pelo catálogo/parâmetro e não reproduz o carregamento global (`sources/Engine/src/top/EffectSystem.cpp:480-499`). | Por isso, “sem referência direta encontrada” não é tratado como prova de arquivo inútil. |
| Uso e diagnóstico | As regras de carregamento não dizem se um efeito chegou a ser exibido. | O auditor já lia `.par`; agora também associa os `.eff` pais a cenas e habilidades rastreáveis (`sources/Engine/include/top/FxCoverageAudit.h:250-285,451-490`). | Novo `fx_eff_usefulness.csv` com evidência por arquivo, parâmetros inválidos e `runtime_use_proven=no`. |

## Resultado medido no Client disponível

Execução de `build/top_fx_coverage_audit --client Client --out build/sk217ac-eff-usefulness`:

- 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas PAR e 2.832 partes EFF decodificadas.
- 31 falhas no modo estrito; **31/31** são recuperadas estruturalmente no modo diagnóstico com parâmetros de modelo brutos. Falhas estruturais no modo diagnóstico: **0**.
- 30/31 têm referência direta em PAR; esses 30 também correspondem a ID de `sceneffectinfo.txt`. Há 32 referências PAR no total, porque `jbg05.eff` aparece em `jbg02.par`, `jbg03.par` e `jbg04.par` (IDs 3641, 3642 e 3643).
- 98 componentes de parâmetro base não finitos e nenhum componente animado não finito. A distribuição por modelo decodificado é: `RectPlane` 91, `boom.lgo` 3, `8yuejl01.lgo` 3 e `ttd.lgo` 1; `Cylinder` e `Rect` não têm componentes não finitos nesses 31 arquivos.
- `fboomerff.eff` contém `RectPlane` e `boom.lgo`, mas não tem referência direta `.par`, ID de cena ou habilidade alcançada pelo grafo atual. O carregamento global DX9 mantém possível uma solicitação indireta. **Não classificado como não usado.**
- A coluna de habilidades vazia não prova ausência de vínculo: o grafo atual não representa todos os consumidores de `sceneffectinfo.txt`.

O teste de fixture cria um EFF com um `NaN` em `RectPlane`: o parser estrito o rejeita e o parser diagnóstico lê a estrutura, identifica o modelo, a referência PAR, o ID de cena e a habilidade da fixture. Um EFF malformado separado continua registrado como falha real.

## Limites e próximos dados necessários

**FATOS:** a tabela comprova referências estáticas e que o DX9 percorre o diretório inteiro. Os parâmetros não finitos encontrados estão em `RectPlane` e modelos LGO; nenhum está em `Cylinder` nos arquivos examinados.

**INFERÊNCIA:** para os efeitos que chegam a modelos LGO/`RectPlane`, esses parâmetros geométricos provavelmente não alteram a geometria consumida pelo renderer. Isso não substitui comparação com o comportamento completo DX9 nem prova o caminho em runtime.

**DESCONHECIDO:** não há telemetria de chamadas `LoadEffect`/`Play` no cliente real, nem vínculo completo de `sceneffectinfo` para skills/ações. Para declarar `fboomerff.eff` não usado, falta rastreamento runtime ou evidência completa de todas as chamadas de nome no DX9.

Nenhum asset foi removido e nenhuma habilidade foi declarada visualmente equivalente. Não houve comparação visual.

## Testes executados

- `cmake --build build -j$(nproc)` — sucesso; o build Debug existente compilou `top_gl` e os alvos de teste.
- `ctest --test-dir build --output-on-failure` — **128/128** passaram.
- `build/top_fx_coverage_audit --client Client --out build/sk217ac-eff-usefulness` — sucesso, `drift=0`, `failures=0` no modo diagnóstico.
- O teste novo do audit passou em Debug; a fixture usa verificação explícita e não depende de `NDEBUG`.

## Reversão

`git apply -R SK217AC.patch`
