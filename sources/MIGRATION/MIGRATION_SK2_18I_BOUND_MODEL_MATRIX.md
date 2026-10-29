# SK2.18-I — matriz do dummy para filhos EFF não-billboard

## Resultado e base

Correção compartilhada no motor, sem condição por ID de skill: `TOP_HISTORICAL_BOUND_MODEL_MATRIX=1` aplica a parte linear da matriz real do dummy ao filho `.eff` não-billboard de um MODEL/STRIP elegível e usa a translação do vínculo mais o offset do PAR. Desligado por padrão. Recupera o ramo de composição com o osso; não declara o efeito completo visualmente igual ao DX9.

G já cobre filhos billboard e H já investigou os 13 canais sem recurso usando TXT/BIN. Esta etapa usa o mesmo provedor de matrizes de actor/target/item de G e complementa o ramo não-billboard. Não reimplementa o controlador de partículas, o parser EFF ou o bridge de pose.

## Original DX9 × porte anterior × mudança

| Regra | Original DX9 | Porte antes de I / mudança |
|---|---|---|
| Elegibilidade do vínculo | `arquivo/fontes/Engine/src/MPParticleSys.cpp:1205-1227`: pai não-billboard, sem `_pCModel`, tipo MODEL/STRIP e `_bModelDir=false` guardam `_SBoneMat`. `:1220-1222` acrescenta `_vOffset` à translação, em mundo. | O GL já recebia matrizes, mas `positionForAge` só usava seus componentes de translação (`sources/Engine/src/top/EffectSystem.cpp:1574-1620`). I seleciona a matriz efetiva em `:1654-1670` e condiciona o uso a pai/filho elegíveis em `:2822-2830`. |
| Atualização do pai vinculado | `_FrameMoveModel` copia `_SBoneMat` em `MPParticleSys.cpp:3157-3159`; `_FrameMoveStrip` faz o mesmo em `:3218-3220`. O centro calculado por MoveTo/range não substitui essa matriz. | Antes, GL desenhava no centro com range/ângulos do pai, mesmo vinculado. I usa origem do dummy + offset, sem aplicar novamente range/path de MoveTo ou ângulos PAR a esse ramo. `EffectSystem.cpp:2825-2835`; `LegacyEffectGeometry.h:128-136`. |
| Encadeamento do filho | `MPParticleSys.cpp:1526-1531` aplica escala do sistema e passa a matriz ao filho. O ramo sem osso, que chama rotações PAR, está em `:1542-1574`. `arquivo/fontes/Engine/include/MPModelEff.h:71-96` monta escala/rotação/translação da chave filha; `:653-669` monta a transformação do controlador. | O emitter CPU já monta a chave filha; o novo `ParentTransform::bone` aplica o 3×3 row-vector depois da escala do sistema (`sources/Engine/include/top/LegacyEffectGeometry.h:125-136`). O caminho sem vínculo mantém a rotação existente. |
| Composição de malhas | `arquivo/fontes/Engine/src/MPModelEff.cpp:843-846` multiplica a transformação filha por `m_SpmatBone` no ramo não-billboard. Malha externa usa a mesma composição em `:699-705`. | I fornece o mesmo parent para malha `.lgo` (`EffectSystem.cpp:2914-2915`) e procedural (`:3000-3001`). O ramo billboard continua separado (`:2973-2990`), coberto por G. |
| Matriz disponível no frame | O vínculo usa matriz, não apenas posição, conforme as funções acima. | `sources/Engine/src/top/ActorSkin.cpp:1430-1541` já compõe dummy/pose/mundo e entrega matrizes finitas. `:139-152` monta yaw e translação do ator sem escala arbitrária adicional. O bridge em `sources/Platform/linux/main.cpp:2830-2857` amostra após o desenho do ator e respeita switches de vínculo. I reutiliza esses dados. |

## Tradução para OpenGL

O cálculo local permanece nos eixos MindPower XYZ, Z sendo altura. Para um vetor linha `(x,y,z)`, a parte linear é `(x*m0+y*m4+z*m8, x*m1+y*m5+z*m9, x*m2+y*m6+z*m10)`. Isso preserva rotação, escala e cisalhamento presentes na matriz; normalizar seus eixos perderia parte do vínculo. A translação é usada uma vez, pela origem do desenho. O offset PAR é acrescentado em mundo, como no original, e não é escalado/rotacionado pelo osso.

