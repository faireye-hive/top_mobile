# SK2.17-AL — Corrigir a classificação do byte SHADE no FX audit

## Resumo

Esta etapa corrige a classificação de um metadado, sem alterar o renderer. O audit contava cada flag `m_bShade` serializada como uma lacuna SHADE, apesar de o DX9 recalcular essa flag pelo tipo do controlador e o OpenGL também selecionar SHADE pelo tipo 13.

No Client auditado, os 8 bytes SHADE presentes correspondem a 8 sistemas type 13; não há divergência de dados. A feature do ranking agora conta somente flags v10+ diferentes de `(type == 13)`, e a lacuna falsa desaparece. SHADE continua parcial por suas diferenças geométricas/visuais já registradas.

## Original DX9 x porte OpenGL x alteração

| Original DX9 | Porte atual | Alteração desta etapa |
|---|---|---|
| `CMPPartSys::SetOpertion` inicializa `m_bShade=false` e só o torna verdadeiro no ramo `PARTTICLE_SHADE` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:361-405`). `LoadFromMemory` lê o byte serializado, mas chama `SetOpertion` logo depois (`MPParticleSys.cpp:4469-4473`). `BindingRes` também chama `SetOpertion` antes do uso dos recursos (`MPParticleSys.cpp:573-590`). | `loadParticles` consome o byte v10 sem armazená-lo (`sources/Engine/src/top/EffectSystem.cpp:225-226`); SHADE usa a seleção por `profile.type == 13` no renderer (`EffectSystem.cpp:2278-2295`). | `LegacyParAudit::System` passa a registrar se a flag existe na versão (`sources/Engine/include/top/LegacyParAudit.h:23-28,130-134`). A feature `shade_type_mismatch` conta somente quando a flag está presente e não coincide com `type == 13` (`sources/Engine/include/top/FxCoverageAudit.h:193-195`). Nenhum comportamento em runtime mudou. |

## Fato, inferência e desconhecido

- **Fato:** no carregamento em memória DX9, o valor salvo em `m_bShade` é substituído pela execução de `SetOpertion`, que deriva o membro do tipo. O caminho OpenGL existente toma a mesma decisão pelo tipo 13.
- **Fato:** no Client atual, as 8 flags SHADE correspondem a 8 sistemas type 13; `shade_type_mismatch=0`. O número de lacunas cai de 27 para 26 sem mudar a cobertura dos outros mecanismos.
- **Inferência:** para os assets deste Client e os caminhos DX9 que chamam `SetOpertion`, a flag serializada é redundante para selecionar o modo SHADE.
- **Desconhecido:** não auditamos todas as versões do editor nem estados temporários entre a leitura e o vínculo dos recursos. Não se deve generalizar os 8 casos a outros Clients sem executar o audit neles.
- **Limite:** isso corrige uma métrica do audit; não melhora a projeção nem valida SHADE visualmente. SHADE continua **aproximado, não validado**.

## Testes e auditoria

O teste de formato cobre um `.par` versão 10 com flag presente e testa as combinações coincidentes/divergentes entre flag e tipo, além de garantir que versões sem a flag não sejam marcadas como conflito.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

build/top_fx_coverage_audit --client Client --out build/sk217al-shade-consistency \
  --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv

git apply --check SK217AL.patch
git apply SK217AL.patch
```

O audit leu 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 partes EFF; encontrou 26 lacunas, `drift=0`, zero falhas estruturais e zero conflitos `shade_type_mismatch`. O build Debug completo, incluindo `top_gl`, terminou; CTest passou **133/133**. A fixture do audit também passou sob ASan/UBSan com `detect_leaks=0`. `SK217AL.patch` passou por `git apply --check` e foi aplicado a uma cópia temporária do estado pós-AK com o parser de audit existente. A comparação visual DX9/OpenGL continua pendente.
