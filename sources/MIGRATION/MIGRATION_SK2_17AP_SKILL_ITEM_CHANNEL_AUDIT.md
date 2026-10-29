# SK2.17-AP — cruzamento auditável dos canais de item

## Resumo

O FX audit agora gera `fx_skill_item_channels.csv`, cruzando os canais Item/ItemExtra de `skillinfo.txt` com IDs de `sceneffectinfo.txt`, arquivos PAR encontrados e tipos de controladores decodificados. O novo cruzamento corrigiu a conclusão de SK2.17-AO: Fireshield não aciona `01040009.par` pelo canal de item, e nenhum dos 50 canais de item não nulos do Client atual alcança type 16.

Essa etapa altera apenas auditoria e documentação. O controlador experimental `TOP_HISTORICAL_DUMMY_CONTROLLER` permanece opt-in, mas sua utilização com os recursos atuais do Client não está comprovada. Nenhum decoder de `.eff` foi removido ou afrouxado por esta etapa.

## Evidência: original DX9 x porte atual x mudança

| Original DX9 | Porte OpenGL/auditoria antes | Mudança e tradução |
|---|---|---|
| `SkillRecord` lê `sItemEffect1/2` como pares ID/keyframe (`arquivo/fontes/Libraries/common/src/SkillRecord.cpp:391-407`). `STAttack` chama `ItemEffect` apenas se o ID for positivo e o keyframe coincidir (`arquivo/fontes/Client/src/STAttack.cpp:262-263`). | `FxCoverageAudit` já lia `sceneffectinfo` e PARs, mas `fx_skill_gaps.csv` usava `dependencies.csv`; não mostrava diretamente os canais de item (`sources/Engine/include/top/FxCoverageAudit.h:382-410,490-499`). | O auditor lê colunas 54 e 55 zero-based, resolve os IDs para `sceneffectinfo`, então associa nomes de PAR a candidatos encontrados e tipos PAR decodificados (`FxCoverageAudit.h:251-278,486-538`). Isso é uma junção de dados de auditoria, não um comportamento novo do renderer. |
| `sWhoped` é lido depois do canal sky e antes dos campos do alvo (`SkillRecord.cpp:415-424`); no impacto, seu valor é enviado a `PlayEnvSound` (`arquivo/fontes/Client/src/EffDelay.cpp:27`). | O relatório AO havia interpretado o 152 de Fireshield como efeito de item porque `sceneffectinfo` ID 152 aponta a `01040009.par` (`Client/scripts/table/sceneffectinfo.txt:76`). | Corrigimos a associação: a linha de Fireshield tem `sItemEffect1=0,0` e `sItemEffect2=0,0`; 152 está em `sWhoped`. O auditor agora registra canais reais separadamente e não chama a presença em catálogo de prova de execução. |

## Resultado atual do Client

Comando executado:

```bash
./build/top_fx_coverage_audit \
  --client Client \
  --out build/sk217ap-skill-item-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv
```

Saída: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF, 26 gaps, 307 skills com gaps, `drift=0` e zero falhas estruturais. `fx_skill_item_channels.csv` contém 50 canais em 46 skills; nenhum arquivo candidato contém type 16.

Fireshield é `skill_id=0024`/linha 24 de `Client/scripts/table/skillinfo.txt`. Nas colunas 54 e 55 seus valores são `0,0`; coluna 61 (offset zero-based) contém `152`, lida como `sWhoped`. `CHitRepresent::Exec` passa esse campo a `PlayEnvSound` (`EffDelay.cpp:27`). A entrada `sceneffectinfo` 152 e o arquivo type 16 são fatos do inventário, mas não formam ligação entre essa skill e o PAR.

### `.eff` que falharam em strict mode

O mesmo audit registra 31 `.eff` recusados pelo modo estrito; os 31 são estruturados pelo modo diagnóstico. Trinta têm referências diretas por PAR e ID de `sceneffectinfo`; o 31º, `fboomerff.eff`, não tem aresta PAR→EFF direta, embora exista `fboomerff.par` irmão ligado ao catálogo. Cruzando os pais diretos dos 31 com os 50 canais Item/ItemExtra, nenhum `.eff` strict-failure foi alcançado por esses canais.

Isso **não** classifica os arquivos como inúteis: 30 são dependências explícitas de PAR/catálogo. O loader DX9 enumera e indexa os `.eff` da pasta (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`); esse fato prova que o arquivo é carregado pelo inventário, não que todo arquivo seja executado numa sessão. Para `fboomerff.eff`, o uso dinâmico continua desconhecido. A etapa não altera o parser, não descarta recursos e não afirma uso visual em runtime.

## Fatos, inferências e desconhecidos

- **Fato:** 50 canais não nulos cruzam para PARs existentes; a tabela/CSV anota todos os tipos de controller desses PARs.
- **Fato:** nenhum desses 50 canais cruza para type 16 neste Client.
- **Fato:** a skill 24 não declara canais de item e o campo 152 é `sWhoped`, tocado como som de impacto pelo caminho mostrado acima.
- **Inferência:** type 16 pode ainda ser iniciado por código fora dos canais de `skillinfo` analisados; esse caminho não foi localizado por esta auditoria.
- **Desconhecido:** se/como `01040009.par` é efetivamente reproduzido em uma sessão, e se `fboomerff.eff` é solicitado por algum cliente além de ser enumerado no carregamento global.
- **Desconhecido:** paridade visual DX9/OpenGL; não houve comparação visual.

## Testes e reversão

Build executado com `CMAKE_BUILD_TYPE=Debug`; `cmake --build build -j$(nproc)` concluiu com sucesso, incluindo `top_gl`. O fixture `top_fx_coverage_tests` cobre duas colunas de canal, resolução de ID/candidato e tipo PAR; passou. CTest completo: **136/136** aprovados.

O novo relatório é somente auditoria, não precisa de variável runtime. Para reproduzir:

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/top_fx_coverage_audit --client Client --out build/sk217ap-skill-item-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv
```

Patch incremental: `SK217AP.patch` (todos os caminhos do patch começam com `sources/`). Reversão: `git apply -R SK217AP.patch`.
