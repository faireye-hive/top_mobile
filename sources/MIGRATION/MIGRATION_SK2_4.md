# SK2.4 — Result histórico separado do voo (C++17 / Linux / OpenGL)

Base exigida: SK2.3 aplicada (inclui SK1, SK2.1, SK2.2). Este patch modifica somente
`EffectSystem`, os testes do efeito e CMake; **não** altera PlayCombat, movimentação,
combate, animações, saves, UI ou os ativos do Client.

## Evidência no cliente DX9

- Em `MPEffectCtrl.cpp`, `CMagicCtrl::Create` lê o campo `Result` e cria um
  controlador separado `_pPartResult` via `GetPartCtrlID(result + ".par")`.
- `CMagicCtrl::Render`, após `_bStop`, renderiza `_pPartResult` até terminar.
- Na implementação OpenGL anterior `resourceFor` acrescentava
  `definition->result` aos emissores em movimento, confundindo voo e resultado.
- `magicsingleinfo.txt` possui 19 registros com `Result` não vazio neste acervo.
  O registro 20 (Frozen Arrow) utiliza `icearrowh.par` como resultado.

## Mudanças

1. O recurso de voo mantém somente os modelos e emissores declarados para voo.
2. Ao cruzar `age >= life` da instância `Flight`, `step()` resolve, uma única
   vez, o controlador `Result` próprio do magicsingle que originou o voo.
3. O resultado é criado na **posição de chegada**, com seu atraso, curvas,
   quantidade e duração. Usa o mesmo leitor de `.par` e o mesmo suporte
   anterior a modelos `.eff` nativos/aninhados.
4. O cache separa o ID do `Result` do ID de cena e do ID do magic
   (`-1000 - magicId`); portanto não confunde `Result icearrowh` com efeitos
   de conjuração/hit do `skillinfo.txt`.
5. Projéteis `magicgroup` já expandidos pela SK2.2 disparam seu `Result`
   individualmente, após cada chegada, inclusive com atrasos de 0,2 s.
6. Uma skill histórica sem canais hit em `skillinfo` não cria uma explosão
   procedural para esconder a ausência de recursos, mas o `Result` real do
   projétil continua possível e independe do acerto do dano (MISS/HIT).
7. Frozen Arrow volta a **escala padrão 1×**. Para comparação, ainda aceita
   `TOP_FROZEN_ARROW_FX_SCALE=1` ou um multiplicador experimental.
8. Nomes numéricos incompletos de `Result` (por exemplo `1030006`)
   tentam primeiro a forma literal e depois `01030006.par`, sem substituir
   referências existentes do próprio mod.
9. `TOP_TRACE_SKILL_FX=1` acrescenta a linha `kind=result` com magic ID,
   arquivo `.par`, número de controladores interpretados e trilhas nativas.

## Instalar

```bash
# executar na raiz do repositório TalesOfPirateDX9

git apply --check tales_05_SK2_4_result_impacto_historico_RAIZ.patch
git apply tales_05_SK2_4_result_impacto_historico_RAIZ.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
TOP_FROZEN_ARROW_FX_SCALE=1 TOP_TRACE_SKILL_FX=1 ./build/top_gl --profile 2>&1 | tee sk24-frozen-arrow.log
```

Depois de disparar Frozen Arrow, conferir algo como:

```text
[skill-fx] skill=93 kind=flight visual=1020 ...
[skill-fx] skill=93 kind=result visual=-1020 magic=20 par=icearrowh systems=3 nested_tracks=N at=(X,Y)
```

`nested_tracks=0` **não** quer dizer que o `Result` não foi iniciado: significa
que o OpenGL ainda não interpretou trilhas nativas desse `.eff`. O modelo
`icearrowh.eff` da auditoria contém seis subefeitos e utiliza `ice.lgo`,
cujo uso aninhado no `.par` **ainda não está completo**; esse é o trabalho
previsto para SK2.5. Outros dois sistemas `FIRE` de `icearrowh.par` já usam o
renderizador de partículas existente, com suas escalas históricas.

## Testes

- `ui05sk24_magic_result_impact`: fixture com magicsingle 20, `Result`
  separado, modelo aninhado, tempos de voo/partículas, chegada e ausência de
  duplicação; inclui um grupo com `Result` numérico (`1030006` →
  `01030006.par`) no segundo projétil.
- `ui05g_skill_channel_composition` contém o mesmo conjunto de fixtures.
- Regressões `ui05f/ui05h/...`, `ui05bi`, `ui05sk1`, `ui05sk23` mantidas.

Aparência final requer validação visual com o Client completo no Ubuntu. A
ferramenta não muda fórmulas de dano nem inventa partículas de impacto.
