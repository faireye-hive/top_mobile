# Tales of Pirates — Migração 05-K (geometria procedural MindPower)

Base: versão 05-J. Cliente offline C++17/OpenGL, sem modificações na UI ou rede.

## Origem histórica

O código antigo inclui `CMPResManger::LoadTotalMesh()` e as rotinas
`CEffectModel::CreateTriangle()`, `CreatePlaneTriangle()`, `CreateRect()`,
`CreatePlaneRect()`, `CreateRectZ()`, `CreateCone()` e `CreateCylinder()` em
`Engine/src/I_Effect.cpp`. Essas são **primitivas geradas pelo motor**, não
arquivos `.lgo`. A leitura estrutural de `I_Effect::LoadFromFile()` já existia
na versão 05-I.

## Implementado

- `LegacyEffectGeometry.h`: triângulos OpenGL para os sete modelos originais:
  `Triangle`, `TrianglePlane`, `Rect`, `RectPlane`, `RectZ`, `Cone`, `Cylinder`.
- Transformação dos vértices locais por tamanho, rotação e posição com os
  keyframes `I_Effect`, incluindo conversão dos eixos MindPower Z-up →
  mundo OpenGL Y-up. Mesma escala visual-base da versão 05-I (`0.3`).
- Preservação de `ModelParams` originais (`segments`, `height`, `topRadius`,
  `bottomRadius`) e dos quadros de parâmetros versionados. Para cone e cilindro,
  os raios e a altura são interpolados; a quantidade de segmentos troca
  discretamente para evitar acesso a vértices de outra topologia.
- Preservação de sequências UV para modelos com 3 a 128 vértices, além
  das trilhas de 4 vértices dos billboards da 05-J. UVs incompatíveis usam
  valores iniciais definidos pelo próprio primitivo; atlas/texturas animadas
  seguem as regras da etapa 05-J.
- Somente nomes exatos de primitivas conhecidas e parâmetros geométricos
  seguros são aceitos. Arquivos `.lgo` externos e nomes desconhecidos não são
  convertidos em geometria inventada.
- `EffectSystem` admite um segundo grupo de trilhas gráficas por efeito:
  `billboards` e `nativeMeshes`, resolvidos sob demanda pelo mesmo catálogo.
- `top_eff_inspect` sinaliza `mesh-nativo=1` e conta os quadros de parâmetros.

## O que ainda falta

- Geometria externa de `model/effect/*.lgo`, animação óssea do modelo externo,
  blending DirectX completo e recursos especiais de vertex shader.
- Cone/cilindro são aproximados por triângulos de um strip do original;
  a lógica de morphing mais complexa do original requer comparação visual.
- O efeito `hpushf04.eff` demonstrado no Linux contém `RectPlane` marcado
  como **billboard** e, portanto, continuará no caminho anterior. Para avaliar
  a nova renderização, é necessário um efeito real não-billboard com um dos
  nomes primitivos conhecidos e com textura existente.
- Não foi realizado teste de janela OpenGL/GLX neste ambiente (faltam
  cabeçalhos `GL/gl.h`/`GL/glx.h`). Compile e valide no Linux.

## Testes

`ui05k_native_effect_primitives` verifica sete topologias, parâmetros
inválidos, curvas UV, transformações, frames de cone, recusa de `.lgo`,
truncamento binário e integração efetiva com `EffectSystem` (comparação
quantitativa de triângulos `Rect`/`Cone` versus modelo desconhecido).

A regressão completa `ctest`: **16/16 testes aprovados** nesta execução.

```sh
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
./build/top_eff_inspect Client/effect/hpushf04.eff
```

Identificador: `top_gl offline - migracao 05-K (malhas primitivas MindPower; bolsa 24-48)`.

## Próxima etapa sugerida

Modelos de efeitos externos `.lgo`: identificar como o cliente antigo os
carrega e quais formatos de `ModelFile` já são compatíveis com OpenGL,
sem reimplementar funcionalidade existente. Manter cliente offline.
