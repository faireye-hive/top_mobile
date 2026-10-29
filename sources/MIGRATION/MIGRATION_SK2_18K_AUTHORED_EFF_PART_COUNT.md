# SK2.18-K — conservar a contagem de filhos declarada no EFF

## Resultado e estado anterior

J observou G/I em prévias OpenGL e isolou oito partes barradas pelo limite de tracks do loader no ID 282. I já compõe a matriz do dummy e G já recupera o centro de filhos billboard. K corrige a preocupação comum de **contagem de filhos EFF**, sem refazer essas regras, mudar escalas ou substituir assets.

`TOP_HISTORICAL_EFF_PART_COUNT=1` permite consumir todos os filhos aceitos pelo decoder existente. Desligado por padrão: preserva os limites de compatibilidade GL de 8/16. Os formatos, limites de segurança do parser, validação de malhas/texturas e API de eventos permanecem os mesmos. A opção precisa ser escolhida antes de carregar os recursos; para A/B, reiniciar o programa.

## Original DX9 × porte anterior × mudança

| Regra | Original DX9 | GL anterior e K |
|---|---|---|
| Contagem do arquivo | `arquivo/fontes/Engine/src/MPResManger.cpp:1337-1345` lê a quantidade, dimensiona `_vecEffectList[idx]` e carrega cada filho. Não limita a oito. | `sources/Engine/src/top/EffectSystem.cpp:743` antes interrompia `resolveNestedModels` após oito tracks retidas. K só aplica esse corte com o switch desligado. |
| Controladores de filhos | `arquivo/fontes/Engine/include/MPModelEff.h:446-465` obtém `m_iEffNum` do tamanho do array e dimensiona efeitos/controladores. `arquivo/fontes/Engine/src/MPModelEff.cpp:623-629` percorre esse total no renderer VS. | O GL já usa vetores e clocks por filho. K deixa o decoder determinar a quantidade disponível; não introduz nova quantidade histórica fixa. |
| Caminho direto de magia | A mesma carga/BindingEffect/Render acima recebe os filhos do EFF, sejam procedurais, billboard ou modelo externo. | Os cortes GL de 16 billboards/16 primitivas/8 malhas externas agora também dependem do switch (`EffectSystem.cpp:584-592`). Arquivos simultâneos não são descartados por esses cortes de compatibilidade quando K está ligado. |
| Segurança do formato | A leitura DX9 usa o tamanho informado. Isso não exige aceitar arquivos malformados no porte. | `sources/Engine/include/top/LegacyEffFormat.h:150-156` continua rejeitando contagem acima de 64. K não altera esse código nem outras verificações. Fixture cobre 64 filhos e rejeição de 65 diretamente no decoder. |

O switch é lido em `sources/Engine/src/top/EffectSystem.cpp:79-82`, aceitando exatamente `1`. Os valores 8/16 continuam sendo comportamento anterior do GL, **não limites encontrados no original DX9**. O novo CSV extrai esses valores do código GL atual, sem espelhá-los em uma tabela manual.

## Tradução para OpenGL

Cada filho recuperado usa o mesmo caminho de geometria, textura, blend, sampler, câmera e matriz já utilizado pelos primeiros filhos do arquivo. A quantidade é traduzida para os vetores de tracks GL; nenhum objeto Direct3D é copiado. A conversão de coordenadas row-vector/Z-up para GL X/altura/Z e as regras de G/I continuam nos seus ramos existentes. K não muda a convenção de winding/culling nem demonstra paridade dessas outras regras.

## Auditoria global antes de implementar

`top_fx_coverage_audit` agora escreve `fx_eff_part_counts.csv`, com uma linha por EFF decodificado, separando strict/diagnostic, tipos de filhos, limites extraídos do renderer, PARs diretos, IDs do grafo e flags **potenciais** de truncamento. `runtime_clipping_proven=no` evita apresentar a contagem estática como observação de desenho.

Implementação do audit: `sources/Engine/include/top/FxCoverageAudit.h:469-473,521-527,591-598,915-944`. Testes alteram o literal do limite em uma cópia do fonte e confirmam que o audit lê o novo valor; se o limite não puder ser extraído, registra `unknown`, sem assumir oito.

Medição real sobre o Client, antes/depois, exit 0:

