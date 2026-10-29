# SK2.17-BS — encerramento do ciclo de vida MODEL/STRIP

## Resumo

O porte já deixava de desenhar os controladores MODEL/STRIP quando `effect.age >= playTime`, mas ainda mantinha a instância em `live_` até `delay + playTime + life`. No DX9, `UpdateDelay` chama `Stop()` quando o relógio absoluto desde `Play()` chega a `playTime`; `Stop()` desliga `_bPlay`, e `CMPPartCtrl::IsPlaying()` deixa de considerar o controlador ativo. Adicionei `TOP_HISTORICAL_MODEL_LIFETIME=1`, desligado por padrão, para encerrar a retenção no máximo entre o atraso e o stop absoluto, sem reduzir o prazo de outros perfis da mesma instância.

## Evidência DX9 × OpenGL × mudança

| Aspecto | DX9 original | OpenGL antes | Mudança |
|---|---|---|---|
| Relógio de atraso/fim | `Play()` zera `_fCurPlayTime` e MODEL/STRIP inicia o controlador (`arquivo/fontes/Engine/src/MPParticleSys.cpp:907-924`). `UpdateDelay()` compara o mesmo relógio com `_fDelayTime` e `_fPlayTime`; o limite de parada é absoluto desde `Play()`, não `delay + play` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:176-233`; `arquivo/fontes/Engine/include/MPParticleSys.h:484`). | O parser lê os dois campos (`sources/Engine/src/top/EffectSystem.cpp:201`). A renderização já deixa de desenhar MODEL/STRIP em `effect.age >= playTime` (`EffectSystem.cpp:2090-2099`). | Mantive o limite de desenho. A nova opção altera só `expiresAfter`, usando `max(delayTime, playTime)` para MODEL/STRIP com `playTime > 0`; o `max` com o prazo atual da instância preserva quaisquer outros perfis/tails mais longos (`sources/Engine/include/top/HistoricalModelLifetime.h:8-20`; `EffectSystem.cpp:1052-1059`). |
| Encerramento do objeto | `Stop()` marca `_bPlay=false` para MODEL/STRIP (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1121-1154`); `CMPPartCtrl::IsPlaying()` agrega o estado dos sistemas (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:441-466`); `CMagicEff` invalida o efeito quando o controlador termina (`arquivo/fontes/Client/src/EffectObj.cpp:1202-1207`). | O gate de renderização escondia a geometria, mas o prazo calculado com a vida do perfil podia reter uma instância sem desenho (`EffectSystem.cpp:1052-1056,2090-2099`; remoção em `EffectSystem.cpp:1425-1427`). | Em modo opt-in, o registro é removido no limite de controlador quando nenhum outro prazo da instância é maior. PlayTime não positivo conserva o limite genérico já existente. |
| Tradução | O DX9 usa o tempo do controlador iniciado em `Play()`. | O GL mede `effect.age` a partir da criação da instância. | Ambos os limites são tempos desde a criação/início desse controlador. Esta etapa não altera vetores, transformação, blend, culling ou coordenadas. |

## Medição no Client

`top_par_audit` em `Client/` decodificou **1.031 PAR**, **3.476 sistemas**, sem falhas. Há **777 sistemas type 5 (MODEL)**: 167 têm `delay > 0`, 299 têm `play > 0`, e 319 têm pelo menos um desses campos não zero. O FX audit mede `model_controller_lifetime` a partir de `type in {5,6} && play > 0`: **656 sistemas**, **248 arquivos PAR** e **272 IDs estáticos de skills**. A auditoria valida a presença do helper no fonte OpenGL. O refresh também manteve 31 EFF strict-only: 31 recuperáveis em diagnóstico, 30 com PAR direto e nenhum com uso runtime provado; nenhum foi removido ou alterado. Estes são dados estáticos, não prova de execução runtime.

## Fato, inferência e desconhecido

- **FATO:** em DX9, quando `playTime > 0`, `UpdateDelay()` chama `Stop()` ao atingir o tempo absoluto; `CMagicEff` invalida quando `IsPlaying()` retorna falso. No GL anterior, MODEL/STRIP já não era renderizado a partir desse mesmo limite, mas o objeto podia continuar em `live_` por `delay + playTime + life`.
- **FATO:** 299 dos 777 MODEL do Client têm `playTime > 0`; a fixture sintética com atraso 0,2, playTime 1,5 e vida 0,4 observa a instância ainda ativa no modo antigo aos 1,51 s e removida no modo histórico.
- **INFERÊNCIA:** remover uma instância que já não pode desenhar nenhum controlador reduz retenção desnecessária; `EffectSystem` continua usando o maior prazo entre o limite deste perfil, vida-base e demais perfis/tails.
- **DESCONHECIDO:** ordem exata DX9 de `Render/UpdateDelay` versus `FrameMove` em todos os caminhos e equivalência visual. A correção não altera o desenho, e não é declaração de fidelidade visual.

## Testes e auditoria

- Fixture integrada serializa um PAR v6 com type 5, atraso e `playTime`; compara a contagem viva com o switch desligado/ligado e cobre type 6, tipos não afetados e `playTime <= 0` no helper.
- O teste do FX audit verifica que a nova medição corresponde a type 5/6 com play positivo e que a regra aponta para o helper chamado no renderer.
- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit 0.
- `cmake --build build -j$(nproc)` — exit 0, incluindo `top_gl`.
- `ctest --test-dir build --output-on-failure` — **141/141** passou.
- `build/top_par_audit --client Client --out build/sk217br-model-lifecycle-audit` — exit 0; 1.031 PAR, 3.476 sistemas, zero falhas.
- `build/top_fx_coverage_audit --client Client --out build/sk217bs-model-lifetime-audit --deps sources/auditoria-historica/dependencies.csv --table sources/auditoria-historica/fx_port_coverage.csv --source sources/Engine/src/top/EffectSystem.cpp --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp --dx9-particle-source arquivo/fontes/Engine/src/MPParticleSys.cpp --dx9-particle-header arquivo/fontes/Engine/include/MPParticleSys.h` — exit 0; 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 26 lacunas/307 IDs estáticos, deriva/falhas estruturais zero; `model_controller_lifetime` 656/248/272. Relatórios estritos: 31 EFF strict-only, todos recuperáveis em diagnóstico, 30 com PAR direto, nenhum com runtime provado.
- Fixture focal em Debug com `-fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1` — exit 0.

## Ativar, desligar e reverter

```bash
TOP_HISTORICAL_MODEL_LIFETIME=1 ./build/top_gl --profile
TOP_HISTORICAL_MODEL_LIFETIME=0 ./build/top_gl --profile
```

Ausência da variável também deixa a opção desligada. Para reverter somente esta etapa depois de aplicar o patch: `git apply -R SK217BS.patch`.
