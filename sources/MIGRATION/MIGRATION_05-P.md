# Migração 05-P — IA de monstros: ataque e fuga, memória de alvo e passeio offline

Base: 05-O, que corrigiu os Action IDs/poses. Não modifica as animações nem o código da interface.

## Evidência histórica

Em `servidor/server/GameServer/resource/script/ai/ai_define.lua`, o GameServer identifica:
`AI_NONE=0`, `AI_N_ATK=1`, `AI_FLEE=2`, `AI_MOVETOHOST=4`, `AI_R_ATK=5`,
`AI_ATK=10` e `AI_ATK_FLEE=11`. No `ai.lua` original, `AI_ATK_FLEE` foge
quando o alvo está a menos de 400 cm; `ai_idle` executa `birth_rand_move(c,600)`
para monstros sem rota explícita de patrulha, e `AI_NONE` não executa o loop de IA.

## Alterações

- **AI 11**: começa a recuar ao visualizar o jogador a menos de 4 m;
  continua o recuo até 5 m para evitar oscilação por quadro (histerese local,
  não é um parâmetro histórico) e cancela um ataque corpo-a-corpo pendente.
  Ao se afastar, só volta a perseguir/atacar se as regras normais permitirem.
  A fuga nunca escolhe destino fora do raio de nascimento, e usa a rota A* 05-N.
- **AI 1/5**: ao ser provocado (ataque normal, skill ou canhão), inicia uma
  memória de 1,6 s do último alvo visto. Perder contato pelo período todo
  limpa a provocação; entrar em zona segura/morrer a limpa imediatamente.
  A memória curta não dá visão através de paredes ou permissão de dano fora
  do alcance; essas condições continuam checadas em 05-M/05-N.
- **Dano em área**: todos os monstros atingidos por uma habilidade em área,
  e não somente o alvo principal, registram provocação/última posição.
- **AI 0**: criaturas com `AI_NONE` não iniciam passeio aleatório, mesmo
  se a tabela atribuir velocidade.
- **Passeio local sem rota explícita**: preserva a ideia de `birth_rand_move`
  dentro de 6 m do spawn, reduzindo o raio para coleiras menores; rejeita
  células bloqueadas e aguarda antes de tentar novamente um percurso impossível.
  Não implementa `GetChaPatrolPos()` nem a lógica original de patrulhas
  configuradas, pois não há essas coordenadas no `MonsterSlot` atual.

## Limites

- Há somente um jogador como alvo no combate offline; não há redistribuição de
  aggro entre vários jogadores/companheiros. Não executa scripts Lua do servidor.
- Não porta as skills à distância usadas pelo AI 11 original. Monstros que só
  disparam magias enquanto recuam ainda precisam de outra etapa.
- Recuo e percurso dependem da colisão/navegação dos mapas; confirmar em jogo.

## Testes

Novo teste `ui05p_monster_target_memory_and_retreat`, além das 20 suítes
anteriores e de `Play::selfTest()` com os dados históricos.

```bash
cmake -S sources -B build && cmake --build build -j4
./build/top_gl --check
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```