- 1.227 EFF decodificados no inventário diagnóstico.
- **54 EFF** com PAR direto e mais de oito partes declaradas; o grafo disponível alcança **cinco IDs de skill: 95, 223, 282, 285, 290**.
- Um EFF tem mais de 16 partes nativas (`04000011.eff`, 23). Não foi encontrado EFF individual acima de 16 billboards ou oito modelos externos nesse inventário. Combinações de arquivos simultâneos são outra maneira de alcançar o corte cumulativo; cobertas por fixture, sem alcance global medido nesta etapa.
- Esses são candidatos estruturais: malha/textura ausente, SHADE, outro consumidor ou janela de execução podem impedir o desenho. Não classificar os demais arquivos como usados em skills nem como inúteis só porque o grafo não tem um ID.

O plano de prévias é derivado do CSV e das dependências atribuídas por `Tools/eff_part_count_previews.py`, sem lista de IDs no renderer ou no planejador. O runner existente ganhou o status `potential_eff_part_truncation` e passa a registrar explicitamente o valor de K no ambiente do experimento.

## Comparação com assets reais: mesmo executável, K=0/K=1

Foram executadas duas baterias sob Xvfb/GL por software, uma com K=0 e outra com K=1. Cinco prévias por bateria, todas exit 0 e com captura inicial/final. O runner usa cópia dos dados de usuário; os hashes originais permaneceram estáveis durante cada bateria, conforme `runs.json`. Não foram editados os saves originais para preparar as prévias.

| ID / canal | EFF | Partes distintas com geometria na prévia K=0 | K=1 |
|---|---|---:|---:|
| 95 / item-extra | `04000011.eff` | 8 | 23 |
| 223 / actor | `rouse(b).eff` | 8 | 12 |
| 282 / actor | `bdlights1.eff` | 8 | 12 |
| 282 / actor | `bdlights5.eff` | 8 | 12 |
| 285 / actor | `bdsummon1.eff` | 8 | 12 |
| 290 / actor | `flasers2.eff` | 8 | 10 |

**FATO:** 33 filhos adicionais, em seis EFF e cinco IDs, foram observados com geometria emitida no ramo aninhado das prévias. Nos seis casos o conjunto de índices observado com K=1 corresponde à contagem do arquivo. Os caminhos diretos de magia com limites 16/8 foram exercitados por fixtures sintéticas; a tabela acima não os apresenta como uso Client comprovado.

**INFERÊNCIA:** o corte explica a ausência dessas partes no GL anterior. A observação A/B reforça a relação causal para a quantidade, sem demonstrar textura, duração, transformação ou aparência integralmente iguais ao DX9.

**DESCONHECIDO:** comparação visual do usuário com DX9, ocorrência desses mesmos canais em combate e execução dos demais candidatos estruturais. Illusion Slash continua **aproximado**, não aprovado visualmente. As 15 combinações não observadas por J, além das oito de ID 282 corrigidas aqui, continuam pendentes de causa isolada.

Dados: `build/runtime-part-count-sk218k-off`, `build/runtime-part-count-sk218k-on`; cada diretório contém `runs.json`, logs de rastreio e capturas. O binário tem o mesmo hash nas duas baterias; somente K foi alternado entre as opções explicitamente usadas pelo runner.

## EFF com falha estrita: não descartar nem habilitar suporte por suposição

O audit atual mantém os 31 casos em `fx_eff_usefulness.csv`: 31 recuperáveis em diagnóstico, 30 com PAR direto e `fboomerff.eff` sem referência direta comprovada; uso runtime ainda não provado por esse inventário. Nenhum asset removido.

Doze dos 54 candidatos de contagem usam decode diagnóstico: `jjbg02/03/04.eff`, `jjg02/03/04.eff`, `jjrg02/03/04.eff` e `jjyg02/03/04.eff`. Todos estão individualizados com seus consumidores PAR no CSV; o grafo de skills usado aqui não atribuiu IDs a eles. Isso não prova que não sejam usados por itens/outros donos. K não habilita automaticamente `TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS` e não modifica o decoder estrito. As seis observações Client da tabela acima não dependem desses doze arquivos.

## Testes executados

- A fixture inicial sobre o runtime pré-K falhou, exit 1, em `historical nested EFF renders all twelve authored children` (`build/sk218k-before-test.log`).
- `SK218KEffPartCountTests.cpp`: verifica defaults 8/16, modo histórico 12/17/64, billboard/procedural/LGO, filho anterior sem malha, dois EFF simultâneos e rejeição de 65. A asserção de segurança inicialmente consultava o inspector, que pode retornar sucesso de varredura sem decode estruturado; foi corrigida para chamar diretamente `legacy_eff::decode`. Nenhuma regra do parser ou teste anterior foi enfraquecido.
- `test_eff_part_count_audit.py`: três casos executados, sem skips no CTest, cobrindo inventário por categoria, limite extraído de fonte alterado, limite desconhecido, falha estrutural e plano derivado de pais/canais reais.
- Configuração/build Debug completos, incluindo `top_gl`: exit 0. CTest: **149/149**, exit 0 (`build/sk218k-build.log`, `build/sk218k-ctest.log`).
- Build focal ASan/UBSan: exit 0; CTest G/I/K **4/4**, exit 0 (`build/sk218k-asan-ctest.log`). `detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`.
- Reaudit FX: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 27 gaps/307 IDs estáticos, drift 0/falhas estruturais 0 (`build/fx-audit-sk218k`). Os tipos completos continuam parciais; não reduzir artificialmente os gaps por recuperar apenas a quantidade.
- Audit de vínculos mantém os 203 candidatos G/I e 13 canais sem recurso (`build/effect-binding-sk218k`). Não houve comparação visual DX9/OpenGL.

