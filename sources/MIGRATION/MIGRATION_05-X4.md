# Tales of Pirates offline — 05-X4: animação de atores sem trabalho repetido

Base: **05-X3**. Nenhuma alteração na interface, regras de combate, formatos `.lab`/`.lgo`, equipamentos, câmera ou streaming de terreno/cenário.

## Motivação e dados

No Ryzen 5 4600G, o relatório da 05-X3 mediu ~7–14 ms de tempo **atores** por quadro, enquanto o streaming de cenário já caiu para ~1–3 ms. Esses valores são medições do computador do usuário. Não presumimos um FPS novo sem testar nessa máquina.

Antes, para *cada índice de triângulo* da malha do personagem, o `ActorSkin.cpp` recalculava skinning de posição, skinning da normal, transformação pelo nó e normalização. O mesmo vértice costuma aparecer em vários triângulos e mesmo em subconjuntos. Além disso, a matriz de todos os ossos era reconstruída mesmo quando a animação ainda usava o mesmo quadro, e cada espada consultava o dummy recalculando todo o esqueleto.

## Alterações

- `Engine/include/top/ActorFrameCache.h`: cache por **índice de vértice**, com carimbos de geração. A posição e a normal deformadas no espaço do modelo são preservadas **apenas enquanto o quadro da animação for o mesmo**. A transformação para o mundo é refeita **a cada `ActorSkin::build`**, inclusive quando o personagem apenas gira ou se move e a pose não muda.
- `Engine/src/top/ActorSkin.cpp`: mantém uma matriz de ossos reutilizável por corpo. Após a mudança de quadro, recalcula a pose; se o quadro for repetido, reutiliza a pose. Subconjuntos compartilham um cache de vértices da geometria, mas continuam emitindo a **mesma lista de triângulos**, com a mesma ordem, UVs, materiais e normais do modelo. Armas usam o dummy aplicado à matriz de ossos já calculada (mesma ordem de multiplicação da correção histórica 05-D).
- Trocas de equipamento e de barco limpam os caches de geometria, evitando reutilizar vértices da malha antiga. A troca de mapas já recria os corpos.
- `--profile`: linha extra `profile atores detalhado` com número de esqueletos recalculados, poses reutilizadas, vértices únicos deformados, vértices transformados para o mundo e índices emitidos. **Esta linha reflete o último quadro amostrado**, enquanto os tempos em milissegundos continuam sendo médias do intervalo.

## Validação

- `ui05x4_actor_skinning_vertex_cache`: um quad com dois triângulos deve deformar 4 vértices únicos, não os 6 índices emitidos; testar reutilização da mesma pose, mudanças de posição/orientação entre quadros, invalidação da pose, limpeza do modelo e redimensionamento.
- Sintaxe C++17 dos arquivos `ActorSkin.cpp` e `main.cpp` validada; ambos também foram compilados em `-O2` sem o caminho GL (warnings preexistentes de funções não utilizadas no ActorSkin). Não existe `GL/gl.h` neste ambiente: o binário gráfico completo requer a máquina Linux do usuário.
- CTest: **36/36 suites aprovadas**. A suite 05-X4 foi recompilada; as 35 suites anteriores, de fontes não modificados, foram executadas de novo usando binários da 05-X3.

## Instalação e teste

Na pasta que contém `sources/` e `Client/`:

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

Conferir armas com duas espadas, os personagens e os monstros em movimento, o encaixe dos punhos e os efeitos de morrer/sentar. Caminhar por múltiplas regiões para assegurar que cenário/terreno não desapareceram.

Comparar `--profile` na **mesma posição e resolução** da 05-X3 e da 05-X4. Se aparecerem falhas gráficas, voltar ao ZIP completo da 05-X3 e informar a criatura/modelo e o instante.

## Ainda pendente

Skinned meshes continuam usando CPU e sendo transferidas ao OpenGL a cada quadro. GPU skinning, LOD, culling de personagens e buffers persistentes para vértices animados ainda são possíveis melhorias, mas precisam de testes e dados antes de mudar o motor histórico.
