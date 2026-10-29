# Migração 04-I — Componente independente de minimapa

Base: sources_com_4-h.zip enviado pelo usuário.

## Alterações
- Novo `Engine/ui/screens/MinimapUI.h/.cpp`: posição do radar, área clicável,
  desenho da moldura/fundo, sprite da imagem e textos de nome/coordenadas.
- `ActorSkinUIBridge.cpp`: usa `MinimapUI` para layout, hit-test e textos.
- `GameHUD.cpp`: delega minimapa a `MinimapUI`, mantendo HP/SP/EXP e atalhos.
- Novo `tests/UI04ITests.cpp` e configuração de CTest.
- Nenhuma alteração na geração de pixels, leitura de `.pk`, combate, rede ou regras offline.

## Validação no ambiente
- `top_ui04i_tests`: aprovado.
- `g++ -std=c++17 -fsyntax-only` dos três `.cpp` alterados: aprovado.
- Build OpenGL completo: não validado; ambiente sem headers GL/X11 e compilação foi interrompida.

## Validar no Linux
```
rm -rf build
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Verificar visualmente minimapa, coordenadas e captura de clique na borda,
especialmente depois de redimensionar a janela e trocar de mapa.
