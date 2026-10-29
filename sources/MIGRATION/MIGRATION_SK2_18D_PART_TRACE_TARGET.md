# SK2.18-D — Part_trace acompanha alvo e colisão com terreno

## Resumo

Adicionei a tradução do controlador DX9 `Part_trace` para o voo de skills no porte OpenGL. O novo caminho depende de `TOP_HISTORICAL_PART_TRACE=1` e continua desligado por padrão. A auditoria estática aponta Frozen Arrow (skill 93 / Magic 20) e Illusion Slash (skill 3 / Magic 10) entre as skills potencialmente alcançadas pelo filtro mode 2; isso não comprova execução em runtime. Illusion Slash 81 (Magic 18 / mode 3), Taunt 242 e Berserk 84 não são ativados por este caminho segundo as dependências Client auditadas.

## Evidência: DX9 original × porte GL × mudança

| Comportamento | Original DX9 | Porte OpenGL antes | Mudança nesta etapa |
|---|---|---|---|
| Render e avanço | `Part_trace` renderiza antes de avançar e limita cada passo a 1,5 unidade (`arquivo/fontes/Client/src/EffectObj.cpp:243-260`). | Voo mode 2 usava a posição analítica de duração fixa (`sources/Engine/src/top/EffectSystem.cpp:1141-1170,1606-1620`). | Timeline separada armazena a amostra renderizada e traduz a ordem render-antes-do-movimento (`sources/Engine/include/top/HistoricalPartTrace.h:29-43,62-102`; `EffectSystem.cpp:1193-1204,1606-1609`). |
| Fim por alvo | DX9 termina quando a distância inicial é menor que 1, quando o passo excede a distância restante, ou quando entra na faixa planar; proximidade compara somente X/Y (`EffectObj.cpp:245-267`; `arquivo/fontes/Engine/include/MPParticleSys.h:48-53`). | A duração derivava da velocidade e distância inicial; o evento GL não carregava a identidade do alvo. | Implementa limites DX9 e a faixa planar no plano GL X/Z (`HistoricalPartTrace.h:14-25,69-85`). |
| Alvo móvel | `Part_trace` obtém `GetCha(_iTargetID)`, soma 1 em Z e corrige posição/direção/distância quando o alvo muda (`EffectObj.cpp:274-312`). `CMagicCtrl::CalculateEmission/Emission` inicializa alvo, direção e distância (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:464-515`). | `PlayCombat` emitia o voo sem identidade do alvo (`sources/Engine/src/top/play/PlayCombat.cpp:842-846`). | O voo agora leva o índice do alvo; o step usa a raiz atual, somando 1 à altura GL (`PlayCombat.cpp:842-846`; `EffectSystem.cpp:1185-1204`). A raiz é amostrada depois de `ActorSkin::build` e antes do step (`sources/Platform/linux/main.cpp:2823-2851,2858-2860`). |
| Terreno e resultado | DX9 para em `HitTestMap` (`EffectObj.cpp:268-271`); `CMagicCtrl::Stop()` inicia o Result em `_vPos` (`arquivo/fontes/Engine/include/MPEffectCtrl.h:418-431`). | O resultado era disparado pelo fim da duração fixa. | A timeline consulta a altura do mapa GL (`HistoricalPartTrace.h:80-85`) e cria o Result na posição final/colisão (`EffectSystem.cpp:1456-1487`). |

### Traduções de coordenadas

- **FATO:** o helper DX9 `PointInstrPointRange` compara X/Y e ignora Z (`MPParticleSys.h:48-53`).
- **TRADUÇÃO:** no mundo GL deste porte, os eixos no plano são X/Z e a altura é Y. Portanto o teste planar histórico X/Y vira X/Z; o `+1` em Z do alvo DX9 vira `+1` em Y GL. Nesta etapa D, a colisão usou `MapFile::heightAt(x,z)`. A etapa E corrigiu essa consulta: `CGameScene::GetGridHeight(float,float)` multiplica por 2 antes de consultar a grade pequena (`arquivo/fontes/Client/src/Scene.h:536-540`), portanto a amostra correspondente é `MapFile::sceneGridHeightAt`, incluindo o quadrante `btBlock`.
- A atualização usa `dt` do GL; o limitador `1.5` e a proporção de correção do alvo mantêm os valores escalares do DX9. Não é cópia de API D3D.

## Auditoria e alcance dos dados

A execução atual de `top_fx_coverage_audit` sobre `Client/` contou 1.031 PAR, 1.227 EFF, 3.476 sistemas e 2.832 partes EFF; registrou 27 gaps estáticos e 307 IDs de skill associados a gaps, sem drift e sem falhas estruturais. `fx_decode_failures.csv` não tem linhas. As 31 recusas estritas de `.eff` continuam recuperáveis no modo diagnóstico; 30 têm referência direta a PAR. Nenhum desses arquivos foi descartado. `fboomerff.eff` não tem aresta PAR direta e sua execução permanece desconhecida.

O cruzamento estático entre `dependencies.csv` e os modos da tabela Client encontrou 122 linhas de dependência mode 2 e 59 IDs únicos potencialmente alcançáveis. É alcance estático, não telemetria. A correção seleciona mode 2 com altura absoluta e alvo conhecido. Frozen Arrow é um caso medido; Illusion Slash skill 3 é outro candidato. A diferença entre Illusion Slash 3 e 81 é importante: o 81 usa Magic 18/mode 3 e não entra nesta regra.

## Implementação

- `HistoricalPartTrace.h` contém a timeline isolada e amostras usadas para preservar render-before-update.
- `EffectSystem` habilita por variável de ambiente, segue raízes atuais, aplica colisão, mantém a duração histórica enquanto segue e cria o Result após a parada.
- `PlayCombat` passa o alvo do voo do jogador e usa `-1` (raiz do jogador) para o voo de monstro.
- O default continua desativado. Desligue com `TOP_HISTORICAL_PART_TRACE=0` ou removendo a variável. Reverta apenas este patch com `git apply -R SK218D.patch` em uma cópia pré-SK2.18-D.

## Testes executados

- Configuração Debug: `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit 0.
- Build completo: `cmake --build build -j4` — exit 0, incluindo `top_gl`.
- CTest completo: `ctest --test-dir build --output-on-failure` — **142/142 aprovados**.
- Fixture focal ASan/UBSan no workspace: `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sk218d-asan --output-on-failure -R sk218d_part_trace_target_and_terrain` — **1/1 aprovado**. A tentativa anterior sob `/tmp` não iniciou por permissão de execução; foi refeita no workspace.
- Auditor FX atual: exit 0, com os totais acima.
- Não executei uma comparação visual DX9/OpenGL nem uma sessão jogável com Client. Logo, esta mudança está testada por fixtures e compilação, mas não foi validada visualmente como fiel.

## Limitações, fatos e desconhecidos

- **FATO:** `pendingAttack_` continua calculado a partir da distância/duração inicial (`PlayCombat.cpp:835-846`). Este patch altera o tempo visual do projétil e do resultado, não o relógio de dano/combate; a sincronização completa ainda diverge.
- **DESCONHECIDO:** se o alvo desaparecer, o DX9 chama `Stop()` quando `GetCha()` retorna nulo e depois desreferencia o ponteiro (`EffectObj.cpp:279-284`). Esse caminho original é inválido/ambíguo. O porte mantém o último alvo conhecido; essa escolha é uma proteção de runtime, não equivalência histórica demonstrada.
- **FATO:** se não houver raiz do alvo no primeiro step, o caminho opt-in não começa e permanece a animação existente. Alvos dinâmicos só são acompanhados quando há raiz disponível.
- **INFERÊNCIA:** o root GL é o melhor ponto correspondente disponível para esta primeira etapa. Os fontes DX9 usam `GetPos()` do personagem, não um dummy, para `Part_trace`; ainda não há comparação visual para confirmar escala/altura do root entre motores.
- Nenhuma alteração foi feita em escalas de efeito nem na regra de prioridade `.eff` sobre `.lgo` do Frozen Arrow. `TOP_FROZEN_RESULT_GROUND` permanece independente.

## Checklist de comparação visual

Use o mesmo Client, mapa, skill, alvo, câmera e posição inicial em cada captura. Compare DX9 com GL para Frozen Arrow em alvo parado, alvo movendo lateralmente e voo interceptando terreno. Verifique o rastro, ponto de chegada, instante do Result e sincronização com o dano. Registre também Illusion Slash skill 3 separadamente; skill 81 não está coberta por este switch.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

TOP_HISTORICAL_PART_TRACE=0 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
./build/top_gl --profile

TOP_HISTORICAL_PART_TRACE=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
./build/top_gl --profile
```

O resultado visual do usuário é necessário antes de qualquer promoção do switch ao padrão.
