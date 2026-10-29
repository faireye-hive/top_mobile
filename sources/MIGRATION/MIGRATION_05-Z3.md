# 05-Z3 — impacto sincronizado com projéteis de skills

O porte já reproduzia a pose de Illusion Slash como one-shot e criava o evento
de voo com a velocidade histórica da tabela. Porém, também aplicava o dano no
quadro de ação `0` (cerca de 10 ms), enquanto o projétil ainda percorria a
distância até o alvo. No cliente DirectX, quando a skill tem efeito aéreo, o
dano é associado ao controlador de atraso desse efeito.

Agora, skills com efeito de voo e velocidade definida aguardam ao menos o tempo
de percurso calculado entre o personagem e a posição capturada do alvo antes de
resolver o dano. Skills sem projétil continuam usando o quadro de impacto da
ação. A duração da pose e o cooldown permanecem independentes desse atraso.

O teste de regressão para Illusion Slash confirma que o alvo mantém o HP antes
da chegada e recebe o dano após o tempo de voo (`8,5 m / 16 m/s`). Também são
mantidas as verificações do alcance de 8 m, da pose one-shot de 5 s e dos
eventos Cast/Flight. `ui05v_play_state_regression` passou.
