# Tales of Pirates — Migracao 05-I: tracks estruturados de .eff

Base: 05-H, compilada no Linux e confirmada com 13/13 testes pelo usuario.
O cliente permanece C++17/OpenGL offline. Nao altera UI, inventario,
combate, NPCs, rede ou arquivos de save.

## O que realmente foi portado

1. `LegacyEffFormat.h` decodifica o layout **declarado pelo codigo original**
   `CMPResManger::LoadEffectFromFile()` e `I_Effect::LoadFromFile()` presente
   em `Antigo(1).zip` (MindPower3D): cabecalho com 95 bytes, tecnica,
   caminho, som, rotacao e contador de subefeitos; para cada subefeito,
   tipo, blending DX, comprimento, tempos, escala XYZ, rotacao XYZ,
   translacao XYZ, cor RGBA, contadores UV, textura, modelo, flag
   billboard e sufixos condicionais por versao.
2. Decodificacao `uint16`, `uint32` e floats little endian, com limites
   explicitos de arquivos, subefeitos, quadros e contadores de textura/UV.
   Strings de 32 bytes devem ser terminadas em NUL. Leitura falha inteira
   se encontra campos incompletos, NaN/inf ou bytes restantes nao explicados.
   Fallback mantido para variantes binariamente desconhecidas.
3. O inspetor `top_eff_inspect` agora diferencia **cabecalho reconhecido**,
   **subefeitos totalmente validados** e **ASCII nao confirmado**.
   Residuos como `.eff` e `ff0070` deixam de aparecer como referencias
   interpretadas. O resultado historico `+95 hpushf02.eff` e consistente
   com a posicao do primeiro subefeito apos o cabecalho de 95 bytes.
4. O `EffectSystem` guarda em cache os tracks de `.eff` estruturados
   com a respectiva textura historica. Somente subefeitos marcados como
   **billboard**, com textura resolvida, sao renderizados diretamente.
   Interpola tempo, escala, translacao, cor, alpha e rotacao Z. O sistema
   converte os eixos MindPower `(x, y-terreno, z-altura)` para OpenGL
   `(x, altura, z-terreno)`.
5. Se a textura nao existe, o formato nao e interpretado por completo,
   ou a geometria exige modelo DX, o fallback das versoes anteriores
   continua. Nenhum arquivo do Client e modificado.
6. `UI05ITests.cpp`: fixture .eff versao 6 no layout historico,
   keyframe intermediario nao linear, decodificacao integral,
   truncamento com fallback e renderizacao dos dados na malha OpenGL.

## Limites e validacao externa necessaria

* Na conversa, `Client.zip` traz tabelas e scripts, mas **nao** fornece
  os .eff binarios reais. O usuario rodou `top_eff_inspect` no Linux com
  `hpushf04.eff` (564 bytes, versao 6) e detectou `hpushf02.eff` no
  offset 95. Sem amostra binaria desse .eff, nao e possivel afirmar que
  todos os subefeitos reais ja se encaixam perfeitamente neste parser.
* Nao ha conversao dos vertices, malhas, shaders, blending DirectX ou
  orientacao 3D completos. Apenas subefeitos `billboard` sao desenhados
  com tracks originais, sob as convencoes conhecidas no porte.
* O arquivo .eff pode possuir formatos de outras builds historicas;
  o inspetor indica quando a estrutura nao foi totalmente validada.
* O teste 05-I usa um arquivo sintetico construido com o layout da rotina
  de leitura original. Necessita validacao visual com `Client/effect/`
  completo no Linux.
* A compilacao integral `top_gl` nao foi executada neste ambiente:
  faltam GL/gl.h e GL/glx.h. Os testes independentes e `main.cpp`
  passaram compilacao C++17 e as 14 suites passaram via CTest.

## Como validar no Linux

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

Proximo objetivo para 05-J: comparar o diagnostico dos arquivos .eff reais
com as estruturas I_Effect, suportar UV/modelos de efeito historicos e
recuperar blending/animacao de texturas onde for possivel validar.
