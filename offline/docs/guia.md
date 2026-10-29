# Guia do porte Linux/OpenGL

Atualizado em 23/10/2026.

O diagnóstico, a estimativa de conclusão, as pendências, os modelos recomendados e o roteiro estão em [Estado do porte](../../sources/ESTADO_DO_PORTE.md). Esse é o documento principal de planejamento; os registros MIGRATION descrevem etapas anteriores.

## Compilar e verificar

Executar a partir da raiz do repositório:

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/top_gl --check
./build/top_gl
```

A build depende do ambiente C++/CMake e das bibliotecas Linux usadas em sources/CMakeLists.txt, incluindo OpenGL, X11 e zlib. O modo gráfico precisa de uma sessão gráfica compatível com GLX/OpenGL 3.3. O check não substitui abrir e jogar.

A build validada nesta revisão tem 71 testes. Não interpretar esse número como porcentagem de conclusão. Antes de validar Release, garantir que os testes que usam assert continuem executando suas verificações.

## Onde fica cada parte

- sources/CMakeLists.txt: lista exata do código compilado.
- sources/Engine/src/top e src/top/play: integração atual de mundo, atores e gameplay.
- sources/Engine/src/item, mission e npc: domínios já separados.
- sources/Engine/ui: telas e composição de interface.
- sources/Engine/src/gl: backend OpenGL e janela GLX.
- sources/Platform/common/GameMain.cpp: inicialização e loop compartilhados entre Linux e web.
- sources/Platform/linux/main.cpp: entrada do executável Linux.
- sources/Platform/web/: adaptadores para navegador; [guia web](../web/README.md).
- Client/: recursos usados pelo porte; preservar Client/user/ e os saves.
- arquivo/fontes e arquivo/servidor: referência original de engine, cliente, servidor e regras.
- sources/tests/: regressões.
- sources/auditoria-2026-10-23/revisao-atual-*: resultados da análise atual.

Não extrair um ZIP antigo por cima do diretório atual. O estado local já inclui melhorias posteriores às versões dos pacotes históricos.

## Auditoria de recursos

```bash
./build/top_asset_audit --client Client --json /tmp/top-assets.json --csv /tmp/top-assets.csv --scene-impact /tmp/top-impact.csv
```

A auditoria informa faltas e referências de recursos; não comprova renderização correta nem equivalência de regras Lua.

## Referências

- [Estado do porte e prioridades](../../sources/ESTADO_DO_PORTE.md).
- [Formato do mapa](formato-mapa.md).
- [Referências e histórico em sources](../../sources/README.md).

Os relatórios e roteiros superados foram removidos. Fontes originais e recursos continuam preservados para comparação.
