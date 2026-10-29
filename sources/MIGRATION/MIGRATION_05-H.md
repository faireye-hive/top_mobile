# Tales of Pirates — Migracao 05-H: curvas de particulas e inspecao de .eff

Base: 05-G validada no Linux (12 testes). Port offline C++17/OpenGL.
Nao modifica UI, rede, regras de dano, personagens ou saves.

## Entregue

1. `LegacyParticleCurves.h`: interpolacao entre TODOS os keyframes lidos
   do CMPPartCtrl `.par` (tamanho, canais RGBA e rotacao Z). A cor alfa
   passa a vir do track original, em vez de ser sobrescrita por um fade
   artificial. O tamanho da curva NAO define numero de frames UV.
2. `EffectSystem.cpp`: amostra as curvas em `age / profile.life`, produz
   billboards com rotacao e conserva billboards simples quando nao ha
   rotacao. Continua a utilizar as texturas e IDs das etapas 05-F e 05-G.
3. `loadParticles`: recusa NaN/infinito e cabecalhos malformados; so
   publica os sistemas apos ler o controlador por completo. Um arquivo
   `.par` truncado nao pode deixar um sistema de particulas pela metade.
   A textura em cache e reiniciada por `EffectSystem::open()`.
4. `LegacyEffectInspector.h` e alvo CLI `top_eff_inspect`: inspecao
   conservadora do cabecalho (`uint32 little endian`, versao reconhecivel
   por este porte), comprimento e strings ASCII candidatas a recursos,
   com seus offsets. O carregador existente passa a utilizar esse leitor.
   Limites: 8 MiB/arquivo e 64 strings; filtros de nomes rejeitam `../`.
   Observacao: **nao** interpreta objetos CMPModelEff, geometrias,
   transformacoes, shaders ou a hierarquia de um `.eff`.
5. `UI05HTests.cpp`: testes de curva intermediaria NAO linear,
   coloracao/RGBA e rotacao reais em malha, UV integral, arquivo
   invalido/truncado, seguranca de referencias, fallback e reabertura.

## Limitacoes

* `Client.zip` enviado nesta conversa contem somente scripts/tabelas,
  sem os `.eff`/`.par` binarios e texturas completos do jogo.
  Os testes usam fixtures pequenas com layout parcial conhecido;
  nao comprovam equivalencia visual com DirectX9.
* O eixo Z dos tres floats/quadro e tratado como rotacao de billboard em
  radianos. A semantica de todos os canais/versoes de `.par` precisa ser
  validada contra arquivos antigos, pois depende da versao do jogo.
* A geometria e as matrizes de `.eff` **nao foram migradas** nesta fase.
  Para isso, deve-se obter uma amostra `.eff` real e comparar o layout
  com as estruturas originais CMPModelEff de MindPower3D.
* A decodificacao do `.par` continua sendo parcial quanto a emissores
  especiais, blending, texturas animadas e eventos adicionais.

## Validacao no Linux

Na pasta que contem `sources/` e `Client/`:

```sh
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

O `top_gl` deve mostrar `migracao 05-H`.

Para inspecionar um `.eff` real (com `Client/effect/` presente):

```sh
./build/top_eff_inspect Client/effect/hpushf04.eff
```

A listagem representa **strings candidatas**, nao offsets de campos
comprovadamente nomeados pela versao original. Voce pode compartilhar
esse diagnostico ou um exemplo de `.eff` historico para investigar
as transformacoes reais na proxima etapa, sem enviar todo o Client.
