# SK2.17-AT — rotação dos WIND billboard `RectPlane`

## Lacuna medida e escopo

O audit SK2.17-AS decodificou 4 sistemas WIND em 3 `.par`: dois `RectPlane` com `billboard=1`, um `RectPlane` com `billboard=0` e um filho `fsummon1.eff`. Esta mudança cobre somente os dois billboards `RectPlane`; não modifica o plano mundial nem o filho `.eff`.

## Original DX9 x porte OpenGL x mudança

| Original DX9 | Porte GL antes | SK2.17-AT |
|---|---|---|
| `_CreateWind` define `m_vVel.z = _fVecl` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3270`). `_FrameMoveWind` soma `m_vVel.z * dt` ao ângulo Z, gera `D3DXMatrixRotationZ` e posiciona a partícula (`:3409-3415`). | O ramo type 7 desenhava uma `ribbon` junto com ROUND/LINE_ROUND (`sources/Engine/src/top/EffectSystem.cpp:2706-2714`). | Com `TOP_HISTORICAL_WIND_BILLBOARD_GEOMETRY=1`, para type 7 + `billboard` + `RectPlane` e com o modo estendido ativo, desenha quad de câmera rotacionado por `profile.velocity * idadeDoSlot`. Os outros WIND mantêm o renderer atual.
| `CMPPartSys::Create` guarda `bBillBoard`; com o flag ativo, o DX9 multiplica a base billboard antes da rotação do modelo (`arquivo/fontes/Engine/src/MPParticleSys.cpp:259-263,3429-3442`). | O OpenGL mantém sua base ortonormal de câmera em `EffectViewBasis`; `quadRotated` emite vértices nessa base (`sources/Engine/src/top/EffectSystem.cpp:350-377`). | Tradução D3D→GL: a orientação billboard da matriz DX9 é representada pela base `right/up` do OpenGL, e a rotação Z local vira rotação 2D do quad. Não são copiados estados ou matrizes D3D.
| `RectPlane` tem vértices locais em `[-0.5,+0.5]` (`sources/Engine/include/top/LegacyEffectGeometry.h:51-54`); `CMPPartSys` inicializa `_vScale=(1,1,1)` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:49,796`). | O GL usa o coeficiente de escala já existente `0.12` ao calcular `size` (`EffectSystem.cpp:2251`). | O quad recebe `size*0.5` como semi-extensão, preservando o coeficiente atual e traduzindo a extensão unitária do plano. Nenhum fator de escala novo foi introduzido.

O render continua usando a textura, alfa, cor, blend e filtros do perfil. Não houve mudança de winding/culling nem de transformação para os tipos 9/18. O filho `.eff` já passa por sua rota própria de geometria e não entra nesse branch.

## Fatos, inferência e desconhecido

- **FATO:** DX9 incrementa o ângulo Z por `m_vVel.z * dt`; `m_vVel.z` vem de `_fVecl`. O GL anterior desenhava type 7 como ribbon. O Client tem dois casos WIND `RectPlane` com billboard.
- **INFERÊNCIA:** quad orientado pela base de câmera e rotacionado em seu plano é a tradução OpenGL do caso DX9 billboard `RectPlane`.
- **DESCONHECIDO:** comparação visual com Direct3D 9, se os dois recursos são acionados em jogo, e os efeitos de algum `setScale` runtime não localizado nos call sites examinados.

## Ativar e reverter

O novo comportamento fica desligado por padrão. Para comparar, além dos switches de movimento já usados:

```bash
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_WIND_BILLBOARD_GEOMETRY=1 \
./build/top_gl --profile
```

Para desligar, remova ou defina `TOP_HISTORICAL_WIND_BILLBOARD_GEOMETRY=0` e reinicie. Reverter o patch incremental: `git apply -R SK217AT.patch`.

## Testes e limitações

O teste de integração conserva as asserções existentes e acrescenta uma fixture `RectPlane` billboard: fallback ribbon e modo novo emitem os mesmos 6 vértices, mas a distribuição geométrica difere.

Build Debug completo, incluindo `top_gl`, terminou com código 0; CTest completo passou **136/136**. O teste de integração compilado com ASan/UBSan também passou (`ASAN_OPTIONS=detect_leaks=0`; LSan não executado). Nenhuma comparação visual foi feita; o resultado é experimental, não declarado fiel/validado.
