# Migração 05-CE — pose de skill e estado de armas

## Problema

O cliente OpenGL iniciava a ação de skill por um temporizador fixo de 0,45 s e
encurtava a pose de ataque de acordo com a velocidade de ataque. O renderizador
repetia as ações de ataque/skill em loop. Isso fazia a animação reiniciar ou ser
interrompida antes do último quadro. Além disso, a troca de área segura/combate
do DirectX não chegava à apresentação do personagem.

## Alterações

- Duração e quadro de impacto passam a ser extraídos da ação em
  `CharacterAction.tx`, por skill e por variante de ação.
- O cooldown continua seguindo ASPD, mas a pose de skill é reproduzida uma vez
  até o último quadro; o dano ocorre no quadro-chave de impacto.
- O alcance é mantido conforme `skillinfo.txt`; o teste confere o alcance de 800
  (8 m) do Illusion Slash e a aproximação até a borda correta.
- A região `NOT_FIGHT` seleciona ações de espera/corrida sem arma sacada e usa
  os dummies históricos 21–23 para guardar as armas nas costas. Em áreas de
  combate, retorna às mãos. O alinhamento dos encaixes na mão permanece intacto.

## Verificação

- `top_ui05v_play_tests`: passou, incluindo a validação de alcance/ação do
  Illusion Slash.
- CTest focado em `ui05d_weapon_pose_modes`, `ui05e_historical_action_poses` e
  `ui05v_play_state_regression`: 3/3 passaram.
- `top_gl --check`: terminou com código 0 e carregou atores, tabelas de skill,
  efeitos e mapa de Argent.
- A suíte completa executou 71 testes; dois testes de UI de seleção de
  personagem falharam em `ui04e_modal_regression` e `ui04f_visual_commands`, que
  não cobrem estes módulos. Os outros 69 passaram.

## Arquivos principais

- `Engine/include/top/ActorSkin.h`
- `Engine/src/top/ActorSkin.cpp`
- `Engine/include/top/Play.h`
- `Engine/src/top/Play.cpp`
- `Engine/src/top/play/PlayCombat.cpp`
- `Engine/src/top/play/PlayPresentation.cpp`
- `Engine/src/top/PlaySelfTest.cpp`
