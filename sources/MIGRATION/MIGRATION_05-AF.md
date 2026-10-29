# Tales of Pirates offline — migração 05-AF (inventário de recursos)

Base: **05-AE**. Destino: Linux/OpenGL, execução **offline**. Não inclui multiplayer, GameServer ou autenticação.

## Implementado

- `top_asset_audit`: ferramenta independente de OpenGL/GLX e de inicialização do jogo.
- `top_gl --audit-assets`: mesmo inventário dentro do executável, **sem criar janela**, sem ler/gravar save e sem iniciar renderização.
- Checagem por recurso de `ok`, `missing`, `invalid`, `empty` ou `case_mismatch`, com o nome esperado e o nome realmente encontrado.
- Diagnóstico de cabeçalhos de `.map`, inclusive limite de tamanho para evitar alocação incorreta em `.map` danificado (não examina todos os blocos do terreno).
- Checagem de `.obj` opcional (formato v600 esperado); mapas sem `.obj` podem ser válidos e desenham cena sem objetos.
- Conferência da existência das tabelas, recursos visuais, animações e caminhos conhecidos no código. Referências `.lmo` de `sceneobjinfo.txt` aparecem individualmente como `scene_catalog`/`diagnostic`, pois nem todos são usados por todos os mapas.
- Levantamento por mapa: `.map`, `.obj`, `npc.txt`, `ChaSpn.txt`, `swhmap.txt`, incluindo layout alternativo `scripts/offline/<map>/` além de `scripts/offline/maps/<map>/`.
- Relatórios JSON e CSV (campos: `map`, `category`, `expected`, `actual`, `status`, `importance`, `detail`).
- `--strict` retorna não-zero quando recurso obrigatório está ausente, existe diferença de caixa ou arquivo está inválido; modo padrão é **apenas diagnóstico**.
- Suporte a `TOP_CLIENT_ROOT` ou `--client` para localizar o cliente sem depender da pasta de trabalho.
- Novo teste `ui05af_assets_diagnostics` com `.map` válido/inválido, nomes de caixa diferente, localização alternativa de scripts e serialização JSON/CSV.

## Executar no Ubuntu (na pasta do projeto que contém `Client/`)

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --audit-assets --client Client --json 05AF_assets.json --csv 05AF_assets.csv
# Alternativa que NAO precisa compilar a parte grafica do projeto:
cmake --build build --target top_asset_audit -j4
./build/top_asset_audit --client Client --json 05AF_assets.json --csv 05AF_assets.csv
# Um mapa apenas:
./build/top_asset_audit --client Client --map garner --json garner.json --csv garner.csv
# Para verificacao automatica/CI (falha se faltam recursos obrigatorios ou existe case mismatch):
./build/top_asset_audit --client Client --strict
```

Diferente de `--check`, esta auditoria **nao impõe que existam pelo menos 50 mapas**: um cliente parcial recebe um diagnóstico, não uma falha na contagem arbitrária. Sem `--strict`, o exit code 0 significa que a ferramenta terminou e escreveu os relatórios — **não significa que os recursos estão completos**.

## Testes sem OpenGL

```bash
cmake -S sources -B build-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-tests --target top_regression_tests -j4
ctest --test-dir build-tests --output-on-failure
```

Se os scripts históricos estiverem em `Client/scripts/` ao lado da pasta `sources/`, o CMake configura o `ui05v_play_state_regression` para executar a partir da raiz do projeto automaticamente. Se não estiverem disponíveis na configuração, o teste ainda requer fixtures em `build-tests/Client/scripts/` e falhará claramente sem elas. O instalador **não extrai nem altera** scripts de ZIP.

## Validação realizada neste ambiente

- Ferramenta `top_asset_audit` compilada em C++17 e executada sem bibliotecas de desenvolvimento OpenGL.
- **46/46 testes de regressão** aprovados ao disponibilizar os scripts atuais de `scriptsclientatual.zip` no caminho esperado do teste.
- Sobre uma montagem contendo **apenas** os scripts enviados, foram descobertos **26 conjuntos de scripts de mapas** e **0 arquivos `.map` de geometria**: resultado correto para aquele subconjunto de dados, não é teste do Client completo.
- Detectadas 6 inconsistências reais de nomes no Linux entre o padrão minúsculo consultado e os caminhos enviados: `DreamIsland` (NPC/ChaSpn/portal) e `PKmap` (NPC/ChaSpn/portal).
- O ambiente de auditoria não tem `GL/gl.h`/`GL/glx.h`: compilação completa e teste visual de `top_gl` precisam ser realizados no Linux do usuário.

## Limites e próximos passos

**Não foi declarado que a renderização está corrigida.** Testes validam existência e cabeçalhos, não geometria completa, textura decodificada, iluminação, efeitos, scripts Lua executados, nem comportamento gráfico DX9. `.lmo` catalogado não significa modelo presente em um mapa concreto.

**Próxima migração 05-AG:** auditar correspondência dos esqueletos, matrizes, *dummy bones* e pontos de fixação de armas (erro previamente reportado: espadas afastadas das mãos), com fixtures por personagem/pose e capturas comparáveis.
