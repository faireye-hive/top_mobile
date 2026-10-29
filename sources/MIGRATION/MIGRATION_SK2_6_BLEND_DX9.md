# SK2.6 — Comparação experimental de fatores históricos DX9 → OpenGL

> **Atualização (SK2.11):** os fatores históricos agora são o padrão; `TOP_HISTORICAL_BLEND=0` volta ao modo legado para comparação, e o alfa do destino fica opaco por padrão (`TOP_HISTORICAL_DEST_ALPHA=framebuffer` restaura o caminho RGBA). Os trechos abaixo que dizem "padrão = renderização atual" descrevem o estado do SK2.6.

Base: fontes `sources(3).zip` enviados em 24/10/2026, **já contendo SK2.5 e blend-audit**. O patch modifica somente o tradutor de fatores e sua passagem para o dispositivo OpenGL. Ele não muda as tabelas, escalas, texturas, alcance, combate, temporização de projéteis, trajetória ou o critério de emissão do Result.

## Problema corrigido

O modo atual usa apenas dois `glBlendFunc`: aditivo `(GL_SRC_ALPHA, GL_ONE)` e convencional `(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`. Os dados DX9 têm outros fatores. Em particular, D3DBLEND 6 é `INVSRCALPHA`, mas **7 é `DESTALPHA`**, não o inverso do alfa de origem. Antes, os efeitos `.eff` com `(5,7)` eram desenhados como `(5,6)`, e os `.par` descartavam totalmente o par salvo no binário.

## Como comparar

Rode duas vezes, com o mesmo personagem, posição, iluminação e habilidade:

```sh
# Renderização atual (padrão; o patch não muda o modo padrão)
./build/top_gl --profile

# Experimento: todos os efeitos históricos, inclusive .par e .eff aninhados
TOP_HISTORICAL_BLEND=1 TOP_TRACE_SKILL_FX=1 ./build/top_gl --profile 2>&1 | tee sk26-historico.log
```

Os logs no modo experimental incluem `[blend-sk26] historical DX9 factors ON | GL_ALPHA_BITS=N`. Esse valor informa a disponibilidade real de alfa no framebuffer. O GLX pede 8 bits de alfa, mas a implementação consulta quantos bits recebeu. Uma cena com alpha do destino distinto pode fazer `(5,7)` parecer muito diferente de `(5,6)`. O modo experimental serve à comparação **não garante fidelidade total ao DX9**, pois o tratamento histórico da escrita no canal alfa, ordenação e estados por shader ainda precisa de validação com o executável original.

Opcionalmente, para congelar a ampliação artificial experimental do Frozen Arrow no tamanho padrão:

```sh
TOP_HISTORICAL_BLEND=1 TOP_FROZEN_ARROW_FX_SCALE=1 ./build/top_gl --profile
```

## Fatores suportados

A enumeração D3DBLEND tem os valores 1..11, respectivamente:

`ZERO`, `ONE`, `SRCCOLOR`, `INVSRCCOLOR`, `SRCALPHA`, `INVSRCALPHA`, `DESTALPHA`, `INVDESTALPHA`, `DESTCOLOR`, `INVDESTCOLOR`, `SRCALPHASAT`.

Eles são convertidos para `GL_ZERO`, `GL_ONE`, `GL_SRC_COLOR`, `GL_ONE_MINUS_SRC_COLOR`, `GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA`, `GL_DST_ALPHA`, `GL_ONE_MINUS_DST_ALPHA`, `GL_DST_COLOR`, `GL_ONE_MINUS_DST_COLOR`, `GL_SRC_ALPHA_SATURATE`. `SRCALPHASAT` é aceito **somente como fator de origem**. Fatores inválidos e zero/desconhecidos mantêm o renderizador legado.

Principais casos da auditoria:

| Par DX9 | Fatores OpenGL | Estado anterior | Estado experimental |
|---|---|---|---|
| (5,2) | GL_SRC_ALPHA / GL_ONE | aditivo (correto) | mesma combinação |
| (5,6) | GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA | aditivo (incorreto) | alfa convencional |
| (5,7) | GL_SRC_ALPHA / GL_DST_ALPHA | alfa convencional (incorreto) | depende do alfa do framebuffer |
| (7,6) | GL_DST_ALPHA / GL_ONE_MINUS_SRC_ALPHA | aditivo (incorreto) | fatores DX9 reais |

## Escopo técnico

- `Engine/include/top/HistoricalBlend.h`: tradutor sem dependência de OpenGL, testável headless.
- `EffectSystem.h/.cpp`: preserva os dois `int` serializados em `.par`, em ordem de origem/destino. Para `.eff`, utiliza `Subeffect.sourceBlend/destBlend`; para `STRIP`/`MODEL` aninhados, preserva o blend do modelo interno `.eff`. No modo histórico, o agrupamento de triângulos respeita também o par de fatores, evitando que modelos com a mesma textura mas combinações diferentes acabem no mesmo lote.
- `GfxDevice.h`, `GfxDeviceGL.cpp`: `draw(...)` recebe o par como argumentos opcionais, que só são fornecidos para lotes de efeitos. Os passes de UI, terreno e atores não mudam.
- `Platform/linux/main.cpp`: transmite o par do lote para a chamada de desenho.
- `UI05GTests.cpp` e `UI05SK26Tests.cpp`: asseguram defaults preservados, tradução 1..11 e propagação de `.eff` e `.par`.

## Testar

```sh
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

O ambiente onde foi construído o patch não dispõe de `GL/gl.h` e `libGL.so` para executar o cliente gráfico. Os testes headless passaram, e `GfxDeviceGL.cpp` foi verificado com cabeçalhos GL temporários apenas para sintaxe. **A compilação/execução GL real e a comparação visual precisam ocorrer no Ubuntu do usuário.**

## Como avaliar

Compare em três skills sem mudar tamanhos: Illusion Slash (81, voo/impacto), Frozen Arrow (93, voo/`Result`) e Dual Shot (90, 3 projéteis/impacto). Se os efeitos ficarem saturados ou invisíveis no modo histórico, capture o log com `GL_ALPHA_BITS` e compare prints da mesma cena. Não aumente escalas para compensar transparência.
