# 05-SK2.1 — Renderização histórica: múltiplos modelos e blend por subefeito

Base: 05-SK1 (inclui correções 05-BG a 05-BJ.1). Não modifica `PlayCombat`, `HistoricalSkillExecution`, nem os atalhos/poses das skills.

## Correções

1. `EffectSystem::resourceFor` agora percorre **todos** os modelos `.eff` declarados num registro `magicsingleinfo.txt`, em vez de sair ao reconhecer o primeiro. Continua ignorando arquivos malformados/ausentes e respeitando os limites de 16 billboards, 16 malhas nativas e 8 malhas externas por recurso. A execução histórica original (`CMagicCtrl::Init` e `CMagicCtrl::Render`) já cria e percorre todos os modelos cadastrados.
2. Material `SRC_ALPHA / INV_SRC_ALPHA` de subefeitos `.eff` agora mantém o modo OpenGL 8 tanto nos billboards como nas malhas externas, além das malhas nativas/aninhadas que já o faziam. Outras combinações ainda usam o modo aditivo legado; um mapeamento completo de `D3DBLEND` fica para uma etapa posterior, após auditoria.
3. `UI05GTests` agora cria um efeito com **dois modelos reais** e um nome ausente entre eles; exige ambos os modelos no resultado, com transparências diferentes. Testa também a compatibilidade com o impacto do Illusion Slash.

## Escopo e limites

- 7 dos 53 registros do `magicsingleinfo.txt` do acervo fornecido declaram mais de um modelo. Isso não comprova que todas as skills já vão desenhar corretamente: `.eff/.par` não suportados, dummies, tempo de emissão e orientação ainda precisam de validação individual por mecanismo.
- SK2.1 corrige infraestrutura compartilhada; **não** altera a escala configurada do Illusion Slash, seu alcance de 8 m, a seleção de alvo, a movimentação automática até a distância de disparo ou o impacto 195.
- Suíte headless executada no ambiente de desenvolvimento: 6/6 (`ui05f`, `ui05g`, `ui05k`, `ui05v`, `ui05bi`, `ui05sk1`), com acervo `Client` de testes para `ui05v`.
- O build OpenGL completo e a fidelidade visual de todos os recursos devem ser validados no Ubuntu com `Client` completo.

## Testar no projeto

```sh
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/top_gl --profile
```

Recomendado testar: **Illusion Slash** (regressão), **Frozen Arrow** (múltiplos modelos), além de uma skill com efeito `.par`/modelo vinculado.
