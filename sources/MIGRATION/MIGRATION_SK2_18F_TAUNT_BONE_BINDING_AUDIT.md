# SK2.18-F — auditoria do vínculo de Taunt ao personagem

## O que já existe

SK2.18-D/E adicionaram o caminho opt-in de Part_trace e corrigiram sua consulta de chão. O porte já recebe matrizes dos dummies: `TOP_HISTORICAL_DUMMY_BIND=1` habilita as matrizes do jogador, e `TOP_HISTORICAL_TARGET_DUMMY_BIND=1` habilita as dos alvos. O renderer usa a translação dessas matrizes. Esta etapa F acrescenta informação ao auditor somente-leitura, sem mudar o renderer.

## Evidência DX9 × GL × mudança no auditor

| Item | Original DX9 | Porte GL atual | Mudança nesta etapa |
|---|---|---|---|
| Campo de direção do modelo | `_bModelDir` é serializado depois de `_iRoadom` e lido quando `dwVersion>13` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:4170-4173,4330-4334`). O construtor inicializa o campo como falso (`:96`). | O runtime pula esse byte (`sources/Engine/src/top/EffectSystem.cpp:267`); o auditor também o pulava. | `LegacyParAudit.h:144-147` agora preserva `modelDirection`. `Tools/par_audit.cpp:150,192` acrescenta `model_direction` ao fim do CSV, preservando a ordem das colunas existentes. |
| Canal actor de Taunt | `skillinfo.txt:188`: actor effect 336, dummy 1, action key -1. `STAttack.cpp:238-252` passa o dummy a `SelfEffect`; `Character.cpp:940-945` segue a matriz do dummy. | `SkillVisual.h:113-124` preserva o canal e dummy; `EffectSystem.cpp:1565-1576` usa apenas os elementos de translação da matriz, ou raiz do personagem como fallback. | O CSV permite confirmar a condição `_bModelDir` nos sistemas desse canal. |
| Vínculo em STRIP | `BindingBone` habilita `_bUseBone` para MODEL/STRIP sem billboard, modelo `.eff` e `_bModelDir=false`; copia a matriz e adiciona o offset à translação (`MPParticleSys.cpp:1205-1227`). `_FrameMoveStrip` copia `_SBoneMat` ao slot (`:3218-3220`); o render passa essa matriz ao filho `.eff` (`:1526-1531`). | O `ParentTransform` tem escala e ângulos, mas não recebe a matriz do dummy (`LegacyEffectGeometry.h:118-129`; `EffectSystem.cpp:2787-2796`). | A ausência da matriz completa existe para filhos que a utilizem. É necessário verificar o billboard do filho antes de concluir que sua orientação deveria seguir o osso. |
| Billboard filho vinculado | Para filho billboard, `CMPModelEff::Render` usa a câmera e **substitui** sua translação pela translação do vínculo (`MPModelEff.cpp:820-840`); a multiplicação de matriz completa em `:845-846` pertence ao ramo não-billboard. | `EffectSystem.cpp:2933-2946` suprime `keyframe.position` somente quando o **PAR pai** tem billboard; não considera o vínculo de um pai STRIP não-billboard. | Os dois filhos de Taunt são billboards e têm posições animadas: a lacuna concreta é a preservação indevida dessa translação filha no GL quando o dummy está vinculado. |
| Canal target | Taunt declara effect 337/dummy 1 em `skillinfo.txt:188`; `EffDelay.cpp:35-38` passa esses campos a `SelfEffect` do alvo. | `EffectSystem.cpp:1590-1604` pode seguir o dummy do alvo, mas o ramo billboard filho ainda aplica sua posição animada. | A regra de centro vinculado deve considerar actor e target, respeitando os flags do PAR e a presença efetiva do dummy. |

## Dados medidos

`build/top_par_audit --client Client --out build/par-audit-sk218f` terminou com exit 0: 1.031 PAR decodificados, 0 falhas, 3.476 sistemas e 110 referências ausentes. O novo campo foi contado a partir do CSV, não inferido do nome de arquivo:

- `model_direction=0`: 3.256 sistemas; `=1`: 220.
- 955 sistemas MODEL/STRIP têm `.eff`, `billboard=0` e `model_direction=0`. Esse total identifica condições de formato; **não** prova que todos recebam `BindingBone` durante uma sessão.

| PAR / sistema | Tipo | Billboard | model_direction | Modelo | Offset DX9 |
|---|---|---|---|---|---|
| `cfsf.par` / 0 | STRIP (6) | 0 | 0 | `cf.eff` | (0,0,1) |
| `cfsf.par` / 1 e 2 | BLAST (3) | 1 | 0 | `RectPlane` | (0,0,1) |
| `cfsj.par` / 0 | STRIP (6) | 0 | 0 | `cfsj.eff` | (0,0,1) |

As linhas de dados estão em `build/par-audit-sk218f/par_systems.csv:670-673`. Os dois STRIP selecionam a condição DX9 de matriz completa quando vinculados ao dummy. Os dois BLAST billboard seguem a translação pelo ramo `MoveTo`, não a mesma regra de matriz completa. Não generalizar a regra para todo o PAR.

`top_eff_inspect` agora mostra os limites das posições dos keyframes, além do billboard já existente. `build/top_eff_inspect Client/effect/cf.eff` e o comando para `cfsj.eff` terminaram com exit 0: cada arquivo contém um `RectPlane` billboard, com posições variando de (0,0,0) a (0,0,1). Logs: `build/sk218f-cf-inspect.log`, `build/sk218f-cfsj-inspect.log`. Portanto a rotação completa do osso **não** foi estabelecida como necessária para orientar esses dois quads. O centro DX9 vinculado deve ignorar esse deslocamento animado; o GL atualmente o soma porque o PAR pai não é billboard.

O arquivo de dependências associa `cfsf.par` ao actor de Taunt 242 e `cfsj.par` aos targets de Taunt 242 e Roar 243 (`sources/auditoria-historica/dependencies.csv:345-348`). São **2 IDs estáticos**, sem prova de uso runtime individual.

## Tradução OpenGL exigida pela próxima correção

O original compõe matrizes row-vector em eixos DX9 XYZ/Z-up. O porte fornece as matrizes nessa convenção e troca a posição para GL X/altura/Z. Para o filho billboard, o centro já convertido do vínculo deve prevalecer sobre `keyframe.position`; a câmera continua orientando o quad. A composição completa da matriz é uma preocupação distinta para filhos não-billboard e precisa respeitar a ordem de escala/transformação e os flags do PAR.

**INFERÊNCIA:** a translação filha indevidamente somada pode alterar a altura do ícone de Taunt durante o efeito. **DESCONHECIDO:** quanto isso explica do defeito visual relatado; não houve captura comparável DX9/GL. Escala, altura do dummy e demais controladores continuam sujeitos às suas próprias evidências.

## Auditor FX e EFF

O executável FX foi recompilado e executado em `build/fx-audit-sk218f`: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 27 gaps/307 IDs de skill, drift 0, falhas estruturais 0, strips não associados 0. O ranking mede os controladores e não inclui ainda uma métrica de pose/matriz completa.

O CSV `fx_eff_usefulness.csv` continua contendo uma linha individual para cada uma das 31 recusas strict-only: 31 recuperáveis em modo diagnóstico, 30 referenciadas diretamente por PAR e `fboomerff.eff` sem referência direta conhecida. Nenhum uso runtime individual foi comprovado e nenhum arquivo foi removido. Esta extensão do audit PAR não altera o decoder `.eff`.

## Testes e limites de execução

- Fixture Debug: dez versões PAR; verifica `_bModelDir` verdadeiro/falso em v14/v15, ausência nas versões antigas e truncamento no byte de v14. Nenhum assert foi desativado ou teste antigo enfraquecido.
- Build Debug completo incluindo `top_gl`: exit 0 (`build/sk218f-full-build.log`). CTest completo: **142/142** (`build/sk218f-ctest.log`).
- Fixture PAR em ASan/UBSan: **1/1** com `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1` (`build/sk218f-asan-ctest.log`).
- Auditorias operam sobre o Client da raiz. Não houve sessão jogável, renderização de pose comparada com o DX9 nem validação visual.

## Reproduzir e reverter

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/top_par_audit --client Client --out build/par-audit-sk218f
```

`SK218F.patch` é incremental sobre a árvore com E corrigida e contém somente caminhos `sources/`. Em uma cópia dessa base: `git apply --check SK218F.patch`, depois `git apply SK218F.patch`. Para reverter F: `git apply -R SK218F.patch`. Os arquivos locais já contêm F; não reaplicar aqui. Nenhuma variável nova é necessária para a auditoria, pois não há comportamento novo no jogo.

## Próxima preocupação

Recuperar o centro de billboard filho vinculado em MODEL/STRIP, sob switch desligado por padrão, com fixture que distingue a translação do vínculo da posição animada filha e cobre `_bModelDir`/dummy ausente. Taunt/Roar são os 2 IDs estáticos identificados por estes recursos; o alcance global exige cruzamento dos vínculos e dos billboards filhos, não somente contar os 955 sistemas.