Depois da composição, o emitter CPU converte para GL `(X,altura,Z) = (DX X, DX Z, DX Y)`, usando o caminho já existente. Não copia a transposição de upload do shader D3D (`MPModelEff.cpp:858`) para vértices já transformados na CPU. O fator de geometria `.3f` já existia no porte e permanece; esta etapa não demonstra que a conversão global de tamanho de todos os efeitos está visualmente correta.

O renderer GL desabilita culling (`sources/Engine/src/gl/GfxDeviceGL.cpp:348`). I não recupera os estados de culling específicos dos vários ramos DX9: essa diferença permanece e não pode ser declarada resolvida por uma correção de matriz.

## Auditoria de todos os consumidores elegíveis

O tipo 6 era o topo do ranking G: 1.041 instâncias/504 arquivos/242 IDs estáticos, com cobertura parcial. O cruzamento do ramo específico não-billboard é mais restrito:

```bash
build/top_effect_binding_audit --client Client --out build/effect-binding-sk218i
```

Execução real, exit 0: 1.880 combinações de canal/sistema/parte resolvidas; 181 candidatas não-billboard, **34 IDs de skill**, 26 PARs. São 122 combinações STRIP e 59 MODEL; 90 actor, 83 target e 8 item. O CSV agora acrescenta `non_billboard_binding_candidate` ao fim das colunas, preservando o status billboard anterior. A condição em `sources/Engine/include/top/EffectBindingAudit.h:75-88` é confrontada com o renderer pela fixture de integração; não há tabela manual de IDs no runtime.

IDs estáticos encontrados: `1,2,3,4,5,12,16,24,47,60,61,81,111,114,118,201,231,232,238,239,248,254,257,263,265,269,271,273,282,283,284,294,325,447`.

**FATO:** os dados fornecidos conectam esses canais a um PAR/filho elegível e configuram um dummy. Inclui os IDs 3 e 81 de Illusion Slash, com 2 e 3 combinações respectivamente. Frozen Arrow 93, Berserk 84 e Taunt 242 não têm candidatos neste subcaso do audit; isso não afirma que seus outros problemas acabaram. Taunt tem filhos billboard tratados pelo ramo G. IDs usados na fixture sintética não são prova sobre os assets reais de uma habilidade com o mesmo número.

**INFERÊNCIA:** usar apenas a translação deixa a malha vinculada sem acompanhar a orientação/escala do osso e pode explicar parte dos erros visuais com o personagem em movimento. A ligação entre essa diferença e o defeito percebido pelo usuário precisa de comparação visual.

**DESCONHECIDO:** execução efetiva desses 34 IDs/canais, disponibilidade de cada dummy em sessão e concordância visual. O CSV mantém `runtime_binding_proven=no`. O bridge de actor monstro e barco ainda não oferece todos os mesmos caminhos do jogador; um candidato de catálogo pode não receber a matriz no runtime GL.

Reaudit FX executado após a mudança: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 27 gaps estáticos/307 IDs, drift 0, falhas estruturais 0 (`build/fx-audit-sk218i`, log `build/sk218i-fx-audit.log`). Esses totais gerais não devem cair artificialmente: o tipo inteiro continua parcial e não teve aprovação visual. As 31 recusas do parser estrito continuam discriminadas no CSV de utilidade, recuperáveis em diagnóstico; nenhuma foi removida.

## Testes e resultados reais

- A fixture inicial compilou sobre o runtime pré-I e falhou com exit 1 na comparação de vértices (`build/sk218i-before-test.log`). Após a implementação passou. A fixture foi ampliada para incluir também `.lgo`, chaves filhas com escala/rotação, matriz afim com escala/cisalhamento e exclusões.
- `sources/tests/SK218IBoundModelMatrixTests.cpp` gera PAR v14, EFF v6 e LGO 0x1003. Verifica vértices para MODEL/STRIP e actor/target/item, escala do pai, chave filha, matriz identidade e matriz afim, range/ângulos PAR não zero e offset em mundo. As comparações calculam o resultado row-vector esperado, sem chamar o helper novo como oráculo.
- Regressões específicas: switch desligado permanece distinto do ramo novo; `_bModelDir=true`, pai billboard, filho billboard, dummy ausente e vínculo item/target desligado não recebem a composição nova. O audit identifica os mesmos quatro candidatos sintéticos do renderer.
- Configuração e build Debug completos com `top_gl`: exit 0. `ctest --test-dir build --output-on-failure`: **145/145**, exit 0 (`build/sk218i-final-build.log`, `build/sk218i-ctest.log`).
- ASan/UBSan Debug: build dos testes I/G e vizinhos UI05G/K/L com exit 0; seleção focal CTest **5/5**, exit 0. Logs `build/sk218i-asan-build.log`, `build/sk218i-asan-final-build.log`, `build/sk218i-asan-ctest.log`. `ASAN_OPTIONS=detect_leaks=0` desabilita detecção de vazamentos nessa execução, não a checagem de endereço; UBSan usa `halt_on_error=1`.
- Não houve sessão gráfica jogável, captura DX9/GL ou aprovação visual. Illusion Slash permanece **aproximado**, não validado visualmente.

