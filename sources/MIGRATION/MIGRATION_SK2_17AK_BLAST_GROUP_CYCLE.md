# SK2.17-AK — ciclo de grupo do BLAST

## Lacuna medida

O FX audit executado no Client atual encontra **423 sistemas BLAST (`type=3`)**, em **118 `.par`**, alcançados estaticamente por **101 skills**. O ranking os classifica como aproximados; a etapa O corrigiu o termo de aceleração, mas ainda derivava a vida/geração de cada slot separadamente.

## Original DX9 × porte OpenGL × mudança

| Original DX9 | Porte OpenGL antes de AK | Mudança AK |
|---|---|---|
| `_CreateBlast` inicializa todos os slots, sorteia a vida individual e zera `_wDeath` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3844-3877`). | Com pool histórico ativo, `historical_particle_pool::at` derivava vida e geração por slot a partir da idade do evento (`sources/Engine/src/top/EffectSystem.cpp:2133-2141`). | `TOP_HISTORICAL_BLAST_GROUP_CYCLE` guarda o quadro, tempo, vida, idade e geração por slot e reinicia o grupo apenas quando todos morrem (`sources/Engine/include/top/HistoricalBlastGroupTimeline.h:23-90`). |
| `GetCurFrame` avança no máximo um quadro, descarta o excedente e marca o slot morto ao alcançar `wTotalFrame` (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). | A amostragem analítica podia reciclar cada slot independentemente de quantos outros permaneciam vivos. | A nova timeline usa `dt` de cada `EffectSystem::step`, descarta excedente e mantém os slots já mortos até a morte do grupo (`EffectSystem.cpp:1113-1123`; helper `:53-90`). |
| `_FrameMoveBlast` incrementa `_wDeath`; só quando atinge `_iParNum` chama `_CreateBlast` para todos, pois o caminho `CMagicCtrl` começa os controladores com `Play(0)` (`MPParticleSys.cpp:3891-3909`; `arquivo/fontes/Engine/src/MPEffectCtrl.cpp:545-548`). Após o reset, os índices seguintes ainda passam pelo laço corrente. | O fallback permitia que slots com vida menor reiniciassem antes dos demais. | A opção é opt-in e usa a política de loop comprovada para esse caminho. Ao reiniciar, a timeline também aplica o mesmo `dt` aos índices posteriores no laço corrente. O renderer continua OpenGL; nenhuma matriz, winding ou regra de blend D3D foi copiada. |

## Fatos, inferências e desconhecidos

- **FATO:** o código DX9 conta slots mortos e reinicia todo o grupo somente em `_wDeath == _iParNum`. `GetCurFrame` descarta o excesso de `dt` ao atravessar uma chave.
- **FATO:** `CMagicCtrl::Play` chama `.Play(0)` para cada `CMPPartCtrl`, ativando `_bLoop` em `CMPPartSys::Play` para esse caminho. As chamadas de outras categorias de efeitos não foram provadas por este teste.
- **INFERÊNCIA:** a timeline reproduz a ordem de slots visível em `_FrameMoveBlast`, mas a vida sorteada usa o PRNG determinístico do porte, não o fluxo global `rand()` do DX9.
- **DESCONHECIDO:** sequência aleatória original, cadência real de cada sessão, demais chamadores de BLAST fora do caminho `CMagicCtrl` e paridade visual. Sem comparação com o Client DX9, BLAST continua **aproximado**, não validado.

## `.eff` estritos recusados: utilidade no Client atual

A auditoria encontrou 31 falhas estritas; as 31 foram estruturalmente recuperadas pelo parser diagnóstico. **Trinta** são referenciadas diretamente por `.par` e têm IDs `sceneffectinfo.txt`. `effect/fboomerff.eff` não tem link direto PAR→EFF nem entrada própria identificada; existe `fboomerff.par`, ligado à cena 919/skill 308, mas os sistemas desse PAR usam `boom.lgo` e `RectPlane`, sem referência ao EFF irmão. `Client/tree.log`, `Client/treee.log` e os pacotes ZIP listam os dois arquivos, o que prova distribuição no Client, não execução. Isso é evidência de que o arquivo não é dependência direta dessa habilidade, não prova de que nenhum consumidor dinâmico o peça: o DX9 enumera globalmente os `.eff` (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Portanto, não removi o arquivo nem o decoder; não há motivo medido para implementar suporte específico adicional a `fboomerff.eff` sem uma chamada runtime que o solicite.

## Auditoria e testes

O novo item `blast_group_cycle` no FX audit deriva o conjunto de `type=3` dos `.par`; ele não mantém uma lista manual de assets. A fixture de parser verifica que o feature é contado para BLAST e não para BLAST2.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217ak-fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

Execução observada: build Debug incluindo `top_gl` terminou com sucesso; CTest passou **133/133**. As fixtures unitária e de integração também passaram com ASan/UBSan (`detect_leaks=0`). A integração usa `.par`/`.eff` sintéticos, ativa o pool e o movimento, e mede a mudança de centroide após ciclos de grupo. A auditoria Client registrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes EFF, 27 lacunas, `drift=0`, falhas estruturais `0`, 31 recusas estritas e 31 recuperações diagnósticas.

## Ativação e reversão

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_BLAST_GROUP_CYCLE=1 \
./build/top_gl --profile
```

`TOP_HISTORICAL_BLAST_GROUP_CYCLE` vem desligada por padrão. Remova-a ou defina `0` e reinicie para desligar. A comparação recomendada mantém as duas opções de pool/movimento iguais e varia apenas o switch novo. Reversão: `git apply -R SK217AK.patch`.
