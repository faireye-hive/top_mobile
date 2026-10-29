# 05-BH — direção e dano de Backstab

`Skill_Bc_End` (Backstab, ID 85) compara `GetObjDire(ATKER)` e `GetObjDire(DEFER)`: para diferenças absolutas menores que 90° ou maiores que 180°, usa o ramo de dano aumentado; nos demais ângulos, usa `Atk_Raise(1 + nível × 0.05)`. O combate OpenGL agora recebe o yaw atual do personagem por quadro e calcula a diferença com o yaw do monstro no momento do impacto. A função `Atk_Raise` preserva a ordem histórica: multiplicar o ataque amostrado antes de aplicar resistência e defesa.

No ramo aumentado, o Lua original consulta `MxatkIb(role)`, embora `role` não seja parâmetro nem variável local dessa callback. O cliente offline usa o bônus de `maxAttack` dos equipamentos do jogador como equivalência para o atributo histórico `ATTR_ITEMV_MXATK`. Isso é uma interpretação explícita do campo pretendido, não reprodução de uma variável Lua global de estado indefinido; deve ser comparada com execução DX9 se essa global tiver sido injetada em runtime.

`Play::selfTest` cobre os limites 0°, 90° e 181°, e a integração Linux sincroniza `actor.yaw` antes de cada `Play::step`. Validação desta etapa: build `top_gl`, `top_gl --check`, CTest (70/70) e `git diff --check`.

Limite: outros callbacks ofensivos, efeitos de estado e o vínculo temporal de todas as animações ainda não estão completos.
