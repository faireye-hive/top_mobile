# 05-BK — caminho do jogador com colisão da cena

O A* do clique considerava somente os quatro bits de colisão do mapa; a movimentação, além deles, respeitava as lajes sólidas de `SceneMesh` e o limite de subida do piso. Assim, uma rota podia atravessar uma parede/casa invisível para o planejador e o passo real ficava parado ou cancelava o comando.

`PathFinder::find` agora aceita uma consulta de bloqueio adicional por célula. O planejamento e o recálculo do jogador passam a incluir o footprint de 0,40 m e as mesmas lajes/pisos estáticos da cena usados no deslocamento. A célula inicial continua permitida mesmo quando já sobreposta a um objeto, para que o personagem possa sair da colisão. O check cobre uma rota real do Garner com a colisão estática carregada.

Esta mudança trata obstáculos de cena estáticos; obstáculos animados/dinâmicos e a validação visual de percursos longos ainda precisam de cobertura. Validação desta etapa: build `top_gl`, `top_gl --check`, CTest e `git diff --check`.
