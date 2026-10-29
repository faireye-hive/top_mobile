# 05-CG — Revisão das animações de habilidades e combate

## Referência histórica

A comparação foi feita com `arquivo/fontes/Client/src/STAttack.cpp`,
`CharacterModel.cpp`, `Libraries/common/include/SkillRecord.h` e as tabelas do
cliente local. Não houve execução lado a lado do cliente DirectX.

Em `CWaitAttackState::_UseSkill`, o ramo de ações cíclicas usa a velocidade da
habilidade/ASPD. Já o ramo não cíclico redefine `PoseVelocity` para `1.0` em
30 FPS ou `0.5` em 60 FPS, resultando em 30 quadros autorais por segundo. O
ponto e vírgula depois do `if (GetPoseVelocity() < per)` torna essa atribuição
incondicional na fonte arquivada. `chPlayTime == 1`, coluna 68 de
`skillinfo.txt`, distingue esses ramos.

Portanto, os 5.000 ms de `SkillCooldown_Hyz` são a recarga de Illusion Slash,
não a duração de seu movimento não cíclico. A interpretação anterior em
05-CF e no teste de regressão esticava a pose para cinco segundos. Esse teste
foi corrigido: uma pose sintética de 0,8 s mantém essa duração e sua recarga
continua sendo de 5 s.

## Correções

- Habilidades não cíclicas usam a duração nativa; ações cíclicas continuam
  usando o tempo histórico ou ASPD. A fórmula dependente de nível usa o nível
  aprendido, sem subtrair um nível indevidamente.
- Ataques normais avançam seus quadros na mesma velocidade usada para o
  temporizador de impacto. As variantes 7/8/9 consultam seus próprios tempos,
  e os movimentos terminam uma vez, sem repetir durante o golpe.
- Os tempos do jogador são atualizados quando muda o grupo de poses da arma.
  Não ficam presos à arma existente na primeira chamada de `bind`.
- Monstros recebem a ação escolhida pela habilidade, velocidade e contador
  de execução. Com uma pose disponível, seu dano, conjuração e lançamento
  seguem os quadros do próprio modelo; a chegada do projétil soma o tempo de
  voo. Os valores genéricos antigos permanecem apenas para atores sem dados
  de animação, incluindo fixtures de testes.
- Buffs repetidos reiniciam o movimento, o autoataque aguarda sua pose e uma
  habilidade ofensiva anteriormente armada não suprime a animação do buff.
- Efeitos aguardando um quadro são descartados quando a execução foi
  substituída ou seu dono morreu/foi atordoado. Monstros retornando ou recuando
  também descartam os efeitos ainda não emitidos.
- Um impacto no primeiro quadro é preservado como tempo zero.
- A duração usa `(end - start) / 30`, como `CharacterModel::PlayPose`. A pose
  substituta de uma ação ausente é a mesma para o cálculo de tempo e desenho.

## Verificação

- Build completo aprovado.
- CTest: **71/71 aprovados**.
- `top_gl --check` aprovado para Lance, Carsise, Phyllis e Ami.
- Regressões numéricas: movimento de Illusion Slash versus recarga, variantes
  com durações diferentes em ASPD alto, callback no primeiro quadro, buffs
  repetidos, lançamento/chegada de tiro de monstro e cancelamento por stun.
- `ActorSkin::selfTest` exercita o relógio real do renderer sem GL: seleção
  da ação do monstro, avanço acelerado, parada no último quadro, reinício da
  mesma ação e tempos/fallback de outro grupo de armas.

Os testes verificam os tempos e o caminho de renderização, mas não demonstram
paridade visual completa de todas as habilidades. Não foi concluída uma
comparação visual lado a lado de emissores, encaixes de efeitos e todas as
combinações de personagem/arma/habilidade; os fallbacks visuais existentes
continuam sendo uma limitação do porte.
