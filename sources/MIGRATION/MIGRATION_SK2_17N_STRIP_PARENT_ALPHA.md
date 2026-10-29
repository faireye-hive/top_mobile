# SK2.17-N — alfa do controlador pai em STRIP aninhado

## Escopo e evidência

Esta etapa corrige uma diferença estreita entre o `PARTTICLE_STRIP` (tipo 6) do DX9 e o render de `.eff` aninhado no OpenGL. A alteração é opt-in por `TOP_HISTORICAL_STRIP_PARENT_ALPHA=1`; sem a variável, a multiplicação RGBA anterior continua ativa.

| Original DX9 | Porte OpenGL antes da etapa | Mudança desta etapa |
|---|---|---|
| `_FrameMoveStrip` cria um único controlador e copia a cor do primeiro quadro do pai para `m_SCurColor` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3173-3186`). | STRIP amostra o primeiro quadro (`sources/Engine/src/top/EffectSystem.cpp:1750-1756`) e entrega a cor do pai aos modelos aninhados. | Preserva-se a cor inicial já usada; nenhuma curva ou ciclo do STRIP muda. |
| No render do modelo filho, quando `m_SCurColor.a < 1`, o DX9 chama `SetAlpha` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1573-1575`). | O GL multiplicava cada canal RGBA do `.eff` pela cor do pai (`EffectSystem.cpp:2014-2019` antes desta etapa). | O switch reproduz o override somente para alfa de tipo 6: alpha filho recebe alpha pai quando este é menor que 1; RGB do filho permanece autorado. |
| `CMPModelEff::SetAlpha` escreve `m_dwCurColor.a = fAlpha` para cada controlador filho (`arquivo/fontes/Engine/include/MPModelEff.h:596-602`). | O GL armazena RGBA por vértice e usa o shader/lote OpenGL existente. | A equivalência D3D→GL ocorre no valor RGBA emitido para os vértices; não há estado de texture stage D3D copiado. O override do DX9 é representado diretamente como alpha de vértice. |

## Auditoria dos dados do Client

O novo `top_strip_alpha_audit` lê `.par` e `.eff` em modo somente leitura. Na execução `build/top_strip_alpha_audit Client/effect`:

- 84 sistemas tipo 6 com alfa do primeiro quadro do pai abaixo de 1 e `.eff` filho;
- 84 `.eff` filhos decodificados estruturalmente;
- 33 sistemas têm uma ou mais partes do filho com algum alfa de quadro diferente de 1;
- 51 não têm divergência de alfa filho detectada;
- os 84 sistemas são referenciados por `sceneffectinfo.txt`; um único sistema é ligado a canais diretos de `skillinfo.txt`;
- 0 `.eff` sem estrutura e 0 `.par` com falha de leitura.

O único sistema ligado diretamente a canais de habilidades é `01020005.par`, sistema 1 → `ice.eff`. `skillinfo.txt` associa esse visual a Fiery Arrow, Inferno Blast, Parry e Dispersion Bullet; seus seis subefeitos começam com alfa 1, então esse sistema não está entre os 33 com divergência de alfa. O cruzamento identifica **zero skills diretamente ligadas aos 33 sistemas divergentes** pelos canais de `SkillVisualCatalog`.

Exemplo de recurso divergente: `Client/effect/01020033.par`, sistema 1, aponta para `ice01.eff`; alfa pai inicial `0.47058827`; seis partes do filho possuem alfa não unitário. A linha de `sceneffectinfo.txt` para visual ID 200 descreve esse `.par` como “Frozen Arrow” (`Client/scripts/table/sceneffectinfo.txt:124`), mas o ID 200 não aparece como canal de habilidade em `skillinfo.txt`. Portanto ele está referenciado na tabela de efeitos, sem vínculo direto demonstrado a uma skill. O audit não prova se algum sistema externo ao catálogo de skills dispara esse efeito durante o jogo.

Os `.eff` anteriormente questionados não têm falhas de decodificação no audit FX atual: `build/sk217m-fx-coverage/fx_decode_failures.csv` contém apenas o cabeçalho e `fx_coverage_summary.json` declara `decode_failures: 0`. Portanto não há recusados que possam ser classificados como não usados. O novo auditor também encontrou zero `.eff` filhos não estruturados no conjunto específico de STRIPs avaliados.

## Fato, inferência e desconhecido

- **Fato:** o DX9 substitui apenas alfa, condicionado a alfa do pai `< 1`, e aplica o valor a cada parte filha (`MPParticleSys.cpp:1573-1575`; `MPModelEff.h:596-602`).
- **Fato:** o porte GL multiplicava RGBA filho e pai no caminho aninhado (`EffectSystem.cpp:2014-2019` na base anterior desta etapa).
- **Fato:** o audit encontrou 33 sistemas type-6 com ao menos uma parte filha e algum quadro cujo alfa não é 1; `01020033.par → ice01.eff` é um exemplo alcançado pela tabela `sceneffectinfo.txt`.
- **Fato:** o cruzamento com os canais decodificados de `skillinfo.txt` não encontrou ligação direta entre esses 33 sistemas e uma skill. O sistema `01020005.par → ice.eff` liga quatro skills, mas seu alfa filho é unitário e já equivale ao override para o alfa medido.
- **Inferência:** nesses casos, multiplicar o alfa filho pelo alfa pai escurece/atenua o resultado em relação ao override DX9. O valor numérico do fixture confirma a diferença: `.4 × .47 = .188` no caminho atual e `.47` no caminho histórico.
- **Desconhecido:** se os 33 efeitos divergentes são disparados por outras tabelas/sistemas que não os canais diretos de `skillinfo.txt`; não houve comparação visual com o cliente DX9. Nenhuma afirmação de fidelidade visual é feita; o resultado está **implementado e testado em fixture, não validado visualmente**.

## Implementação e reversão

Arquivos desta etapa: `Engine/include/top/HistoricalStripAlpha.h`, integração em `Engine/src/top/EffectSystem.cpp`, ferramenta `Tools/strip_alpha_audit.cpp` e testes em `tests/SK217NStripAlpha*Tests.cpp`.

O modo histórico fica desligado por padrão. Use `TOP_HISTORICAL_STRIP_PARENT_ALPHA=1` para ativar; omita a variável ou defina `TOP_HISTORICAL_STRIP_PARENT_ALPHA=0` para desligar. O switch é lido no processo, então reinicie `top_gl` ao alterná-lo.

## Como reproduzir

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_strip_alpha_audit Client/effect > build/sk217n-strip-alpha.csv
```

Comparação visual manual com o DX9, usando o mesmo Client e mesma cena:

```bash
TOP_HISTORICAL_STRIP_PARENT_ALPHA=0 ./build/top_gl --profile
TOP_HISTORICAL_STRIP_PARENT_ALPHA=1 ./build/top_gl --profile
```

Para reverter somente esta etapa: `git apply -R SK217N.patch`.

## Execução desta etapa

Build Debug de `top_gl`, do auditor e dos dois testes novos concluído. CTest completo: **111/111 aprovados**. Auditoria de STRIP: números acima. Não houve teste de comparação visual DX9/OpenGL.
