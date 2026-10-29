# 05-M — IA offline dos monstros: agressividade, visao e retorno

## Escopo

Mantem o formato original do cliente e o jogo 100% offline. Esta etapa nao altera o inventario, a interface nem a rede.

O `Play.cpp` ja implementava parte da IA antiga. **Nao** reescrevemos seu controle; corrigimos casos em que o monstro atacava sem alcance ou continuava afastando-se da origem.

- Novo `top/MonsterAI.h`: funcoes puras testaveis de agressividade (`AI >= 10`), revide (`AI 1/5`), fuga (`AI 2`), raio de ataque e de nascimento (`leash` derivado de characterinfo coluna 77).
- Quando houver mapa carregado, monstros verificam linha de visao amostrada na grade de 0,5 m antes de perseguir ou iniciar/acertar ataques. Nao atravessam obstaculos por visao direta; **nao** e um A* nem uma restauracao integral da IA do servidor.
- **Impacto**: cada golpe pendente verifica novamente alcance, area segura, vida do jogador, retorno ao spawn e linha de visao. Esquivas depois do inicio da animacao agora evitam dano, embora o monstro mantenha o cooldown.
- **Retorno**: ao sair do raio `lCDis`, o monstro esquece a provocacao e continua voltando ate se aproximar de 1 m da origem (histerese). Nao ataca enquanto retorna.
- **Fuga**: `AI 2` nao ultrapassa `lCDis`; ao alcancar o limite passa a retornar ao spawn.
- Movimento com mapa testa tambem o destino diagonal, evitando deslizar atraves de quinas bloqueadas.
- Guarda os comportamentos anteriores de monstro passivo, agressivo, parado, respawn, missao, loot e combate offline.

## Limites

- Nao reproduz integralmente as variantes de `ai.lua`: invocacoes, patrol, habilidades especiais e mudanca de alvo permanecem futuras.
- Em locais com paredes, o monstro pode parar sem encontrar um caminho alternativo, pois ainda nao foi integrado A*/navegacao de monstros.
- Os testes de linha de visao usam uma parede sintetica; o comportamento em todos os mapas historicos deve ser validado no Linux.
- O `Client.zip` para testes de logica contem scripts mas nao todos os assets 3D. Build GLX integral requer GL/X11.

## Testes

`top_ui05m_tests` executa casos de agressividade, revide, fuga e coleira, visao interrompida, alcance, area segura e estados de retorno. `Play::selfTest()` foi ampliado para testar dano cancelado quando o jogador esquiva, retorno desprovocando e fuga limitada.

```bash
rm -rf build build-ui-tests
cmake -S sources -B build && cmake --build build -j4
./build/top_gl --check
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Identificador: `top_gl offline - migracao 05-M (IA de monstros e alcance; bolsa 24-48)`.
