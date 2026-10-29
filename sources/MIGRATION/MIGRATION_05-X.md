# Migração 05-X — câmera, cursor, seleção, ASPD e diagnóstico

Base: **05-W2**. Cliente inteiramente offline, C++17, Linux/OpenGL.

## Alterações

- **Zoom clássico:** distância da câmera de 4,5 a 22 m, mantendo inclinação elevada da 05-W2 (sem câmera livre). A roda aproxima; a rolagem dentro do inventário/loja mantém sua prioridade.
- **Alvo selecionado:** renderização de um anel azul translúcido no chão sob o monstro vivo selecionado; desaparece ao perder o alvo ou quando o monstro morre. Shader OpenGL usa modo **9** próprio para não afetar as sombras (modo 6).
- **Cursores:** o leitor `.ani` já existe; esta etapa seleciona `chat.ani` sobre NPCs, `pick.ani` nos itens no chão e `drag.ani` ao mover itens/skills, mantendo `attack.ani` no monstro. O `cursor.zip` fornecido possui esses nomes; eles devem estar em `Client/cursor/` (o jogo já os carregava).
- **Velocidade de ataque:** leitura da coluna **37 / ASPD Bonus** do `iteminfo.txt` para cada peça equipada. Os coeficientes se somam (duas espadas contam separadamente). O servidor original define `ATTR_RADIX=1000` em `variable.lua` e `Aspd_final` em `functions.lua`. Um valor 50 adiciona 5% ao fator aplicado. O intervalo calculado é limitado a 0,23–1,70 s, e a duração do golpe/tempo até o impacto são encurtados em ataques rápidos. A ficha mostra ataques por segundo (**ASPD**).
- **Desempenho:** malha de terreno não é mais reconstruída apenas porque algum objeto de cena animou. Reconstrução por deslocamento de setor; cenário animado é atualizado em uma taxa alvo de 15 Hz em vez de 30 Hz. Pode alterar levemente a suavidade dos objetos; não altera a animação da água nem os atores.
- **Medidor:** `--perf` ou **Alt+P** liga/desliga FPS, média de ms por quadro e RSS (RAM residente) na barra de título e no terminal. Coleta após apresentação, 1 vez/s. Não fornece VRAM: drivers OpenGL não oferecem métrica universal, e RAM residente não é o uso de memória gráfica. O medidor mede tempo total por quadro (CPU + espera de GPU/VSync), não tempos isolados de GPU.

## Validação

- `ui05x_attack_speed_camera_selection_performance`: câmera, parse de ASPD, soma de duas espadas, geometria do anel, medidor.
- `ui05v_play_state_regression`: `Play::selfTest()` ampliado para uma e duas espadas equipadas e retorno do ASPD após desequipar.
- `ui05w2_classic_camera_drag_and_confirm`: faixa antiga da câmera adaptada ao novo zoom, confirmando a inclinação.
- Suítes de UI 04F/04G/04H recompiladas. Demais executáveis antigos de testes independentes reaproveitados da 05-W2.
- Resultado CTest local: **32/32 passed**.
- `main.cpp` e `Play.cpp` verificados em C++17 com `-fsyntax-only`. **Não foi possível compilar/testar o executável gráfico completo neste ambiente**, que não tem `GL/gl.h`; validar no Linux do jogador.

## Comandos

```bash
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --perf

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Enquanto joga, **Alt+P** alterna o diagnóstico. Para validar ASPD, compare a velocidade indicada na ficha antes de equipar, com uma espada e com duas. Para validar CPU, compare FPS e ms/quadro parado e em movimento.

## Limitações

- O círculo usa raio aproximado e altura do mapa. Em pontes/superfícies altas, pode ficar abaixo do piso visual.
- Cursores `.ani` são carregados do `Client/cursor`; não são embutidos nos fontes para não duplicar recursos existentes no cliente.
- A animação visual do ataque ainda usa o conjunto de poses histórico e não reproduz todos os detalhes de velocidade do DX9.
- A correção elimina uma fonte de trabalho da CPU, mas a taxa real de FPS depende da GPU/driver, número de draw calls, cena, VSync e textura. Não alegamos um ganho numérico sem medição no hardware do jogador.
- Instâncias de itens com atributos aleatórios, gemas e refino continuam para uma etapa posterior.
