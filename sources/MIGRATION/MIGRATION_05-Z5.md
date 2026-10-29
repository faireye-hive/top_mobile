# Migração 05-Z5 — sincronização de quadro-chave das skills

## Problema confirmado

O fluxo OpenGL iniciava a animação ofensiva no clique, antes de o personagem
chegar ao alcance, e convertia o `sActionKeyFrme` histórico `0` em impacto após
0,01 s. O client DirectX só entra na ação depois da aproximação e dispara
efeitos/dano nos callbacks da animação (`CWaitAttackState::ActionFrame`). Para
projéteis, o callback de voo é outro campo (`sSkyEffectActionKeyFrame`), seguido
do tempo de deslocamento até o alvo.

## Alterações

- `Engine/include/top/Play.h` e `Engine/src/top/Play.cpp`: preservam também o
  quadro-chave do projétil, coluna 56 de `skillinfo.txt`.
- `Engine/src/top/play/PlayCombat.cpp`: habilidades ofensivas não reproduzem
  uma pose de conjuração enquanto perseguem um alvo fora de alcance. A pose,
  efeito do personagem e disparo do projétil começam pelos respectivos quadros
  da animação; o dano de projétil soma o voo a partir do quadro de lançamento.
  Quadro `-1` representa início, `-2` fim, e IDs positivos/zero usam o quadro
  histórico da pose em vez de um atraso fixo de 0,01 s.
- Skills de autoefeito continuam aplicando sua regra offline, mas reproduzem a
  pose no tempo histórico de `lResumeTime` e agendam o visual no quadro da ação.
- `Engine/src/top/ActorSkin.cpp`: ao mudar entre parado, corrida, ataque ou
  conjuração, reinicia o cursor da pose. O ciclo de corrida não começa mais no
  quadro residual de uma pose anterior.
- `Engine/src/top/PlaySelfTest.cpp`: Illusion Slash verifica que não há efeito
  antes do quadro-chave, que o projétil respeita 8,5 m a 16 m/s e que o dano
  espera a chegada. O teste de Conch Ray agora avança em quadros pequenos para
  observar o estado de atordoamento sem consumi-lo num passo artificialmente
  longo.

## Verificação

- `top_ui05v_play_tests`: passou.
- CTest: 71/71 testes passaram.
- `top_gl --check`: executado após a reconstrução do binário e carregou o mapa,
  atores, efeitos, NPCs e tabelas sem falhas de recurso.
- `git diff --check`: sem erros.

Não houve cópia de novos arquivos de `arquivo/`; o client já contém os recursos
históricos usados por Illusion Slash (`04000009.eff`, `01000014.par` e
`01020028.par`).
