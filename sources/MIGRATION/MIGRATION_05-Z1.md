# 05-Z1 — tempo histórico das poses de habilidade

O DirectX entrega `SSkillGridEx::lResumeTime` ao `CCharacter::PlayPose` como o tempo desejado da animação. Para habilidades cujo `szFireSpeed` aponta para a fórmula histórica, o cliente offline reutiliza o resultado já extraído de `skilleffect.lua`; Illusion Slash (`SkillCooldown_Hyz`) resulta em 5000 ms. A pose OpenGL agora avança proporcionalmente para terminar nesse tempo, em vez de sempre reproduzir a 30 quadros/s. O campo `sActionKeyFrame` é mantido separado da duração: `-1` inicia no começo, `-2` no fim e quadros normais seguem a normalização do DirectX.

Quando a pose indicada por `Play Action` não existe no banco do personagem ou do modo de arma, `ActorSkin` já usa o ataque equipado como fallback. O combate agora mantém essa pose como one-shot e deixa-a terminar no tempo histórico, em vez de encurtar o golpe pelo cooldown normal e voltar a repeti-lo.

Cada reprodução também recebe o ciclo do ataque. Assim, um segundo Illusion Slash (ação 14) reinicia a pose mesmo quando começa no mesmo quadro em que a anterior terminou e não houve um frame intermediário de repouso.

Os efeitos de projétil usam distância/velocidade da tabela para estimar o voo, em vez de encerrar junto com o quadro de impacto. As ações de caminhada seguem os IDs originais de `CharacterAction.h`: ação 5 em área segura e ação 6 em combate; para repouso, ação 1 segura e ação 4 em combate. `Character::GetPose` confirma que RUN/WAITING são remapeadas para RUN2/WAITING2 quando a região permite luta. O runtime OpenGL usa esse estado para alternar também o dummy da arma entre mão e costas.

O alcance de Illusion Slash continua vindo de `skillinfo.txt` (800 cm, mais os raios dos atores). O teste de regressão agora confere o tempo de 5 s, a reprodução one-shot e o encerramento; os testes de modo de arma e ações históricas também passam. Não foram copiados novos arquivos de `arquivo/` neste incremento.

Ainda é necessário validar visualmente os bancos de pose de cada raça/classe e completar um teste de ponta a ponta do clique no alvo, pois os testes automatizados não substituem essa verificação dentro do jogo.
