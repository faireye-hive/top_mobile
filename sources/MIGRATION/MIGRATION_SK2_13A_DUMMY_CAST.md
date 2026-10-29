# SK2.13-A — ligação experimental de efeitos de conjuração aos dummies do personagem

## Base e escopo

Patch incremental para a base **SK2.12**, reconstruída nesta auditoria aplicando sucessivamente SK2.6, SK2.6.1, SK2.7, SK2.8, SK2.9, SK2.10, SK2.11 e SK2.12 sobre `sources(3).zip`.

O DX9 `EffectObj.cpp::Part_bind` usa `GetObjDummyRunTimeMatrix` / `BindingBone` para efeitos ligados ao personagem. O OpenGL já interpreta `.lab` e mantém as matrizes de skinning dos quadros que efetivamente desenha, mas perdia o campo `SkillVisualChannel::dummy` ao emitir os canais cast.

## Implementação

- `ActorSkin::playerDummyWorldMatrices()`: compõe `dummyMatrixFromPose` com a matriz de mundo do personagem, **depois** do mesmo `ActorSkin::build` que desenha o quadro, retornando transformações DX9 row-vector XYZ. Reutiliza as matrizes do skinning/arma e ignora dummies inválidos e modo barco.
- `EffectSystem`: preserva o ID do dummy para canais **Actor** de conjuração (não item, não alvo, não projétil); atualiza a origem da instância em cada quadro usando a **translação** da matriz do dummy. Mantém o antigo posicionamento quando o dummy não existe. IDs duplicados em dummies diferentes não são colapsados.
- `Platform/linux/main.cpp`: assinala origem do jogador apenas para casts disparados de suas coordenadas; lê o snapshot de dummies depois de `actors.build`; por padrão fica DESATIVADO, sem modificar efeitos existentes.
- A transformação guarda a matriz 4x4 completa, mas nesta fase **só a translação alimenta a renderização dos casts**. Rotação/escala total, dummies de item, projéteis e efeitos de monstros aguardam uma fase específica.

## Instalação

```bash
git apply --check tales_SK2_13A_dummies_cast_personagem_RAIZ.patch
git apply tales_SK2_13A_dummies_cast_personagem_RAIZ.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Comparação

**Sem alterar o padrão:**

```bash
TOP_HISTORICAL_DUMMY_BIND=0 ./build/top_gl --profile
```

**Ligações experimentais do jogador:**

```bash
TOP_HISTORICAL_DUMMY_BIND=1 TOP_TRACE_SKILL_FX=1 ./build/top_gl --profile
```

Compare especialmente o efeito corporal de Berserk (skill 84; declara dummy 2), depois Taunt e habilidades de outras classes. O cast acompanha o dummy enquanto estiver ativo. Se faltar a matriz, permanece o antigo fallback. Não muda os fatores de blend ou as curvas.

## Validação e limitações

Executados aqui:

- `top_ui05sk212_tests`: PASS. Fixture sintética com skill 506 e dummy 2, acompanha deslocamento em dois frames; canal que não pertence ao jogador não recebe o dummy.
- `top_actor_skill_pose_tests`: PASS. O dummy 6 acompanha os mesmos frames e posições (3 e 4) da espada que é desenhada no teste existente.
- `git apply --check` sobre cópia limpa da base SK2.12 reconstruída: PASS.

**Não foi executada a compilação GL completa** neste ambiente por ausência de `GL/gl.h`. Uma tentativa de compilar toda a suíte foi interrompida por limite de tempo, não por erro de compilação; somente os dois testes acima estão comprovadamente aprovados neste ambiente. Não há validação visual DX9 desta fase.

## Pendências prioritárias

1. Aplicar orientação completa dos dummies, inclusive dummies de itens e acompanhamento de projéteis, sem girar billboards incorretamente.
2. Interpretar `CMPStrip` diretamente no controlador `.par`, principalmente `swordlight02.par` (skill 81).
3. Corrigir nascimento, vida e término dos pools de partículas com base no DX9.
4. Medir com quadros equivalentes o crescimento percebido da meia-lua do Illusion Slash; `Part_fshade` não declara crescimento linear próximo ao alvo. **SK2.13-A não resolve esse crescimento.**
