# Tales of Pirates OpenGL offline — migração 05-X3

## Motivação: perfil real da APU Ryzen 5 4600G

O teste do usuário com `05-X2 --profile` demonstrou aproximadamente 24 FPS em Argent City, **stream 26–28 ms**, atores 12 ms, render 2,3–2,5 ms e swap ~0,25 ms. Com cache de geometria ligado, transferências caíram de aproximadamente 24 MiB/quadro para 3,75 MiB/quadro, mas stream continuou sendo o gargalo.

O `SceneWorld::build()` refazia todos os ~1.150 objetos do setor ~15 vezes por segundo, mesmo que só aproximadamente 57 tivessem animação no mapa de referência. Isso recriava dezenas de milhares de triângulos estáticos, recalculava piso e colisão e atualizava os VBOs estáticos repetidamente.

## Mudança

`SceneWorld::Pass` oferece:

- `All`: saída antiga, preservada para compatibilidade e testes.
- `Static`: gera objetos estáticos **uma vez por atualização de região** e mantém a malha de colisão original, incluindo o snapshot dos pisos dos objetos animados.
- `Animated`: gera **somente modelos com matriz, UV, opacidade ou textura animada**, sem reconstruir pisos, paredes ou modelos estáticos.

O resultado `Static` fica em `cachedScene`, enquanto o resultado `Animated` fica em `cachedAnimatedScene`. Os dois respeitam a ordenação de renderização (sombras → opacos → blend). Buffers são invalidados ao trocar de mapa. Esta atualização **não altera** a âncora do terreno 05-X1, a câmera, os sprites do mouse, equipamento, combate nem o save.

A classificação de animação é calculada apenas uma vez por modelo e armazenada no cache. No passo `Animated`, modelos estáticos são ignorados antes dos cálculos de terreno e das matrizes. Isso evita a repetição da maior parte do trabalho de cenário.

## Diagnóstico aprimorado

Execute `./build/top_gl --profile`. Além do perfil anterior, o jogo informa:

```
profile stream detalhado: terreno ... ms | cena estatica ... ms | cena animada ... ms | logica/efeitos ... ms
```

As medidas são médias de tempo **CPU**, não tempo de execução interna da GPU. O perfil geral ainda inclui outras operações do loop como altura, áudio e gerenciamento de eventos.

A comparação A/B de `--no-gpu-cache` continua disponível. Desativar o cache de GPU **não** desativa a separação entre as geometrias estática e animada; mede apenas o custo adicional do envio ao OpenGL.

## Testes e limitações

- Nova suíte `ui05x3_static_animated_scene_equivalence` usa um `.obj` e dois `.lmo` sintéticos decodificados pelos parsers reais. Confere os vértices conjuntos contra `Pass::All`, preservação dos pisos, avanço de animação e limpeza em troca de mapa.
- `35/35` suítes CTest passaram em ambiente headless. `SceneWorld.cpp` e `main.cpp` passaram na verificação C++17 com avisos ativos.
- O código OpenGL completo ainda depende da compilação no Linux do usuário (aqui não há cabeçalhos oficiais GL).
- O ganho de FPS **não foi medido neste ambiente** e deve ser confirmado no 4600G. A GPU integrada usa memória compartilhada; RSS da aplicação não é consumo total de RAM ou VRAM.
- A colisão de partes *animadas* é um snapshot do início da janela de streaming, assim como anteriormente entre as reconstruções do cenário. Plataformas realmente móveis exigem uma colisão dinâmica específica.
- Não houve mudanças na precisão visual de materiais ou efeito de transparência; entretanto, a separação pode exigir validação visual de objetos animados que usem blend para se sobrepor a objetos estáticos.

## Instalação

Sobre a versão 05-X2, extraia ZIP incremental `sources/` na raiz do projeto, ou use o completo. Faça backup do código atual antes.

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

O banner esperado: `top_gl offline - migracao 05-X3 (cena estatica/animada separada, perfil detalhado)`.

### O que validar

Observe **chão contínuo** ao caminhar/teleportar, **animações de árvores/água** ativas, **escadas/pisos** acessíveis e ausência de objetos desaparecidos. Compare os tempos `cena estatica`, `cena animada`, `stream` e `atores` numa região parada durante 20–30 segundos. O próximo gargalo provável é o skinning dos personagens, que consumia ~12 ms/quadro no log 05-X2.
