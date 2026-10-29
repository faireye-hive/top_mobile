# Tales of Pirates — SK2.5 (fase 1): geometria externa aninhada

## Diagnóstico

O log da SK2.4 mostrou `skill=93 kind=result par=icearrowh systems=3 nested_tracks=0`.
O resultado já era disparado na posição do alvo; porém o parser de `EffectSystem::resolveNestedModels()`
rejeitava os subefeitos `.eff` que usavam modelos `.lgo`, aceitando apenas billboards e malhas primitivas.
O `icearrowh.eff` histórico contém subefeitos externos, incluindo `ice.lgo`.

## Correção

- Em controladores `.par` tipo MODEL (5) ou STRIP (6), o motor agora aceita subefeitos `.eff` com
  modelos externos `.lgo`, reutilizando `legacy_external::load()` e `subsetPrimitive()` já existentes.
- Modelos com nome repetido são decodificados apenas uma vez por recurso e compartilhados entre subefeitos.
- O renderizador preserva os keyframes `.eff`, UV, blend reconhecido, cores/material de cada subset,
  textura do subset ou da trilha. Não há escala adicional específica para Frozen Arrow ou outras skills.
- Um modelo ausente/inválido ou textura não resolvida **não** ganha billboard procedural substituto.
- Com `TOP_TRACE_SKILL_FX=1`, aparecem diagnósticos por modelo externo carregado, ausente/incompatível,
  ou trilha sem textura. Isso evita interpretar `nested_tracks=0` como falha do Result.

## Testes

`ui05l_external_effect_lgo` agora também reproduz `.par(type=6)` -> `.eff` -> `.lgo`, com
uma textura sentinela `0` no controlador, e verifica vértices reais no lote de renderização.
Verifica ainda que a falta de `.lgo` não resulta em efeito falso.
A suíte headless de 11 testes de renderização, combate e auditoria passou.

## Verificação no Ubuntu

Aplicar na raiz do repositório, **depois da SK2.4**:

```bash
git apply --check tales_05_SK2_5_lgo_aninhado_par_RAIZ.patch
git apply tales_05_SK2_5_lgo_aninhado_par_RAIZ.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
TOP_TRACE_SKILL_FX=1 ./build/top_gl --profile 2>&1 | tee sk25-effects.log
```

**Esperado ao usar Frozen Arrow**: `kind=result ... systems=3` permanece; se `ice.lgo` e texturas
forem aceitos pelo decoder, aparecerá `nested-eff=icearrowh.eff external=ice.lgo status=loaded`
e `nested_tracks` deverá ser maior que zero. Se aparecer `missing_or_unsupported_lgo`, conferir o arquivo
real e as limitações de `ModelFile` antes de alterar a escala. Esta etapa não foi validada com
`icearrowh.eff`/`ice.lgo` reais nem com o OpenGL gráfico completo.
