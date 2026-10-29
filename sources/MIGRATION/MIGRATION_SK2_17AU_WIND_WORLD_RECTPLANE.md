# SK2.17-AU — WIND `RectPlane` no plano do mundo

## Lacuna medida

O audit SK2.17-AS exportou o `billboard` que o decoder `.par` já lia. No Client foram encontrados quatro sistemas `WIND`: dois `RectPlane` billboard, um `RectPlane` sem billboard e um filho `.eff`. A etapa AT cobriu os dois billboards; esta etapa cobre somente o único `RectPlane` não-billboard. O relatório mais recente (`build/sk217au-current-audit`) mede 1 sistema em 1 arquivo e 1 skill rastreada para `wind_world_rectplane_rotation`. A ligação é a skill 283, Black Dragon Illusion Slash. O filho `.eff` e WINDs com outros modelos permanecem fora do escopo.

## DX9 original x porte GL x mudança

| Assunto | Original DX9 | Porte antes desta etapa | Mudança AU |
|---|---|---|---|
| Criação e ângulo | `_CreateWind` grava `_fVecl` em `m_vVel.z` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3270`). `_FrameMoveWind` acumula o ângulo por `m_vVel.z * dt`, cria uma rotação Z e transforma a aceleração (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3402-3415`). | O GL desenhava type 7 como `ribbon` junto com type 9 e 18 (`sources/Engine/src/top/EffectSystem.cpp:2736-2745`). | Novo `TOP_HISTORICAL_WIND_WORLD_GEOMETRY=1`, desligado por padrão: somente type 7, `billboard=false`, `modelName=RectPlane` e modo estendido ativo usam um quad no plano do mundo, rotacionado pelo ângulo acumulado aproximado `velocity * idade do slot` (`EffectSystem.cpp:2730-2735`). |
| Transformação DX9 → GL | O modelo DX9 guarda posição XYZ e usa Z vertical; a matriz de partícula carrega escala, rotação Z e translação (`MPParticleSys.cpp:3429-3442`). | O renderer GL usa Y vertical e X/Z horizontais; `groundQuad` já fornece uma quad no plano X/Z (`EffectSystem.cpp:379-384`). | A rotação XY sobre Z vira uma rotação no plano GL X/Z. A geometria nativa `RectPlane` tem vértices XY em ±0,5 (`LegacyEffectGeometry.h:51-54`); o renderer preserva o fator de tamanho existente (`EffectSystem.cpp:2251`) e passa metade do tamanho como extensão. Não copia matriz D3D, nem muda blend, UV, winding ou culling. |
| Escopo do asset | O flag `_bBillBoard` seleciona uma matriz de câmera separada; o caminho não-billboard usa a matriz de rotação/modelo da partícula (`MPParticleSys.cpp:3429-3442`). | A representação anterior de type 7 era uma fita, independentemente do flag. | O branch novo só é tomado para o caso medido no Client. WIND billboard da etapa AT, filho EFF, tipos 9/18 e modelos diferentes ficam em seus branches anteriores. |

## Fatos, inferências e desconhecidos

- **FATOS:** DX9 acumula rotação Z com `m_vVel.z * dt`; o `.par` define esse valor como `_fVecl`; o Client tem um único `WIND` não-billboard `RectPlane` (no `bdwindaa.par`); o ramo GL anterior usava uma `ribbon`.
- **FATO de tradução:** as posições do controlador DX9 permanecem em XYZ Z-up até a fronteira do renderer, onde X/Y horizontais viram GL X/Z e Z vertical vira GL Y. A rotação Z local portanto age em GL X/Z.
- **INFERÊNCIA:** `profile.velocity * extra.age` representa o ângulo do controlador se a idade da instância corresponder ao tempo efetivamente acumulado pelo DX9. O slot reutilizado e o relógio de render podem divergir da sequência real de `FrameMove`.
- **DESCONHECIDO:** não há comparação visual DX9/GL. Textura, coordenadas UV do mesh de runtime, curva temporal completa de tamanho/cor e cadência real não foram verificadas visualmente.

## Ativação, teste visual e reversão

Compare a skill 283 primeiro com o modo novo desligado e depois com estes switches ativos:

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_WIND_WORLD_GEOMETRY=1 \
./build/top_gl --profile
```

Para desligar, remova a variável ou defina `TOP_HISTORICAL_WIND_WORLD_GEOMETRY=0` e reinicie. A mudança continua experimental até a comparação visual. Reversão do patch desta etapa: `git apply -R SK217AU.patch`.

## Testes e limitações

A fixture sintética adiciona um WIND não-billboard `RectPlane`; a integração compara a extensão geométrica anterior com a quad no plano X/Z, conservando as asserções existentes. O audit também verifica que o novo atributo é selecionado apenas no tipo/modelo/flag correspondentes.

Executado em Debug: configuração CMake e build completo, incluindo `top_gl`; CTest **136/136** aprovado. O audit real leu 1.031 PAR e 1.227 EFF: 3.476 sistemas, 2.832 partes, 26 gaps, 307 skills com gaps, deriva 0 e falhas estruturais 0. O parser estrito ainda rejeita 31 EFF; todos os 31 são recuperados pelo modo diagnóstico com parâmetros brutos. O loader DX9 enumera e indexa os EFF (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`), então nenhum foi classificado como dispensável. A integração WIND passou com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`; LSan não executado). Comparação visual não executada.
