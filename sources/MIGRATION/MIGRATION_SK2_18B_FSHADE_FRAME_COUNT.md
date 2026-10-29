# SK2.18-B — contador de cópias DX9 do `Part_fshade`

## Resumo

Adicionei `TOP_HISTORICAL_FSHADE_FRAME_COUNT=1`, desligado por padrão. No modo opt-in, a contagem de cópias do voo `Part_fshade` mantém o acumulador do DX9: quando `tempo > 0,6 s`, descarta o excesso, zera o acumulador e incrementa uma única cópia por chamada. A idade derivada continua como caminho compatível quando a variável está ausente ou em `0`.

## Original DX9 × porte GL × mudança

| Original DX9 | Porte GL antes | Mudança |
|---|---|---|
| O índice 3 de `MagicList` seleciona `Part_fshade`; sua rotina desenha primeiro, move o projétil, testa proximidade/terreno e percorre `_iCurSNum` cópias (`arquivo/Antigo.zip::fontes/Client/src/EffectObj.cpp:61-69,356-399`). O controlador inicializa `_iCurSNum=1` e `_fCurTime=0` (`arquivo/fontes/Engine/src/MPEffectCtrl.cpp:186-187`). | `shadeAt(mode, age)` calculava a contagem diretamente da idade contínua e limitava o resultado a 16 (`sources/Engine/include/top/HistoricalMagicMotion.h:28-35`, pré-etapa). Em um único `dt=1,21 s`, isso produzia três entradas de cópia, embora a rotina DX9 só pudesse incrementar uma vez nessa chamada. | O novo `ShadeTimeline` guarda acumulador e contador por instância; `advance(dt)` usa o `dt` real, compara estritamente `> 0,6`, zera o acumulador e incrementa uma única vez (`HistoricalMagicMotion.h:10-27`). O estado é atualizado para voos mode 3 apenas com o switch ligado (`EffectSystem.cpp:98-101,1167-1172`) e alimenta billboards, malhas nativas e modelos externos. |

## Dados atuais e alcance

- `Client/scripts/table/magicsingleinfo.txt:10` associa Magic 18 a `04000009.eff`, velocidade 15 e modo 3.
- A auditoria estática `sources/auditoria-historica/dependencies.csv:41,111,358,500` associa esse Magic 18 aos IDs 13 (Summon), 81 (Illusion Slash), 248 (Shark Attack) e 325 (Illusion Slash Illusion). É associação de dados, não prova de execução numa sessão.
- O patch afeta somente voos com modo 3, não os canais de ator, impacto, chão ou água dessas skills. Como o controlador é compartilhado, a opção cobre os quatro IDs acima quando eles realmente usam esse Magic 18.

## Fatos, inferência e desconhecido

- **FATO:** o código DX9 reinicia `_fCurTime` para zero e incrementa `_iCurSNum` apenas uma vez por chamada quando `_fCurTime > 0,6` (`EffectObj.cpp:394-399`). A contagem inicial é 1 (`MPEffectCtrl.cpp:186-187`).
- **FATO:** a tabela do Client declara Magic 18 com modo 3/`Part_fshade`, velocidade 15 e `04000009.eff`.
- **FATO:** a conversão espacial existente representa o plano X/Y do DX9 como X/Z no OpenGL; este patch não altera essa conversão (`EffectSystem.cpp:1974-1975`).
- **INFERÊNCIA:** avançar essa timeline uma vez por `EffectSystem::step(dt)` aproxima o ponto de atualização do DX9. O vínculo entre uma chamada de `step` e cada chamada de `RenderMagic` em todos os ritmos de renderização não foi comparado em execução.
- **DESCONHECIDO:** a resposta visual exata e diferenças sob FPS variável só podem ser avaliadas com capturas DX9/OpenGL da mesma skill, distância, câmera e sequência de quadros. A etapa não declara fidelidade visual.

## Testes executados

- Build Debug completo, incluindo `top_gl`: exit code 0.
- CTest completo: **141/141 aprovados**.
- Regressões focais `ui05k_native_effect_primitives` e `ui05sk28_magic_fshade_motion`: passaram.
- A fixture de integração usa um `.eff` sintético e um passo longo de 1,21 s para distinguir o contador opt-in da contagem derivada da idade. Não substitui execução do recurso real em jogo.
- Não executei o jogo nem comparei visualmente com DX9. Sanitizers não foram executados nesta etapa.

## Comparação no cliente

```bash
# Caminho compatível atual
TOP_HISTORICAL_BLEND=1 ./build/top_gl --profile

# Contador modelado a partir de Part_fshade do DX9
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_FSHADE_FRAME_COUNT=1 ./build/top_gl --profile
```

Compare Illusion Slash (81), Summon (13), Shark Attack (248) e Illusion Slash Illusion (325), mantendo mesma distância, alvo, câmera e FPS. Para desligar, remova a variável ou use `TOP_HISTORICAL_FSHADE_FRAME_COUNT=0`. Para reverter o patch em uma cópia aplicada: `git apply -R SK218B.patch`.

## Limites e próximo trabalho

O patch corrige apenas a cadência de crescimento do rastro. Não corrige a ordem renderizar/mover, colisão de terreno, atualização de alvo, cópias 3D nem limite de cópias do helper antigo; esses comportamentos exigem etapas próprias e comparação visual. O multiplicador provisório de impacto de Illusion Slash e as aproximações de Frozen Arrow, Taunt e Berserk permanecem questões abertas, não foram certificados por esta etapa.
