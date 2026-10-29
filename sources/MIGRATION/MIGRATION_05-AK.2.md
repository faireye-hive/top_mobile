# Tales of Pirates — Migração 05-AK.2: cronômetro de inicialização

## Motivação

Após a otimização do índice de missões na 05-AK.1, o jogo ainda demora a abrir na instalação Ubuntu. O `--profile` existente mede quadros *depois* de iniciar, portanto não identifica a etapa que bloqueia a abertura da janela.

Esta atualização **somente instrumenta a inicialização**: não altera missões, UI, renderização, saves ou conteúdo da pasta `Client/`.

## Uso

A partir da raiz do projeto (onde existe `Client/`):

```sh
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure

# Medir a demora real, incluindo a criação da janela e o primeiro quadro:
./build/top_gl --startup-profile 2>&1 | tee startup_terminal.log

# Opcional: nome customizado para o CSV
./build/top_gl --startup-report inicio_garner.csv

# Opcional: comparar FPS depois da abertura, separadamente
./build/top_gl --startup-profile --profile
```

`--startup-profile` gera **`startup_profile.csv`** no diretório atual e imprime linhas `stderr` prefixadas com `[startup]`:

```
[startup] +   24.12 ms | total    50.21 ms | leitura dos NPCs visuais
[startup] +  624.45 ms | total  2401.82 ms | personagens, modelos e animacoes
[startup] +  305.24 ms | total  2720.18 ms | indicadores de missao (! e ?) - consulta Lua
[startup] ===== etapas mais lentas (ate 5) =====
[startup] TEMPO ATE PRIMEIRO QUADRO: 3600.18 ms
```

Os números acima são **somente ilustração**, não medições da instalação do usuário.

O CSV tem `etapa,delta_ms,total_ms`: `delta_ms` é a duração **desde o checkpoint anterior**, e `total_ms` é desde a entrada em `main()`. Isso identifica o último trecho concluído; não mede subetapas internas de cada função. O arquivo é atualizado e descarregado a cada checkpoint, inclusive se o processo parar antes do primeiro quadro (em caso de encerramento abrupto, a última operação em andamento não terá linha final).

Checkpoints incluídos: descoberta e leitura do mapa, spawn e NPCs, início da janela GLX, inicialização do dispositivo/shaders, texturas de água, cenário, personagens e animações, fonte/cursor, `Play`, efeitos e áudio, portais e scripts NPC, consulta de indicadores de missão, lojas, preparação do loop, streaming, animação, desenho e `swap()` inicial.

A compilação e os testes `--check`/`ctest` não medem a inicialização da GUI. É preciso usar o comando **sem `--check`**, e manter a janela aberta até o texto `primeiro quadro desenhado`. O arquivo pode ser enviado em uma próxima conversa junto com `startup_terminal.log`.

Se o usuário percebe lentidão **antes** da janela aparecer, observe os checkpoints até `criacao da janela GLX`. Se a janela já existe e a tela demora, observe as etapas posteriores. O relatório lista as cinco maiores durações. O `--startup-profile` desativado não abre arquivo e não imprime checkpoints.

## Código alterado

- `Engine/include/top/StartupProfiler.h` — cronômetro monotônico, saída ao vivo no stderr e CSV atualizado a cada etapa; mostra as 5 maiores durações.
- `Platform/linux/main.cpp` — opção `--startup-profile`/`--startup-report`, marcações antes e depois da inicialização dos subsistemas e no primeiro frame; banner atualizado para 05-AK.2.
- `tests/UI05AK2Tests.cpp` e `CMakeLists.txt` — regressão headless do cronômetro e serialização CSV.
- `sources/PROMPT_PROXIMA_MIGRACAO.md` atualizado.

## Escopo e limites

- Sem mudanças nos saves/recursos originais.
- O CSV contém valores reais apenas quando o usuário o executa em sua máquina.
- `--profile` continua sendo o medidor de FPS após a abertura; `--startup-profile` mede a carga inicial.
- A causa da lentidão ainda **não está confirmada**; o propósito desta versão é isolá-la.
- Planejada 05-AL: missões simultâneas, somente depois de validar a inicialização.

## Validação neste ambiente

- 53/53 testes headless aprovados com os scripts históricos em `Client/scripts/`.
- `g++ -std=c++17 -fsyntax-only -Isources/Engine/include sources/Platform/linux/main.cpp` aprovado.
- Compilação gráfica/vinculação `top_gl` ainda dependem do Ubuntu do usuário (SDK `GL/gl.h` ausente no ambiente de análise).
- O cronômetro foi testado artificialmente com atraso conhecido e escape CSV, mas **a lentidão da máquina do usuário ainda não foi medida**.
