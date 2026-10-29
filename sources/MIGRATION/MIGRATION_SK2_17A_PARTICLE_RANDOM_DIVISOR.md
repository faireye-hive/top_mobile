# SK2.17-A — divisor historico de vida e velocidade das particulas (`_iRoadom`)

**Base:** fontes OpenGL SK2.16-D enviados em `sources_250143.zip`.
**Estado:** mecanismo restaurado para o avaliador EXPERIMENTAL de particulas; sem certificacao visual.

## Evidencia no codigo DX9

`fontes/Engine/src/MPParticleSys.cpp`, `CMPPartSys::Load` e `LoadFromMemory`:

- Para `.par` ate a versao 12, `_iRoadom` e inferido do tipo: **4** para
  `SNOW=1`, `FIRE=2` e `DUMMY=16`; **2** para os demais.
- A partir da versao 13, `_iRoadom` e **lido do arquivo** (inteiro de 32 bits).
- `_CreateSnow`, `_CreateFire` e `_CreateBlast` usam
  `Randf(_fLife / _iRoadom, _fLife)` para sortear vidas individuais.
- `_CreateSnow` usa tambem `Randf(_fVecl / _iRoadom, _fVecl)` para sortear
  o modulo da velocidade de cada componente.

Antes do patch, o parser descartava o inteiro gravado em `.par` v13+
(`skip(4)`); o pool de SK2.16-A e o movimento de SK2.16-B assumiam
sempre divisor 2. Isso muda a curva de vida/movimento para varios recursos,
mesmo que suas texturas, tamanhos e geometria estejam corretos.

## Auditoria de `client_effect(2).zip`

Arquivos `.par`: **1.031**. Foram percorridos os campos relevantes de
**todos os 1.031**; nenhuma falha nesta leitura especifica.
Versoes encontradas: v5 (10), v7 (10), v9 (5), v10 (12), v12 (58),
v13 (85), v14 (152), v15 (699).

No total, **1.089 sistemas** declaram ou inferem um divisor diferente de 2.
Desses, **787 sistemas** pertencem aos tipos que atualmente usam o pool:
- SNOW: **378 / 566** sistemas com divisor diferente de 2;
- FIRE: **197 / 504**;
- BLAST: **212 / 423**.

Existem tambem **4 sistemas v13+ com divisor gravado como zero**;
para evitar divisao por zero, o port usa o valor seguro 2 nesses registros.
Isso e uma salvaguarda do port, nao uma afirmacao sobre a saida do DX9
para dados invalidos.

Essa auditoria mede campos decodificados, **nao** fidelidade visual e nem
quantidade de skills afetadas.

## Alteracoes

- Leitura versionada de `_iRoadom` em `loadParticles()` com seguranca
  para dados invalidos;
- `ParticleProfile::randomDivisor` guarda o valor correspondente a cada `.par`;
- SK2.16-A: faixa de vida individual = `[life/divisor, life]`;
- SK2.16-B: faixa da velocidade SNOW = `[velocity/divisor, velocity]`;
- Ajustados os testes de integracao antigos (v2 tinha divisor 4, nao 2);
- Adicionados testes unitarios para versoes e faixas de amostragem, e
  fixtures de integracao v13 com divisor 1 e 4.

**Nao alterado:** prioridade de textura SK2.16-D, Ice1, escalas, geometria,
blend, filtros, impacto 2,5x do Illusion Slash, dummies, animacoes de pose,
saves, logica offline ou demais tipos de particulas.

## Ativar / desativar no Ubuntu

O patch respeita os switches existentes; ambos permanecem desligados por
padrao. Para comparar a SK2.17-A com os recursos experimentais habilitados:

```bash
TOP_HISTORICAL_MODEL_TEXTURE=1 TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_FILTERS=1 TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 TOP_FROZEN_RESULT_GROUND=1 \
./build/top_gl --profile
```

Para isolar o render anterior de particulas, sem mexer em texturas:

```bash
TOP_HISTORICAL_MODEL_TEXTURE=1 TOP_HISTORICAL_PARTICLE_POOL=0 \
TOP_HISTORICAL_PARTICLE_MOTION=0 ./build/top_gl --profile
```

Os valores do divisor sao extraidos dos assets, nao de um novo switch.
Para voltar exatamente a SK2.16-D, reverta o patch via `git apply -R`.

## Instalar e testar

Na raiz do repositorio onde fica a pasta `sources`:

```bash
# Patch gerado com caminhos relativos a pasta sources/
cd sources
git apply --check ../SK217A.patch
git apply ../SK217A.patch
cd ..
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^ui05sk2(10|11|12|15|16|17)' --output-on-failure
```

**Executado neste ambiente:** 14/14 testes selecionados OK, incluindo
as integracoes de SNOW/FIRE e a regressao SK2.16-D das texturas.
O motor OpenGL completo nao foi compilado/testado aqui por ausencia de
headers/libs GL; confirmar visuais no Ubuntu, sobretudo para efeitos que
possuem SNOW, FIRE ou BLAST (incluindo verificacao de Taunt/Berserk).

**Pendente:** RNG/frame stepping exatos, aceleracoes aleatorias de FIRE/SNOW,
validacao das 31 falhas `.eff` e comparacao visual DX9.
