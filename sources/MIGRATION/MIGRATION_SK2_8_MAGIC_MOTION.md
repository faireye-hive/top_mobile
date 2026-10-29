# SK2.8 — Part_fshade histórico (Tales of Pirates DX9 → OpenGL)

**Pré-requisito:** SK2.7 aplicada. O patch SK2.8 é incremental e não reaplica SK2.7.

## Evidência do DirectX original

- `fontes/Client/src/EffectObj.cpp`: o modo 3 de `MagicList` é `Part_fshade`.
- `fontes/Engine/src/MPEffectCtrl.cpp`: `CMagicCtrl` guarda `m_iRenderIdx`
  (coluna 8 de `magicsingleinfo.txt`) e inicia `_iCurSNum=1`.
- `Part_fshade` renderiza o modelo principal, move-se para o alvo e desenha
  cópias com deslocamentos sucessivos de 0,5 unidades **contra a direção**
  da trajetória. A cópia `i` tem alfa `1-(i+1)/count`.
- Após renderizar, chama `CMPModelEff::Scaling(3,3,3)`. Esta operação define
  uma escala **absoluta**, não um multiplicador repetido por frame; o
  primeiro quadro usa escala 1 e os seguintes 3.
- A cada mais de 0,6 s `_iCurSNum` aumenta, permitindo mais cópias.
- Na tabela histórica inspecionada: magic 18 (Illusion Slash) e magic 23
  (Ship) declaram modo 3 e um único arquivo de modelo por magia.

## O que SK2.8 altera

1. `MagicEffectCatalog` preserva o renderMode e a velocidade bruta dos
   registros. Modos ausentes/incorretos permanecem indefinidos (-1).
2. Os modelos de voo `.eff` em modo 3 passam pelo helper
   `HistoricalMagicMotion.h`. Suporta billboards, malhas nativas e externos.
   Os controladores `.par` NÃO recebem cópias artificiais.
3. Mantém os demais modos de movimentação inalterados para minimizar
   regressões até que suas semânticas sejam implementadas.
4. Substitui o aumento fixo `2.75×` do voo do Illusion Slash pela mudança de
   escala absoluta do `Part_fshade` (de 1× inicial para 3× a seguir).
5. O multiplicador do impacto da skill 81 (visual 195) passa a ser **2,5×**
   como ajuste provisório escolhido pelo usuário; ainda pode ser alterado
   por `TOP_ILLUSION_IMPACT_SCALE`.
6. `TOP_TRACE_SKILL_FX=1` emite `[skill-motion]` com ID, modo e velocidade.

## Aplicação e testes

Na raiz do projeto:

```sh
git apply --check tales_05_SK2_8_part_fshade_DX9_RAIZ.patch
git apply tales_05_SK2_8_part_fshade_DX9_RAIZ.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Testar o Illusion Slash

```sh
TOP_HISTORICAL_BLEND=1 TOP_TRACE_SKILL_FX=1 \
TOP_ILLUSION_IMPACT_SCALE=2.5 ./build/top_gl --profile 2>&1 | tee sk28.log

grep '\[skill-motion\]' sk28.log
```

Saída esperada para skill 81, magic 18:

`[skill-motion] skill=81 magic=18 mode=3 speed=15.00 name=Part_fshade`

No DX9, as cópias visíveis aparecem somente quando a viagem dura mais que
0,6 s; com viagens curtas, o efeito pode parecer crescer pela escala que muda
no primeiro frame e por perspectiva durante a aproximação. O modo 3 **não**
descreve uma curva linear de crescimento.

## Limites

- Não inclui ainda parada por proximidade ao alvo, colisão com terreno,
  deslocamento 3D com inclinação, sincronização de dummies e aplicação da
  velocidade histórica 15 unidades/s à duração efetiva do projétil.
- O DX9 também desenha uma cópia com alfa 0; aqui ela é ignorada, o que
  pode divergir em composições dependentes do alfa do framebuffer.
- Permanece a conversão espacial existente de unidades (`0.3`), sujeita à
  comparação visual lado a lado com o original.
- Os testes foram feitos sem renderização GL ou pasta completa `Client/`;
  a validação visual continua necessária no Ubuntu.
