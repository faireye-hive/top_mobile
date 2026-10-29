# Migração 05-F — pipeline de efeitos offline (C++17/OpenGL)

Base: `TalesOfPirates_05-E_Completo.zip`.

## Mudança funcional

- `EffectEvent::Flight` inicia o projétil no começo da animação de ataque com skill, antes de o dano ser aplicado.
- `Play::resolve` agenda o efeito de trajetória **somente** quando a skill possui `Path of Flight Effect` (coluna 59 de `skillinfo.txt`).
- A duração da trajetória usa `pendingAttack_`, que já representa o tempo de windup até o quadro de impacto da animação.
- `Play::landPlayerAttack` continua sendo o único ponto que aplica dano e dispara `EffectEvent::SkillHit` no alvo. `SkillHit` agora desenha o burst no destino em vez de iniciar um projétil atrasado.
- Canhões (`variant >= 900`) mantêm o comportamento de trajeto anterior para não modificar barcos nesta etapa.
- As partículas `.par` já reconhecidas continuam em uso, mas seguem a posição da trajetória (`Flight`) e o destino do impacto (`SkillHit`).
- `SkillVisualCatalog` lê de `skillinfo.txt` os identificadores **Character Effect (51)**, **Item Effect (54/55)**, **Path of Flight Effect (59)**, **Character attacked effect (63)** e os efeitos de chão/água (65/66), sem confundi-los com IDs de `magicsingleinfo.txt`.
- Um ID histórico válido modifica a paleta de fallback do efeito visual, mas **não** implica que seu `.eff` específico seja carregado.

## Limites do porte

Esta é a primeira parte da recuperação dos efeitos específicos. O carregador binário atual ainda lê somente um `.eff` de demonstração, ligado ao ID 38 de `magicsingleinfo.txt`, e dois `.par` de referência. A tabela `magicsingleinfo.txt` deste acervo cobre IDs 10–62; a coluna `Path of Flight Effect` de `skillinfo.txt` usa, entre outros, IDs 1012 e 1013. Eles pertencem a catálogos distintos e não podem ser unidos numericamente. Para reproduzir todos os efeitos é necessário localizar o catálogo legado que associa esses IDs aos arquivos de efeito binários, portar os recursos/formatos e fazer o vínculo aos pontos dummy do modelo. Não inventar nomes de arquivos nem substituir o efeito errado.

A localização 3D do emissor (mão, arma, osso) continua aproximada. Não há protocolo de rede nesta etapa. O HUD não foi alterado.

## Arquivos

- `Engine/include/top/SkillVisual.h` (novo catálogo leve)
- `Engine/include/top/EffectSystem.h`
- `Engine/src/top/EffectSystem.cpp`
- `Engine/include/top/Play.h`
- `Engine/src/top/Play.cpp`
- `Platform/linux/main.cpp` (versão/diagnóstico no `--check`)
- `tests/UI05FTests.cpp` (novo)
- `CMakeLists.txt`

## Validação feita

- Compilação e execução de `top_ui05f_tests` (catálogo, trajetória cronometrada, burst e fallback).
- `Play::selfTest()` aprovado com scripts `Client.zip`.
- Catálogo lido de `Client/scripts/table/skillinfo.txt`: 411 registros, ID 5 com `flight=1012`.
- CTest: 10 alvos disponíveis passaram (`04-D/E/F/G/I` e `05-B/C/D/E/F`). `04-H` não foi recompilado nesta rodada porque a compilação de todos os alvos excedeu o tempo do ambiente.
- Verificação de sintaxe C++17 dos arquivos de gameplay/efeitos: aprovada.
- A compilação OpenGL completa e a inspeção visual ainda precisam ser feitas no Linux com o `Client/` completo.

## Como validar no Linux

```sh
# Extraia o ZIP escolhido NA RAIZ de TalesOfPirateDX9 (contém sources/)
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui05f_tests -j4
ctest --test-dir build-ui-tests -R ui05f_effect_timeline --output-on-failure

# TODOS os testes (constrói os executáveis de teste antes de chamar ctest):
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Marca de versão esperada no console:
`top_gl offline - migracao 05-F (trajetoria e impacto de skills; bolsa 24-48)`

No jogo, compare uma skill com projétil (Fiery Arrow, quando aprendida) e uma skill corpo a corpo (sem coluna 59): a primeira deve gerar trajeto antes da explosão no alvo, a segunda não deve gerar projétil artificial.
