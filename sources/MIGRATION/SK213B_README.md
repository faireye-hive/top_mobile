# SK2.13-B — Interrupção visual da pose de conjuração e acompanhamento de FX corporais

## Base de aplicação

Patch incremental sobre o projeto com **SK2.13-A já aplicada** (reconstruído a partir do `sources(3).zip`, SK2.6.1, SK2.7, SK2.8, SK2.9–SK2.10, SK2.11, SK2.12, SK2.13-A). Aplicar na **raiz do repositório**.

## Problema reproduzido no código

* `Play::playerPose()` retorna 5 enquanto `castingFor_` permanece positivo. `ActorSkin::build()` escolhia a pose 5 antes de conferir `playerMoving_`, e por isso o ator andava com uma pose de skill congelada.
* Na SK2.13-A, apenas os efeitos Cast do jogador com dummy válido e `TOP_HISTORICAL_DUMMY_BIND=1` seguiam a posição atual. Caso contrário, `EffectSystem` continuava usando a origem recebida no instante da conjuração.
* Uma `Cast` agendada guarda as coordenadas originais: conferir proximidade somente no momento tardio do evento podia perder a informação de que pertencia ao jogador.

## Mudanças

1. `ActorSkin`: iniciar a caminhada durante conjuração interrompe **apenas a pose da ação atual**, sem cancelar callbacks de `Play` nem os efeitos. Ao parar, a mesma pose interrompida não reaparece; novo `attackSequence` pode iniciar outra conjuração normalmente. A caminhada usa a taxa normal de quadros, não a taxa de reprodução da skill.
2. `PlayCombat`: marcar a propriedade `sourceIsPlayer` já no agendamento de cada Cast do jogador, inclusive nos casos de habilidades autocentradas e de habilidades ofensivas.
3. `EffectSystem`: preservar a distinção entre canais `Actor`, `Item` e `Ground/Target`. Somente as instâncias de `Actor` pertencentes ao jogador acompanham a âncora atual. Quando a matriz de dummy está disponível no modo experimental, ela continua tendo precedência; caso contrário, usar a raiz atual do jogador à altura padrão anterior (+0,9 m) em vez da posição inicial.
4. `main.cpp`: alimentar `EffectSystem::setPlayerCastAnchor` a cada quadro, depois de calcular as posições do ator.
5. Regressões em `ActorSkin::selfTest`, `UI05SK212Tests` e `Play::selfTest`.

## Aplicação

```bash
git apply --check tales_SK2_13B_pose_interrupt_cast_fx_follow_RAIZ.patch
git apply tales_SK2_13B_pose_interrupt_cast_fx_follow_RAIZ.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Teste visual Berserk

**Padrão** (aura no root do personagem, sem exigir dummy experimental):
```bash
./build/top_gl --profile
```

**Dummy experimental** (usa a matriz de dummy quando fornecida):
```bash
TOP_HISTORICAL_DUMMY_BIND=1 ./build/top_gl --profile
```

Sequência: parado → iniciar Berserk → começar a andar enquanto o efeito está ativo → parar antes de a animação visual terminar → lançar novamente.

Critérios: o personagem troca imediatamente para caminhada; não congela na pose do buff; o efeito corporal acompanha o personagem; ao parar, a mesma pose de conjuração não retorna; o efeito de chão/impacto continua independente; novo cast ainda anima.

## Validação realizada

Compilação Debug e execução de 7 testes selecionados: **7/7 passaram**, abrangendo `Play::selfTest`, a timeline SK2.7, o modo SK2.8, partículas SK2.10, modelos aninhados SK2.11, câmera/transformações SK2.12 e poses de personagem. O teste foi feito em Debug porque os fixtures antigos dependem de `assert()`.

Arquivos do `top_gl` compilados com sucesso até o subsistema de renderização OpenGL; **link/compilação gráfica completa não concluídos** neste ambiente por falta de `GL/gl.h`. `Platform/linux/main.cpp` passou na verificação de sintaxe C++17.

## Limitações

* Não é validação visual direta com o jogo DX9. A semântica de interrupção observada pelo usuário foi usada para corrigir o porte.
* Não há ainda um sistema de owner/dummy completo para NPCs, monstros e canais `Item`.
* A aura segue a posição do root quando a ligação experimental ao dummy está desligada. A rotação completa ainda está pendente.
* Efeitos `Target`/`Ground` não foram transformados em efeitos corporais indiscriminadamente.
* Não altera o crescimento da meia-lua do Illusion Slash nem o impacto em 2,5×.
