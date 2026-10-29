# SK2.15-C — Filtros min/mag históricos por sistema `.par`

## Objetivo

`CMPPartSys::Render()` no DirectX 9 aplica os campos serializados `_eMinFilter` e `_eMagFilter` como estados `D3DSAMP_MINFILTER` e `D3DSAMP_MAGFILTER`. O porte OpenGL descartava esses dois inteiros, e `GfxDevice::createTexture()` sempre configurava `GL_LINEAR` nos dois eixos de amostragem.

Esta migração preserva **o par de filtros de cada sistema `.par`**, sem afetar os filtros de cenário, personagens ou UI. O comportamento fica desligado por padrão para permitir comparação A/B.

## Habilitar / comparar

Modo experimental:

```bash
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_FILTERS=1 \
TOP_TRACE_EFFECT_FILTERS=1 ./build/top_gl --profile 2>&1 | tee sk215c-historico.log
```

Modo pré-SK2.15-C:

```bash
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_FILTERS=0 ./build/top_gl --profile
```

O log `[effect-filter]` informa os primeiros pares distintos de min/mag usados. Um par `1/1` significa `GL_NEAREST/GL_NEAREST` (D3DTEXF_POINT); `2/2` significa `GL_LINEAR/GL_LINEAR` (D3DTEXF_LINEAR). Valores 0 ou não suportados continuam com `GL_LINEAR` para aquele eixo.

## Alterações

- `loadParticles` lê e preserva os dois inteiros dos `.par` nos `ParticleProfile`.
- `EffectBatch` carrega filtros próprios; lotes com mesma textura/blend e filtros diferentes não são fundidos.
- A informação é propagada também aos modelos `.eff` aninhados nos sistemas `.par`, sem alterar filtros de `.eff` independentes.
- `GfxDeviceGL` usa quatro **sampler objects** OpenGL (GL 3.3) em vez de modificar `glTexParameteri` no objeto de textura compartilhado. O sampler da unidade 0 é desassociado ao fim de cada draw; UI/terreno seguem com o filtro anterior.
- Se as funções de sampler não estiverem disponíveis, o jogo registra um aviso e utiliza o filtro linear anterior.
- `TOP_HISTORICAL_FILTERS=1` opt-in; por padrão nada muda.
- Não muda geometrias, blend, escala, movimentos, timeline ou as regras de partículas.

## Compilar e testar

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R 'ui05sk21(4|5)' --output-on-failure
```

Teste GPU opcional com contexto OpenGL, em monitor real ou Xvfb:

```bash
./build/top_effect_filter_gpu_tests
# ou
xvfb-run -a ./build/top_effect_filter_gpu_tests
```

Este programa verifica `GL_NEAREST`, `GL_LINEAR` e se a textura retorna à amostragem linear quando um efeito sem filtro explícito é desenhado usando **o mesmo texture ID**. Esse executável exige contexto OpenGL e não é incluído no CTest headless obrigatório.

## Limitações

- Os pares `(1,1)` e `(2,2)` são mapeados diretamente. Valores `NONE`, `ANISOTROPIC`, `PYRAMIDALQUAD`, `GAUSSIANQUAD` ou desconhecidos usam filtragem linear de compatibilidade no eixo correspondente. Não há afirmação de fidelidade para esses modos.
- Os efeitos `.eff` independentes e as trilhas CMPStrip diretas (sem min/mag serializado no formato usado) conservam seus filtros anteriores.
- Sampler minification usa `GL_NEAREST`/`GL_LINEAR` sem mipmaps, como as texturas atualmente carregadas por `createTexture()`.
- A comparação visual com o DX9 original continua pendente. A SK2.15-C não resolve o crescimento da meia-lua do Illusion Slash nem altera o impacto 2,5×.
- Na máquina de desenvolvimento usada para preparar este patch faltam `GL/gl.h` e libGL.so de desenvolvimento; foram executados testes de CPU e análise sintática com cabeçalhos auxiliares, **não** um teste de framebuffer real.

## Resultados de validação na preparação

- `git diff --check`: sem erro.
- Testes CTest selecionados (cobertura SK2.14, CMPStrip SK2.15-A, STRIP SK2.15-B, integração de filtros SK2.15-C, mapeamento SK2.15-C): **5/5 aprovados**, build Debug.
- Sintaxe do `GfxDeviceGL.cpp` e do teste GPU: verificada contra cabeçalhos auxiliares, sem teste gráfico.
