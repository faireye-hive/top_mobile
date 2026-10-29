# SK2.17-S — clocks DX9 de UV e textura em STRIP `.eff`

## Resultado

O GL agora pode reproduzir os clocks próprios dos controladores UV e textura dos filhos `.eff` disparados por sistemas STRIP (`type=6`). O comportamento novo fica desligado por padrão e é controlado por `TOP_HISTORICAL_STRIP_EFF_UV_CLOCK=1`. Isso não habilita automaticamente o clock de keyframes da etapa R (`TOP_HISTORICAL_STRIP_EFF_FRAME_STEP`).

## Evidência: original DX9 x porte atual x mudança

| Original DX9 (fato) | Porte GL antes desta etapa (fato) | Mudança nesta etapa |
|---|---|---|
| `EFFECT_FRAMETEX=1`, `EFFECT_MODELUV=2`, `EFFECT_MODELTEXTURE=3` (`arquivo/fontes/Engine/include/I_Effect.h:827-831`). `CMPModelEff::FrameMove` escolhe a pista pelo tipo e passa o `dt` atual (`arquivo/fontes/Engine/src/MPModelEff.cpp:568-589`). | O parser preserva `part.type`, arrays UV, listas de regiões e nomes de texturas animadas (`sources/Engine/include/top/LegacyEffFormat.h:178-238`), mas a renderização type 6 amostrava nomes/UV por `modelAge` (`sources/Engine/src/top/EffectSystem.cpp:2110-2118,2132-2140,2174-2178,2241-2263`). | Cada filho passa a guardar três clocks independentes. O renderer consulta o clock de textura para tipo 1, UV interpolado para tipo 2 e região discreta para tipo 3 (`sources/Engine/include/top/HistoricalEffTimeline.h:22-47,88-97,138-190`; integração `sources/Engine/src/top/EffectSystem.cpp:2057-2061,2110-2119,2132-2142,2174-2178,2241-2264`). |
| `CTexCoordList::GetCurCoord` incrementa com o `dt`, avança uma posição somente se `fCurTime > m_fFrameTime`, zera o residual; o último conjunto interpola para si (`arquivo/fontes/Engine/src/I_Effect.cpp:2388-2427`). | `textureFrameIndex(seconds, frameSeconds, count)` deriva o índice por `floor(totalSeconds/frameSeconds)` e pode saltar vários índices (`sources/Engine/include/top/LegacyEffFormat.h:273-278`). | `advanceClock` preserva `>` estrito, um único avanço e descarte do excedente; interpolação mantém o último UV como destino (`sources/Engine/include/top/HistoricalEffTimeline.h:27-38,142-158`). |
| `CTexList::GetCurTexture` troca o conjunto completo de UVs com a mesma comparação estrita e wrap (`arquivo/fontes/Engine/src/I_Effect.cpp:2539-2568`). | `sampleUV`/`sampleMeshUV` escolhiam região com relógio total (`sources/Engine/include/top/LegacyEffFormat.h:297-299,323-328`). | Tipo 3 agora usa `regionClock` por filho e seleciona a região corrente; malhas incompatíveis continuam com os UVs default do GL (`sources/Engine/include/top/HistoricalEffTimeline.h:155-158,178-189`). |
| `CTexFrame::GetCurTexture` seleciona uma textura por controlador e usa `>` estrito, um avanço e wrap (`arquivo/fontes/Engine/src/I_Effect.cpp:2621-2641`). O tipo 1 recebe essa chamada em `CMPModelEff::FrameMove` (`MPModelEff.cpp:580-589`). | O GL selecionava uma textura animada por `modelAge` (`sources/Engine/src/top/EffectSystem.cpp:2110-2118,2132-2140`). | Tipo 1 agora lê `textureClock` por filho; seleção de material `.lgo` continua passando pela prioridade de textura existente (`EffectSystem.cpp:2110-2119,2132-2142`). |

## Dados do Client e `.eff` recusados

`top_fx_coverage_audit` foi executado contra `Client/` com a tabela e as dependências históricas do projeto. Resultado observado: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 875 sistemas STRIP ligados a `.eff`, 22 lacunas, `drift=0`, `failures=0` e `unmatched_strips=0`. `fx_decode_failures.csv` contém somente o cabeçalho; portanto, nesta execução não existe `.eff` recusado cuja utilidade precise ser triada. A contagem de referências é evidência de uso em dados/tabelas, não prova de que cada efeito foi executado durante uma sessão.

## Fatos, inferência e desconhecido

- **FATO:** o enum DX9 define os três modos e `CMPModelEff::FrameMove` despacha cada um ao controlador correspondente.
- **FATO:** todos os `.eff` encontrados pelo audit atual foram decodificados estruturalmente; não há falha para classificar como recurso morto.
- **INFERÊNCIA:** a escolha de UV por `part.type` no GL corresponde às pistas chamadas pelo DX9 para os tipos 1–3; os dados do `.eff` são armazenados sem converter coordenadas de D3D para um estado D3D, e continuam alimentando atributos UV do renderer OpenGL.
- **FATO do audit T posterior:** nos 568 `.eff` únicos referenciados por 875 sistemas type 6, os 973 subefeitos com tipo 1–3 usam somente os sete modelos primitivos nativos reconhecidos pelo porte; nenhum é `.lgo` ou outro modelo. O original resolve esses nomes pela malha nativa ou `CreateTob`; `CEffectModel::IsItem()` retorna falso quando existe `_lpVB`/`_lwMesh` (`arquivo/fontes/Engine/src/I_Effect.cpp:238-275`; `arquivo/fontes/Engine/include/I_Effect.h:456-462`; `arquivo/fontes/Engine/src/MPResManger.cpp:1086-1135`). Assim, o gate `!IsItem()` dos tipos 2/3 é verdadeiro para esses dados Client. O formato ainda não serializa o resultado de carregamento para outros clientes/modelos.
- **DESCONHECIDO:** a equivalência visual não foi comparada com o DX9; não se declara paridade visual.
- **Medição posterior:** dos 973 subefeitos, 2 `FRAMETEX`, 4 `MODELUV` e 367 `MODELTEXTURE` têm mais de uma entrada animada; os outros 600 têm no máximo uma entrada. O alcance por dependências foi 2, 0 e 34 skills, respectivamente, segundo o grafo atual de dependências.

## Testes executados

- Configuração e build em Debug; `top_gl` compilou.
- CTest completo: **121/121** aprovados.
- Teste sintético de clocks compilado e executado com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`), sem diagnóstico.
- Testes S verificam igualdade no limite, comparação estrita, descarte do tempo excedente, wrap, interpolação e seleção da textura através do `EffectSystem`.
- Audit Client final: `failures=0`; sem Client ausente nesta execução.
- Sem comparação visual DX9/OpenGL; não declarar validado ou fiel.

## Como experimentar visualmente

Na raiz do projeto, reinicie o cliente OpenGL usando as opções históricas que já emprega e acrescente:

```bash
TOP_HISTORICAL_STRIP_EFF_UV_CLOCK=1 \
TOP_HISTORICAL_STRIP_EFF_FRAME_STEP=1 \
./build/top_gl --profile
```

Compare uma habilidade STRIP animada com o executável DX9, observando textura, atlas e movimento de UV em vários FPS. Para voltar ao default, reinicie sem `TOP_HISTORICAL_STRIP_EFF_UV_CLOCK` (e remova também `TOP_HISTORICAL_STRIP_EFF_FRAME_STEP` se quiser desligar R). Para reverter apenas esta etapa: `git apply -R SK217S.patch`.
