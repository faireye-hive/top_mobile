# SK2.17-AS — auditoria do flag billboard para WIND

Etapa somente leitura. O `top_par_audit` já decodificava o byte `billboard`, mas não o exportava em `par_systems.csv`; por isso não era possível separar a orientação de câmera do plano mundial ao comparar WIND com o DX9.

## Original DX9 x porte GL x mudança

| Evidência | DX9 original | Porte / auditor anterior | SK2.17-AS |
|---|---|---|---|
| Orientação WIND | `_FrameMoveWind` acumula `m_vCurAngle.z` com `m_vVel.z * dt`, cria rotação Z e transforma a aceleração antes de posicionar (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3402-3415`). `_CreateWind` inicializa `m_vVel.z = _fVecl` (`:3270`). | O GL calcula a trajetória estendida mas desenha tipos 7/9/18 pelo caminho `ribbon` (`sources/Engine/src/top/EffectSystem.cpp:2706-2714`). | Sem alterar runtime, o CSV agora expõe `billboard` para escolher a tradução correta em uma etapa futura.
| Dado gravado | `CMPPartSys::Create` recebe `bBillBoard` e escolhe o caminho de orientação (`arquivo/fontes/Engine/src/MPParticleSys.cpp:259-263,344-349`). | `ParticleProfile` já conserva o byte lido (`sources/Engine/src/top/EffectSystem.cpp:186-188`); `par_audit::System` também o decodifica (`sources/Engine/include/top/LegacyParAudit.h:92-105`), mas a linha CSV não o incluía (`sources/Tools/par_audit.cpp`, cabeçalho e emissão de `par_systems.csv`). | Coluna aditiva `billboard` no inventário PAR; fixture verifica que o byte `1` sobrevive à decodificação.

## Medição no Client

O audit leu `Client/effect/`: 1.031 PAR, 1.031 decodificados, zero falhas e 3.476 sistemas. Encontrou 4 WIND em 3 arquivos:

| PAR e índice | Modelo | Flag billboard | Leitura do caso |
|---|---|---:|---|
| `bdwindaa.par:2` | `RectPlane` | 1 | plano orientado à câmera |
| `bdwindaa.par:3` | `RectPlane` | 0 | plano com orientação mundial |
| `fdeathaa.par:0` | `fsummon1.eff` | 0 | filho de efeito; não é o caminho de quad simples |
| `fsmokeaa.par:1` | `RectPlane` | 1 | plano orientado à câmera |

**FATO:** o original diferencia billboard e não-billboard, e o porte já lê o flag em `ParticleProfile`. **INFERÊNCIA:** os dois casos com `RectPlane` e billboard podem usar uma rotação em base de câmera no GL; os outros não podem ser tratados com a mesma fórmula. **DESCONHECIDO:** dimensão mundial efetiva por causa de `_vScale`, aparência em runtime e se todos os quatro sistemas são acionados durante jogo.

O novo campo não altera arquivos `.par`, API do jogo, renderização ou comportamento. Nenhuma falha `.eff` foi envolvida nesta medição: ela usa a saída do `top_par_audit`, não descarta nem ignora arquivos do catálogo.

## Execução e testes

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_par_audit top_sk23_par_tests -j$(nproc)
ctest --test-dir build -R ui05sk23_all_par_audit --output-on-failure
build/top_par_audit --client Client --out build/sk217as-wind-flag-audit
```

Resultado desta execução: build dos dois alvos terminou com código 0; o teste passou 1/1; o audit terminou com código 0 e encontrou 1.031 PAR, 1.031 decodificados, zero falhas e 3.476 sistemas. O build completo, CTest completo e comparação visual ainda não foram executados nesta etapa.

## Próximo passo

Implementar separadamente a rotação dos dois WIND billboard `RectPlane` atrás de um `TOP_HISTORICAL_*` desligado por padrão. Manter o WIND não-billboard e o filho `.eff` sem alteração até haver uma transformação GL derivada e testada para cada caso.
