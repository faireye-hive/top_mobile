# SK2.16-D — Textura historica dos modelos de efeito `.eff`/`.lgo`

**Base:** SK2.16-C. **Estado:** patch experimental, nao certificado visualmente.

## Diagnostico reproduzivel

No log do Frozen Arrow, as seis camadas `icearrowh.eff` geram 24 triangulos por camada, com alfa de vertice chegando a 1.0 e elevacao acima do terreno, mas a textura enviada em todas as camadas e `Client/texture/effect/1.tga.png`.

O arquivo historico `icearrowh.eff` declara `texture=ice1` em todos os seis subefeitos (modelo `ice.lgo`, nenhuma textura animada). Em `texture_effect.zip`, `effect/1.tga.png` tem 32x32 e canais R/G/B completamente pretos; `effect/Ice1.TGA.png` tem 64x64 e pixels azuis. Isso explica a invisibilidade da geometria aditiva mesmo com alfa de vertice elevado.

O renderizador DX9 `MPModelEff.cpp::RenderVS` aplica `m_pCEffect->SetTexture()` antes de desenhar o subefeito. O porte escolhia primeiro a textura do material `.lgo` e so depois a do `.eff`, quando o material estava ausente. Este patch permite testar a precedencia historica de forma generica, sem troca fixa de nomes por skill.

## Mudancas

- `TOP_HISTORICAL_MODEL_TEXTURE=1`: escolha *quadro animado de textura `.eff`* > *textura declarada no `.eff`* > *textura do material `.lgo`*; usa a mesma regra em `.eff` externos diretos e aninhados em `.par`.
- Desativado ou `=0`: precedencia antiga, para comparar e identificar regressoes.
- `TexturePaths.cpp` resolve variantes maiusculas/minusculas em arquivos PNG de `texture/effect/` **apenas** quando as resolucoes exata e pelo manifesto falham. Exige correspondencia exata de nome ignorando caixa; colisoes ambiguas nao sao escolhidas.
- Logs `[frozen-geometry]` agora mostram `texture_source=eff|frame|lgo`, `eff_tex` e `material_tex`.

A geometria, escala, ciclo de vida, duracao, blend e posicionamento do impacto nao foram alterados. O impacto Illusion Slash 2.5x, Taunt e Berserk nao recebem tratamentos especiais.

## Instalar

Da raiz do projeto, com SK2.16-C aplicada:

```bash
git apply --check SK216D.patch && git apply SK216D.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R 'ui05sk216d_effect_texture_precedence' --output-on-failure
```

## Comparar no Ubuntu

Controle (sem a nova prioridade, mantendo a altura no chao):

```bash
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_MODEL_TEXTURE=0 TOP_FROZEN_RESULT_GROUND=1 \
  TOP_TRACE_FROZEN_GEOMETRY=1 ./build/top_gl --profile 2>&1 | tee frozen-before.log
```

Com prioridade historica:

```bash
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_MODEL_TEXTURE=1 TOP_FROZEN_RESULT_GROUND=1 \
  TOP_TRACE_FROZEN_GEOMETRY=1 ./build/top_gl --profile 2>&1 | tee frozen-eff-texture.log
```

```bash
grep '\[frozen-geometry\]' frozen-eff-texture.log | head -12
```

Esperado **se `ice1` for encontrado**: `texture=.../Ice1.TGA.png texture_source=eff` em vez de `1.tga.png texture_source=lgo`. Isso demonstra a escolha correta do arquivo, mas a confirmacao visual exige executar Frozen Arrow no jogo.

Para isolar o efeito do ancoramento no terreno, repita a segunda execucao com `TOP_FROZEN_RESULT_GROUND=0` e compare.

## Testes executados neste ambiente

- Teste novo `ui05sk216d_effect_texture_precedence`: **aprovado em Debug**; valida a resolucao `ice1` -> `Ice1.TGA.png` no Linux sem manifesto, precedencia DX9, fallback material e prioridade dos quadros animados.
- Regressao selecionada de STRIP, filtros e particulas anteriores: **7/7 testes passaram em Debug** (inclui novo teste).
- As unidades grafica OpenGL e o jogo completo nao foram executados aqui por indisponibilidade das dependencias GL de desenvolvimento e do Client completo.

**Atencao:** Ainda que esse erro explique a invisibilidade observada, so o teste visual confirmara se a representacao do gelo corresponde ao cliente original. Se persistir invisivel com `texture_source=eff`, o proximo diagnostico deve conferir cor de material/vertice, culling e fragments efetivamente desenhados.