## Compilar, executar, comparar e reverter

Da raiz:

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure

TOP_HISTORICAL_BOUND_MODEL_MATRIX=1 \
TOP_HISTORICAL_DUMMY_BIND=1 \
TOP_HISTORICAL_TARGET_DUMMY_BIND=1 \
TOP_HISTORICAL_ITEM_DUMMY_BIND=1 \
TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile
```

Manter os demais switches históricos já escolhidos pelo usuário. Para A/B de I, alterar somente `TOP_HISTORICAL_BOUND_MODEL_MATRIX=0/1` e reiniciar. Para isolar G, alterar seu próprio switch separadamente. Desligar ambos remove os dois comportamentos novos; cada um continua desligado por padrão.

Reproduzir sanitizers:

```bash
cmake -S sources -B build/sk218d-asan -DCMAKE_BUILD_TYPE=Debug \
  -DTOP_BUILD_UI_TESTS=ON \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build/sk218d-asan -j4 --target \
  top_sk218i_bound_model_matrix_tests top_sk218g_bound_billboard_tests \
  top_ui05g_tests top_ui05k_tests top_ui05l_tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sk218d-asan \
  -R 'sk218[gi]_|ui05[kl]_|ui05sk24_' --output-on-failure
```

`SK218I.patch` é incremental sobre H; caminhos começam com `sources/`. Para outra cópia da base correspondente: `git apply --check SK218I.patch`, `git apply SK218I.patch`. Reverter: `git apply -R SK218I.patch`. O workspace atual já contém I; não reaplicar o patch aqui. H e I são patches separados porque audit de tabela e composição de matriz são preocupações distintas.

Checagem executada em cópia dos arquivos capturados antes de cada etapa: `git apply --check`, aplicação, comparação byte a byte com o resultado e `git apply -R --check`, todos exit 0. A sequência H→I também passou, com 10 arquivos finais coincidentes e reversão I→H. Não é uma checagem sobre os ZIPs antigos `sources.zip`/`sources_251146.zip`: seus índices não contêm migrações SK2.18, portanto não representam a base pós-G/H deste patch. Reconstruir essa base a partir de um desses ZIPs exigiria a cadeia anterior completa; não alegar que essa reconstrução foi feita.

Checklist visual para o usuário:

- Escolher um canal candidato cujo dummy exista efetivamente no ator/item/alvo; registrar ID, equipamento, pose e câmera.
- Comparar GL com I=0/I=1 e DX9 no mesmo instante: orientação da malha durante a animação, centro no dummy, offset vertical e tamanho relativo.
- Confirmar que filho billboard continua olhando para a câmera e que efeitos no chão sem vínculo não recebem orientação do osso.
- Conferir a regressão de textura de Frozen Arrow com `TOP_HISTORICAL_MODEL_TEXTURE=1`, sem usar aumento de escala para avaliar I.

## Limitações e próximo passo

A composição do vínculo não resolve sequência global RNG, cadência de todos os controladores, donos sem bridge de dummy, transformações globais EFF ainda não cobertas, culling, ordenação ou sincronização do dano de Part_trace. Os 13 canais sem recursos de H também não ganham assets inventados. A constatação de que o efeito usa uma matriz histórica não substitui a cadeia completa de atualização e desenho.

Próximo passo: instrumentar/observar em execução a disponibilidade dos vínculos nos **34 IDs candidatos** desta regra, para separar os canais efetivamente exercitados dos candidatos apenas estáticos antes de priorizar o próximo defeito visual comum.
