# SK2.17-BR — posição central histórica do controlador MODEL

## Resumo

O DX9 reposiciona o slot único de MODEL em `_vPos + range/2` a cada `_FrameMove`. No OpenGL, o ramo `singleModel` zerava esse deslocamento para type 5. Acrescentei a tradução opcional em `TOP_HISTORICAL_MODEL_POSITION=1`, desligada por padrão, e conectei a lacuna ao FX audit com contagem derivada dos próprios PARs.

## DX9 original × porte OpenGL × mudança

| Aspecto | DX9 original | OpenGL antes | Mudança |
|---|---|---|---|
| Posição do slot MODEL | `_FrameMoveModel` atribui `m_vPos = _vPos + D3DXVECTOR3(range[0]/2, range[1]/2, range[2]/2)` por update (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3097-3101`). `_CreateModel` cria um único slot e inicializa vida/cursor (`:3074-3087`). | O parser preserva os três ranges (`sources/Engine/src/top/EffectSystem.cpp:176-178`). O ramo `singleModel` usava `baseX/floor/baseZ` sem midpoint para type 5; o recuo de `range/2` era aplicado somente ao type 6 STRIP (`EffectSystem.cpp:2441-2448`, antes da etapa). | `HistoricalModelPosition::midpointOffset` calcula range/2 e o ramo só o aplica para type 5 quando o switch está ligado (`HistoricalModelPosition.h:6-14`; `EffectSystem.cpp:2441-2457`). Type 6 mantém o caminho anterior. |
| Tradução de coordenadas | O controlador DX9 armazena X/Y horizontais e Z vertical. | O renderer OpenGL usa X/Z no plano horizontal e Y como altura. | Traduzi o vetor de posição na fronteira do renderer: `range[0]` soma em X, `range[1]` em Z e `range[2]` em Y. É uma permutação de eixos DX9 Z-up → GL Y-up; não copiei matriz D3D nem alterei blend, winding ou culling. |
| Ativação | — | Sem o switch, a posição GL existente continua em uso. | `TOP_HISTORICAL_MODEL_POSITION=1`, desligado por padrão. Nenhum outro tipo é atingido. |

## Medição

O FX audit agora mede `model_midpoint_position` por `type==5 && algum range != 0`, em vez de manter uma lista manual. Client atual: **161 sistemas**, **102 PARs**, **9 IDs estáticos de skills** nesta lacuna; total type 5: 777 sistemas. Exemplos de associação estática: skill 3 “Illusion Slash”, 378 “Little Snow Ball” e 456 “Red Thunder Cannon”. O vínculo não prova que esses sistemas foram executados na sessão.

A execução completa do FX audit registrou 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 26 lacunas/307 IDs estáticos alcançados, zero deriva e zero falhas estruturais. Entre 31 EFF que falham no decode estrito, 31 recuperam no modo diagnóstico, 30 têm PAR direto e nenhum tem uso runtime provado. Esta etapa não removeu nem alterou EFF.

## Fato, inferência e desconhecido

- **FATO:** `_FrameMoveModel` escreve `range/2` diretamente na posição; o GL anterior deixava `corner=0` para type 5. Os dados Client medidos contêm 161 sistemas type 5 com range não zero.
- **INFERÊNCIA:** para esses registros, acrescentar o midpoint no GL representa melhor o posicionamento explícito do DX9 do que deixar o slot na origem do controlador.
- **DESCONHECIDO:** execução runtime dos nove IDs, unidade de mundo em todos os mapas, interação visual com dummies/owners, delta/frame cadence do Client e equivalência visual DX9/OpenGL. A opção não está validada visualmente.

## Testes e auditoria executados

- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit 0.
- `cmake --build build -j$(nproc)` — exit 0, incluindo `top_gl`.
- `ctest --test-dir build --output-on-failure` — **140/140**, exit 0.
- Fixture integrada do modelo usa `.par` sintético com range `(4,6,8)` e filho `.eff`; testa switch desligado, offset `(2,3,4)` e deslocamento do centroide quando ligado.
- Fixture integrada compilada e executada em Debug com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`) — exit 0.
- FX audit focal: `build/top_fx_coverage_audit --client Client --out build/sk217br-model-position-audit --deps sources/auditoria-historica/dependencies.csv --table sources/auditoria-historica/fx_port_coverage.csv --source sources/Engine/src/top/EffectSystem.cpp --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp --dx9-particle-source arquivo/fontes/Engine/src/MPParticleSys.cpp --dx9-particle-header arquivo/fontes/Engine/include/MPParticleSys.h` — exit 0; feature 161/102/9, drift 0, falhas 0.
- Comparação visual não executada.

## Ativação, comparação e reversão

Rode o mesmo Client/skill/câmera em DX9 e GL; no GL compare os dois modos com processo reiniciado:

```bash
TOP_HISTORICAL_MODEL_POSITION=0 ./build/top_gl --profile
TOP_HISTORICAL_MODEL_POSITION=1 ./build/top_gl --profile
```

Use um efeito MODEL ligado a PAR com range não zero. Verifique o centro do modelo em X, profundidade e altura; confira que STRIP, Frozen Arrow, Taunt e Berserk não mudaram. Para desligar, use `TOP_HISTORICAL_MODEL_POSITION=0`. Para reverter o patch: `git apply -R SK217BR.patch`.