## Ligar, desligar, reproduzir e reverter

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure

TOP_HISTORICAL_EFF_PART_COUNT=1 \
TOP_HISTORICAL_BOUND_MODEL_MATRIX=1 \
TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1 \
TOP_HISTORICAL_DUMMY_BIND=1 \
TOP_HISTORICAL_TARGET_DUMMY_BIND=1 \
TOP_HISTORICAL_ITEM_DUMMY_BIND=1 \
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile
```

Manter os outros switches já escolhidos pelo usuário. Para desligar só K, reiniciar com `TOP_HISTORICAL_EFF_PART_COUNT=0` ou sem essa variável.

Reproduzir o experimento sem gravar nos saves originais:

```bash
build/top_fx_coverage_audit --client Client --out build/fx-audit-sk218k
python3 -B sources/Tools/eff_part_count_previews.py \
  --counts build/fx-audit-sk218k/fx_eff_part_counts.csv \
  --out build/sk218k-preview-candidates.csv
xvfb-run -a -s '-screen 0 1280x720x24' env TOP_HISTORICAL_EFF_PART_COUNT=0 \
  python3 -B sources/Tools/run_binding_previews.py \
  --candidates build/sk218k-preview-candidates.csv --out build/eff-count-off-new
xvfb-run -a -s '-screen 0 1280x720x24' env TOP_HISTORICAL_EFF_PART_COUNT=1 \
  python3 -B sources/Tools/run_binding_previews.py \
  --candidates build/sk218k-preview-candidates.csv --out build/eff-count-on-new
```

Os diretórios do runner precisam ser novos. O plano é potencial e pode conter duplicatas por EFF; o runner deduplica skill/canal. Alterar `--character`, equipamento copiado ou tempo de amostra muda o experimento e deve ficar registrado.

`SK218K.patch` é incremental sobre J e usa caminhos `sources/`. Em outra cópia da base correspondente: `git apply --check SK218K.patch`, `git apply SK218K.patch`. Reverter: `git apply -R SK218K.patch`. O workspace atual já contém K: não reaplicar aqui.

Checagem realizada em cópia limpa extraída de `build/SK218K_BASE_POS_J.zip`, fora do repositório: base compatível com a reversão de J, `git apply --check`, aplicação de K, comparação byte a byte dos nove arquivos e `git apply -R --check`, todos exit 0. Esse ZIP foi materializado para representar a base pós-J, não é um dos ZIPs antigos. Os temporários pré-K não persistiram na renovação do ambiente; os preimages de CMake/auditor foram reconstruídos removendo apenas as adições K inspecionadas, e os demais preimages foram preservados em `build/sk218k-baseline`. O patch não inclui as modificações locais anteriores. Evidência da checagem: `build/sk218k-patchcheck.json`.

Checklist visual: comparar K=0/K=1 e DX9 na mesma habilidade, personagem, equipamento, pose e instante; verificar as partes adicionais, duração, sobreposição, centro no dummy e relação com o chão. Não compensar diferenças aumentando escala. Como K pode aumentar geometria e overdraw, comparar também o custo na máquina do usuário; não medir FPS com trace ligado.

## Limites e próximo passo

O decoder mantém o limite de segurança de 64 partes por EFF; não há suporte prometido a arquivos acima disso. Modelos/texturas não resolvidos continuam excluídos. Culling, clocks/RNG e outras transformações não se tornam completos por corrigir a quantidade.

Próximo passo: isolar as **15 combinações restantes não observadas em J**, sem atribuir a ausência ao contador já corrigido: `foot.eff`, `speed up.eff`, `01010003.eff`, `bdwinda2.eff`, `zzfb5.eff`, `zzfb2.eff`, `zzfb1.eff`, `zzfb3.eff`. O CSV identifica **10 IDs**: 2,11,60,81,238,248,283,284,325,447. Medir a causa antes de priorizar uma correção comum.
