# SK2.16-C — diagnóstico de visibilidade do gelo histórico

Base: SK2.16-B. **Patch de diagnóstico, não uma correção visual certificada.**

## Evidência inicial

O log da SK2.16-B confirma `icearrowh.eff` com seis subefeitos em `ice.lgo`, mas **carregado != desenhado**. O arquivo histórico `.eff` possui quadros iniciais com alpha=0 e escala Z=0; as seis camadas começam a ganhar opacidade em momentos diferentes. O `.par` adiciona 0,10s de atraso e pára STRIP aos 2,8s.

## Logs de geometria

Execute, no diretório raiz do projeto:

```bash
TOP_HISTORICAL_BLEND=1 TOP_TRACE_FROZEN_GEOMETRY=1 ./build/top_gl --profile 2>&1 | tee frozen-geometry.log
```

Use Frozen Arrow e procure por `[frozen-geometry]`. A cada aproximadamente 0,25s de idade do efeito, para cada camada externa, registra:

- `triangles`: triângulos realmente emitidos por `ice.lgo`, **antes do rasterizador**;
- `rgba_alpha`: faixa de alfa de vértices, após combinação das curvas `.par` e `.eff`;
- `height` e `terrain`: elevação da geometria vs terreno, em unidades do jogo;
- `texture` e `srcdst`: recurso e fatores declarados pelo `.eff`;
- `ground_anchor`: se o modo alternativo de posicionamento está ligado.

`status=invalid_subset`, `status=missing_texture` e `status=finished` são diagnósticos para etapas que impedem emissão. `triangles>0` **não garante pixels visíveis**: profundidade, enquadramento, textura e estado da GPU ainda podem ocultar o resultado.

O log é limitado a 250 combinações de evento/camada/intervalo ao longo do processo. Sem a variável de diagnóstico, não é gerado.

## Comparação A/B: posição no terreno

O resultado histórico está estacionário no alvo, mas o porte pode herdar altura da chegada do projétil. Para testar a hipótese sem modificar outras skills, há um modo opt-in apenas para o resultado mágico 20 (Frozen Arrow):

```bash
TOP_HISTORICAL_BLEND=1 TOP_TRACE_FROZEN_GEOMETRY=1 TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile 2>&1 | tee frozen-ground.log
```

Sem `TOP_FROZEN_RESULT_GROUND=1`, o comportamento anterior é preservado. O modo experimental força o ancoramento do resultado ao terreno do alvo. **Não foi comprovado que essa seja a posição histórica correta.** Não use essa opção como correção definitiva sem comparar imagens ou o código DX9.

Compare os dois arquivos `frozen-geometry.log` e `frozen-ground.log`:

```bash
grep '\[frozen-geometry\]' frozen-geometry.log > frozen-before.txt
grep '\[frozen-geometry\]' frozen-ground.log > frozen-ground.txt
```

## Validação

Compilações e regressões headless são úteis, mas não substituem uma inspeção no OpenGL do usuário. O patch não muda a geometria, timeline, blend ou tamanho das habilidades por padrão. Não altera o crescimento do Illusion Slash ou a escala 2,5× do seu impacto.
