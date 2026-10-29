# Migração 05-CK — Poções de movimento

## Regra histórica

- `ItemUse_MspdYS` (Acceleration Potion 3339) aplica `STATE_YSMspd` nível 1
  por 900 segundos; `ItemUse_MspdYSA` (Skating Potion 3105) aplica o mesmo
  estado por 1800 segundos.
- `State_YSMspd_Add/Rem` soma/remove `0.15` em `MspdSa`, o modificador
  multiplicativo de velocidade de movimento.
- Ambos recusam uso enquanto o personagem navega. Um estado ativo de mesmo
  nível não é renovado pelo `AddState` histórico.

## Porte OpenGL

Os callbacks consomem o item em terra e adicionam o fator 1,15 ao cálculo
`Play::moveSpeed()`. O timer temporário existente expira o bônus e é preservado
durante a troca de mapa. A escala corresponde ao incremento histórico de
`MspdSa`; efeitos simultâneos com equipamentos são multiplicados no resultado
de movimento atual.

## Verificação e limites

- Cobertura: `item_action_audit.py` classifica os dois itens e callbacks.
- Estado: D1. Compilação, uso interativo e save/reload ainda precisam de
  verificação; o estado expira e reinicia ao fechar o jogo.
