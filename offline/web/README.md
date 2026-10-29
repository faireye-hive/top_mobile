# Cliente web e Linux com o mesmo C++

O jogo, a interface e os leitores de recursos são compartilhados. As mudanças
de combate, itens, missões, mundo e interface devem ser feitas em `sources/Engine/`
e `sources/Platform/common/GameMain.cpp`: ambas as plataformas recebem a mudança.
Não existe uma cópia do gameplay em JavaScript.

## Abrir a versão web

Na raiz do repositório:

```bash
bash offline/web/build.sh
bash offline/web/serve.sh
```

Abra **http://localhost:8080**, espere o botão **Jogar** ficar disponível e clique.
O primeiro acesso baixa os recursos utilizados; acessos seguintes podem aproveitar
o cache HTTP do navegador. A pasta `dist-web/` contém o site distribuível.
Não abra `index.html` diretamente com `file://`.

Pré-requisitos: CMake 3.16+, Python 3.11+ e Emscripten SDK. O SDK oficial 6.0.13
foi instalado nesta máquina em `.local-tools/emsdk/`, pasta ignorada pelo Git.
O script usa esse SDK quando não encontra outro já ativado. Em outra máquina,
instale/ative o [SDK oficial](https://emscripten.org/docs/getting_started/downloads.html).

O script aceita opções adicionais do CMake. Exemplo de fonte alternativa:

```bash
bash offline/web/build.sh -DTOP_WEB_FONT=/caminho/fonte.ttf -DTOP_WEB_FONT_LICENSE=/caminho/licenca.txt
```

Por padrão usa DejaVu Sans e copia o aviso de licença instalado no Linux.
Os avisos das bibliotecas usadas ficam em `licenses/` no pacote final.
`TOP_BUILD_JOBS=2` controla o paralelismo. Para trabalhar só no código, sem
recopiar os recursos:

```bash
source .local-tools/emsdk/emsdk_env.sh
cmake --build build-web --target top_web -j2
cp build-web/index.html build-web/index.js build-web/index.wasm dist-web/
```

Para compilar o Linux, os comandos existentes continuam válidos:

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j2
./build/top_gl
```

## Organização

| Responsabilidade | Local |
|---|---|
| Inicialização e loop compartilhados | `sources/Platform/common/GameMain.cpp` |
| Entrada do executável Linux | `sources/Platform/linux/main.cpp` |
| Janela escolhida na compilação | `sources/Engine/include/top/Window.h` |
| Janela Linux | `sources/Engine/src/gl/GlxWindow.cpp` |
| Canvas, entrada e apresentação web | `sources/Platform/web/WebWindow.cpp` |
| Renderizador compartilhado e variantes GLSL | `sources/Engine/src/gl/GfxDeviceGL.cpp` |
| Fontes web via API oficial do FreeType | `sources/Platform/web/FontFreeType.cpp` |
| Arquivos web e armazenamento local | `sources/Platform/web/WebFiles.cpp`, `bootstrap.js` |
| Opções de compilação web | `sources/cmake/Web.cmake` |
| Preparação dos recursos públicos | `sources/Tools/package_web_assets.py` |

O adaptador web usa WebGL 2 e GLSL ES 3.00. O Linux conserva GLX e GLSL 3.30.
O áudio usa a mesma lógica C++; no navegador, SDL2_mixer é compilado junto do
jogo, com decodificação OGG. O contexto de áudio pode exigir um clique no canvas.

O loop usa Asyncify para suspender sua pilha entre quadros, aguardando
`requestAnimationFrame`, e durante downloads. Isso preserva o estado e as funções
locais já existentes. Não habilitamos pthreads: o prefetch de população usa
trabalho adiado na web e continua em thread no Linux. Carregamentos e parsing
pesados ainda podem causar pausas; medir antes de trocar esse mecanismo.

## Recursos e saves

O empacotador publica as pastas conhecidas de recursos de `Client/`, preservando
nomes e maiúsculas/minúsculas. O índice lista caminhos, tamanho e SHA-256. O
navegador cria uma árvore virtual para que os resolvedores de caminhos e a
descoberta de mapas funcionem, e baixa cada arquivo antes da primeira leitura
por `fopen` (inclusive os leitores `std::ifstream` da libc++).

Arquivos ZIP, executáveis, arquivos ocultos, links simbólicos e **Client/user/**
não são publicados. Não copie a pasta Client inteira para o servidor. O pacote
de recursos completo ocupa aproximadamente 1,7 GiB no disco do servidor; cada
jogador baixa o que sua sessão solicitar. Recursos já baixados permanecem no
filesystem em memória durante a sessão: o limite configurado da memória WASM é
2 GiB, e o consumo deve ser medido em sessões longas e em celulares.

O progresso web fica em `/Client/user`, montado em IDBFS/IndexedDB, separado dos
saves Linux. Ele é restaurado antes de iniciar o jogo e sincronizado a cada cinco
segundos entre quadros. **Salvar no navegador** solicita um save do jogo e aguarda
a sincronização. Também há tentativa de sincronização ao ocultar a aba. Fechar
abruptamente o navegador pode interromper uma gravação; usar o botão antes de
sair evita depender do evento de fechamento. Limpar os dados do site remove
os saves. O endereço/origem do site identifica seu armazenamento.

## Hospedagem e verificação

Hospede apenas `dist-web/` em um servidor estático HTTP(S). Sirva `.wasm` como
`application/wasm`, mantenha `assets-index.json` revalidável e os arquivos de
recursos com cache HTTP. As URLs dos recursos incluem seu hash; a integridade é
verificada no download. HTTPS é necessário fora de localhost para os recursos
de navegador que exigem um contexto seguro. Esta versão não exige cabeçalhos
COOP/COEP porque não usa threads WASM.

Checks do empacotador:

```bash
python3 -m unittest discover -s sources/tests -p test_package_web_assets.py -v
```

O porte web continua com o escopo offline do cliente atual. Multiplayer,
sincronização de saves entre dispositivos, suporte touch e validação de todos
os mapas/navegadores são frentes adicionais. A compatibilidade visual e o
desempenho precisam de teste no hardware alvo; compilar não comprova paridade.
