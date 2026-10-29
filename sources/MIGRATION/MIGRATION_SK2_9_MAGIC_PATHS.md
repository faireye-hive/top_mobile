# SK2.9 — Trajetórias compartilhadas e ciclos dos modelos de voo

Data: 24/10/2026. Etapa incremental sobre **os fontes locais com SK2.8**, não sobre o HEAD Git antigo. A migração completa das habilidades permanece aberta.

## Divergências confirmadas e implementação

Referências primárias: `arquivo/fontes/Client/src/EffectObj.cpp` (`MagicList`, `Part_drop`, `Part_fly`, `Part_trace`, `Part_fshade`, `Part_arc`, `Part_dirlight`, `Part_dist`); `arquivo/fontes/Engine/src/MPEffectCtrl.cpp` (`CalculateEmission`, `Emission`, `Stop`); `arquivo/fontes/Client/src/Character.cpp::SkyEffect`; `arquivo/fontes/Engine/include/MPModelEff.h::Play`; `arquivo/fontes/Engine/src/MPModelEff.cpp::FrameMove`.

O OpenGL decodificava o índice e a velocidade, mas reutilizava interpolação linear e limitava a viagem a 0,04–3 segundos. A orientação de malhas nativas era restrita ao modo 3/Illusion Slash, e modelos externos não recebiam a mesma orientação. Trilhas de voo encerravam ao terminar seu primeiro ciclo.

`HistoricalMagicPath.h` passa a representar os modos em coordenadas X/altura/Z, com amostragem contínua em segundos:

| Modo DX9 | Comportamento recuperado | Limitação restante |
|---|---|---|
| 0 `Part_drop` | Velocidade constante, direção e encerramento na distância inicial | Colisão com terreno |
| 1 `Part_fly` | Voo linear e parada na distância inicial | Não há registros deste modo na tabela atual; teste sintético |
| 2 `Part_trace` | Trajetória para alvo estacionário, aproximação pela caixa histórica de 1 unidade | Alvo móvel, compensação de deslocamento e teto de 1,5 unidade por atualização |
| 3 `Part_fshade` | Velocidade/encerramento por proximidade; mantém escala/cópias SK2.8 | Colisão, ordem desenhar/mover, primeiro quadro, deslocamento vertical das cópias e limite anterior de 16 cópias |
| 4 `Part_arc` | Projeção radial da corda no círculo de `CalculateEmission`, aproximação pela caixa de 0,5 e prazo de 10 s | Colisão e comparação visual com DX9 |
| 5 `Part_dirlight` | Permanece na origem, orientação horizontal e duração de um ciclo dos modelos | `.par` acoplados deste modo e casos sem modelos requerem comparação |
| 6 `Part_dist` | Direção/velocidade constante e encerramento pela distância | Distância substituída pela forma/alcance da habilidade |

A função DX9 `PointInstrPointRange` verifica X/Y do plano, ignorando altura. Não é uma distância esférica. A implementação calcula a entrada na caixa sem depender de atravessá-la num quadro; evita perder o término em FPS baixo. Nos modos lineares, o excesso de tempo é limitado à chegada. O DX9 pode apresentar um pequeno excesso em `Part_dist`; não se reproduz esse artefato dependente de FPS.

**Velocidade:** `Character::SkyEffect` chama `SetVel(sSkySpd)`, que divide por 100 e substitui `magicsingleinfo.nVel`. Por isso, a duração explícita enviada por `Play` continua tendo precedência; dela recupera-se a velocidade planar. A velocidade da magia é usada quando não há duração explícita. Removido o teto artificial de três segundos. Com alturas explícitas, o comprimento percorrido é 3D.

**Alturas:** `EffectEvent` aceita duas alturas opcionais. A ponte da cena fornece as alturas do terreno com a âncora anterior de 0,9. Isso permite interpolação 3D e o arco sem acompanhar artificialmente cada ondulação do chão. **Ainda não são matrizes de dummies**. Chamadores antigos continuam com sua âncora anterior.

**Modelos:** malhas nativas e externas recebem yaw e pitch genéricos do voo após a transformação local. Billboards continuam voltados à câmera. Cada arquivo `.eff` tem seu período calculado pelo máximo das **somas das durações de suas trilhas**. No DX9 `Play(false)` significa loop; `Play(true)` significa uma execução. O voo repete por arquivo, mantendo camadas curtas desligadas até o término da mais longa. `Part_dirlight` executa uma vez.

**Encerramento/Result:** todos os componentes de um voo histórico encerram em sua chegada; o `.par` Result é criado na posição calculada, com o excesso de tempo preservado e uma única emissão. Não se mantém um emissor de voo parado sobre o destino. O dano e os canais normais de impacto de `Play` continuam com a timeline anterior; sua sincronização com a chegada por proximidade é uma pendência explícita.

Não houve alteração no tradutor de blend, nas regras de combate, saves, NPCs, itens, missões ou UI. O multiplicador provisório 2,5× do impacto de Illusion Slash foi preservado. Nenhuma ampliação global foi introduzida.

## Testes

`UI05SK29Tests.cpp` testa os sete índices, equações lineares, caixa de proximidade, arco radial, modo estacionário, orientação em quatro quadrantes e inclinação, precedência da duração, velocidade da tabela quando não há override, voo superior a três segundos, repetição e equivalência de passos grandes/pequenos. Há execução de `EffectSystem` com `.eff` binário sintético, sem GL.

A primeira implementação passou na suíte de 82 testes. Após SK2.10, a suíte conjunta contém 83 testes; resultados finais e logs estão no pacote. Compilação/teste do patch isolado também são registrados no pacote após sua verificação.

Os testes sintéticos não comprovam fidelidade visual. Os modos 0/2/3/4/6 permanecem parciais pelos limites acima.
