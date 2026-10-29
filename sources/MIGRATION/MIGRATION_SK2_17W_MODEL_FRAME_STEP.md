# SK2.17-W — avanço histórico dos quadros de partículas MODEL

## Resumo

O FX audit atualizado do Client encontrou 777 sistemas `type=5` em 387 `.par`; 433 sistemas em 211 arquivos têm mais de uma chave de curva e estão ligados a 54 skills. Para estes dados, havia uma diferença medida entre o controlador DX9 discreto e o cálculo contínuo do porte GL. `TOP_HISTORICAL_MODEL_FRAME_STEP=1` ativa a linha do tempo discreta; a opção fica desligada por padrão.

## Evidência: DX9 original x porte atual x mudança

| Aspecto | DX9 original | Porte OpenGL antes/depois | Mudança |
|---|---|---|---|
| Quadro corrente | `GetCurFrame` soma o delta, avança no máximo um quadro e zera o tempo residual ao cruzar o intervalo (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). | Antes, `EffectSystem.cpp` obtinha a fase por `fmod(particleAge, profile.life)`; agora esse caminho continua sendo o padrão (`sources/Engine/src/top/EffectSystem.cpp:1825-1835`). | O modo histórico mantém `frame` e `frameTime` por instância e perfil; uma atualização avança no máximo um quadro e descarta excedente (`sources/Engine/include/top/HistoricalModelTimeline.h:33-60`). |
| Duração e número de quadros | `_CreateModel` define `m_fFrameTime = m_fLife / _wFrameCount`, reinicia cursor/tempo e usa um único particle slot (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3074-3087`). | O porte já decodificava vida e `frameCount`; seu render MODEL usa os dados para amostrar curva (`sources/Engine/src/top/EffectSystem.cpp:1831-1837`). | O cálculo histórico usa `profile.life / profile.frameCount`, depois amostra as curvas pela fase discreta. O relógio recebe o `dt` de `EffectSystem::step` (`sources/Engine/src/top/EffectSystem.cpp:1060-1090`). |
| Fim e reinício | Quando o índice chega ao total, `_FrameMoveModel` chama `_CreateModel` e retorna antes de recalcular tamanho/cor naquele update (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3101-3110`). | A curva GL antes repetia a fase com `fmod`; não representava explicitamente o update de reinício que conserva a última amostra. | O modo mantém a última fase nesse update e começa o próximo ciclo no quadro zero, conforme a ordem observada no código DX9 (`HistoricalModelTimeline.h:38-60`). |
| Dados efetivamente usados | O cursor recebe `*pPart->_pfDailTime`, não uma taxa fixa codificada (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3101`). | O GL recebe `dt` por `step(dt)` e não converte coordenadas ou convenções de matriz nesta alteração. | O passo discreto segue o delta entregue ao GL. Equivalência do relógio em execução ainda precisa de comparação com DX9. |

## Alterações

- Novo `historical_model_timeline::Timeline`, isolado em `sources/Engine/include/top/HistoricalModelTimeline.h`.
- Estado por instância/índice de perfil no `EffectSystem`; somente perfis `type=5` usam esse relógio.
- A auditoria `top_fx_coverage_audit` mede `feature:model_frame_step` diretamente de `type==5 && frames>1`; a tabela de cobertura mantém o nome da opção e a explicação, sem inventariar arquivos manualmente.
- A saída do Client registrou 433 sistemas, 211 arquivos e 54 skills para a feature; 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas e 2.832 partes `.eff`; zero falhas, `drift=0`, 24 lacunas totais e 307 skills alcançadas por ao menos uma lacuna. `fx_decode_failures.csv` contém só a linha de cabeçalho.

## Compilação e testes executados

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Resultado observado: configuração e build completos, incluindo `top_gl`, concluídos com exit code 0; CTest **124/124**. O teste novo verifica o default desligado, avanço único, descarte de excedente, retorno de ciclo e delta negativo. O mesmo fixture passou com AddressSanitizer e UndefinedBehaviorSanitizer (`ASAN_OPTIONS=detect_leaks=0`).

Auditoria reproduzível:

```bash
./build/top_fx_coverage_audit --client Client --out build/sk217w-final-fx-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

## Limitações e validação visual

O mecanismo é opt-in e não altera o resultado padrão. O teste automatizado verifica a máquina de estados, não compara pixels com o DX9. Não foi feita comparação visual; portanto, esta implementação não é declarada fiel/validada visualmente. O comportamento ainda pode diferir se o GL e o Client DX9 receberem deltas de atualização diferentes. A tabela cobre dados cujo `.par` decodificou; ela não prova que todos os sistemas são alcançados em runtime.

Para desfazer somente esta etapa: `git apply -R SK217W.patch`.

## Checklist visual

Compare a mesma skill/efeito e os mesmos momentos entre DX9 e GL: início, cruzamento de quadro com `dt` normal, quadro final e retorno ao primeiro quadro. Execute o GL com e sem `TOP_HISTORICAL_MODEL_FRAME_STEP=1`; mantenha constantes as opções históricas usadas no ensaio anterior. Confirme a mudança visual antes de considerar qualquer alteração de padrão.
