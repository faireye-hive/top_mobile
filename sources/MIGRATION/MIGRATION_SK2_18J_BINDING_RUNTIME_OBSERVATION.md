# SK2.18-J — observar vínculos com assets reais em prévias OpenGL

## Resultado e base

I já compõe a matriz de filhos não-billboard; G já recupera o centro de filhos billboard. H comparou sceneffectinfo TXT/BIN. Esta etapa acrescenta diagnóstico para observar essas duas regras, sem substituir seus testes ou declarar habilidades completas visualmente fiéis.

Foi encontrada uma lacuna no próprio diagnóstico: `--skill-preview` criava eventos sem identificar o jogador como dono/alvo. Mesmo com G/I ligados, esses eventos não qualificavam os vínculos. `TOP_HISTORICAL_SKILL_PREVIEW_BIND=1` qualifica cast como originado no jogador e hit como destinado ao jogador exibido. O endpoint artificial de flight continua sem identidade inventada. Padrão desligado.

`TOP_HISTORICAL_BINDING_TRACE=1`, também desligado por padrão, registra os ramos efetivamente selecionados durante a emissão CPU de geometria. Não afirma que a malha ficou visível na imagem, nem que o combate real acionou o canal. A captura OpenGL e o registro CPU são evidências complementares, com escopos diferentes.

## Original DX9 × GL anterior × mudança

| Cadeia | Original DX9 | Porte / mudança J |
|---|---|---|
| Dono conhecido | `arquivo/fontes/Client/src/Character.cpp:940-952` faz SelfEffect no próprio personagem e passa seu dummy/raiz. | `sources/Platform/linux/main.cpp:1331-1334` antes emitia só kind/coordenadas/ID/duração. Agora chama `sources/Engine/include/top/HistoricalSkillPreviewBinding.h:6-14` para qualificar o dono conhecido sob switch. A representação `sourceIsPlayer` já existia; não se adivinha dono por distância. |
| Efeito recebido | `arquivo/fontes/Client/src/EffDelay.cpp:35-38` chama SelfEffect no alvo real. | A prévia hit já era emitida no jogador; J usa o índice de bridge existente `-1`, sem afirmar que esse jogador seja o alvo correto de um combate não executado. `EffectSystem.h:60` documenta a identidade; `main.cpp:2833-2853` fornece matrizes de alvo. |
| Vínculo e desenho | `arquivo/fontes/Engine/src/MPParticleSys.cpp:1205-1227,1526-1531` distingue vínculo MODEL/STRIP; `MPModelEff.cpp:820-846` distingue centro billboard e composição da malha. | J registra disponibilidade da matriz e as escolhas reais de G/I depois de emitir vértices: `sources/Engine/src/top/EffectSystem.cpp:2835-2850,2936,3027`. Não replica a decisão em uma lista manual de skills. |
| Índices do arquivo | DX9 percorre todos os subefeitos do arquivo (`arquivo/fontes/Engine/src/MPResManger.cpp:1337-1345`). | O GL filtra sistemas sem recurso e partes sem malha/textura. O índice do vetor resultante não é necessariamente o índice original. J preserva `sourceSystem/sourceParticle/sourcePart` no loader (`EffectSystem.cpp:217-218,781-782`; `EffectSystem.h:164-169`) para o audit não atribuir uma observação à parte errada. Sem mudança no formato binário ou na ordem do desenho. |

O rastreio escreve CSV com prefixo `[fx-binding]` em stderr: skill/visual/evento, instância/idade, dono/dummy, PAR/EFF/modelo, índices originais, disponibilidade, ramo selecionado, quantidade de vértices e os 16 elementos da matriz. O CSV preserva os valores row-vector MindPower; a conversão XYZ/Z-up→GL X/altura/Z continua no emitter existente, conforme I. A ferramenta de leitura não transforma matrizes nem acrescenta dados ausentes.

## Ferramentas e resultados reais

`Tools/run_binding_previews.py` deriva o plano do CSV de candidatos G/I, cria uma sessão com `Client/user` copiado e compartilha os assets por links. Os subprocessos usam essa sessão como diretório corrente. O programa continua podendo escrever nos **saves copiados**, pois usa o startup normal do jogo; o audit não executa aprendizado de skill ou consumo de SP. Os resultados não provam que cada classe/skill seja utilizável em combate pelo personagem escolhido.

Primeira bateria, personagem Lance, tempo inicial 0,35 s e mais 18 atualizações de 1/30 s do modo shot:

- 44 pares skill/canal derivados dos candidatos; todos com exit 0 e captura produzida.
- 6.502 linhas de rastreio, 196 partes observadas; 165/203 combinações candidatas com geometria não vazia, matriz disponível e regra aplicada, em 40 IDs.
- 38 combinações não observadas. Comparação com os dados PAR mostrou 15 cujo atraso era 1 s, depois da janela inicial de 0,95 s. Foi feita segunda bateria nos três IDs correspondentes (24/239/294), com tempo inicial 1 s; três execuções, todas exit 0 e com captura. Não alterar atraso dos assets para fazê-los aparecer.

Resultado combinado (`build/runtime-binding-sk218j/all-observations`):

| Regra | Candidatas estáticas | Observadas com geometria, dummy e regra aplicada |
|---|---:|---:|
| Matriz de filho não-billboard (I) | 181 | 159 |
| Centro de filho billboard (G) | 22 | 21 |
| Total | 203 | **180**, cobrindo os **41 IDs** candidatos |

São 47 execuções de prévia, 6.883 registros e 211 partes observadas no total. Cada execução produz a captura inicial e a captura final do modo shot. Uma observação de uma parte não significa que todas as partes da mesma habilidade foram exercitadas. Os relatórios conservam `declared_scope=preview` e `visual_validation=not_performed`; o escopo é declarado pelo chamador e não autenticado por um log externo.

**FATO:** o executável OpenGL conseguiu abrir o Client real neste ambiente, gerar as capturas e selecionar os ramos instrumentados usando matrizes de poses reais. O sandbox impedia sockets X11; Xvfb funcionou após execução autorizada fora dessa restrição. Foi usado `LIBGL_ALWAYS_SOFTWARE=1`; não é medição de desempenho da GPU do usuário.

**INFERÊNCIA:** os vínculos que antes eram apenas candidatos estáticos podem ser exercitados pelo caminho de prévia corrigido. Isso fortalece a evidência de integração de G/I, mas não substitui os callsites de combate nem a comparação com o DX9.

**DESCONHECIDO:** concordância visual DX9/GL e execução desses mesmos canais em batalha. Illusion Slash continua **aproximado**, não aprovado visualmente. PNGs de referência GL: `build/runtime-binding-sk218j/illusion-slash-preview.png` e `taunt-preview.png`; imagens inspecionadas como capturas do porte, sem imagem DX9 para comparar.

### Saves

Cada bateria registra em `runs.json` a igualdade dos hashes de todos os arquivos originais de `Client/user` antes/depois de seus subprocessos: `original_user_unchanged=true` nas duas baterias. A fixture automatizada também confirma que uma gravação de um processo filho atinge somente a cópia isolada.

Não afirmar igualdade durante o turno inteiro: o snapshot inicial de 01:12 difere em `offline-save-1002.txt` e seu `.bak`, com timestamps 01:15, anteriores à bateria automatizada. A autoria dessa alteração não foi comprovada; nenhum desses arquivos foi restaurado/sobrescrito para esconder a diferença. Os hashes ficaram estáveis durante as baterias isoladas. Dados concorrentes do usuário não devem ser revertidos por este trabalho.

## Lacunas práticas que a observação separou

Restam **23 combinações candidatas não observadas** após as duas janelas. Não foram classificadas como assets inúteis:

- **Oito partes comprovadamente barradas pelo loader:** índices 8–11 de `bdlights1.eff` e `bdlights5.eff`, no ID 282. O GL interrompe após oito tracks (`EffectSystem.cpp:737`); o DX9 dimensiona e percorre a contagem do arquivo (`MPResManger.cpp:1337-1345`). Essa lacuna deve receber um patch próprio e opt-in, com medição global e fixture com mais de oito partes. J não altera o limite.
- As outras 15 combinações não foram isoladas nesta etapa: duas `foot.eff`, uma `speed up.eff`, cinco `01010003.eff`, uma `bdwinda2.eff`, duas `zzfb5.eff`, duas `zzfb2.eff`, uma `zzfb1.eff`, uma `zzfb3.eff`. Ausência no rastreio pode decorrer de janela/ciclo, resolução de textura/malha, filtro do loader ou outro estágio anterior ao desenho. Precisa de diagnóstico específico; não inventar dummy, duração ou textura.

Reaudits atuais, todos exit 0:

- FX: 1.031 PAR / 1.227 EFF / 3.476 sistemas / 2.832 partes, 27 gaps/307 IDs, drift 0, falhas estruturais 0 (`build/fx-audit-sk218j`).
- PAR: 1.031 decodificados, zero falhas, 110 referências ausentes (`build/par-audit-sk218j`).
- Vínculos: mesmos 203 candidatos G/I e 13 canais sem recurso (`build/effect-binding-sk218j`). O hash do CSV coincide com o usado nas prévias.
- Os 31 EFF strict-only continuam individualizados em `fx_eff_usefulness.csv`: todos recuperáveis pelo parser diagnóstico, 30 com consumidor PAR direto e `fboomerff.eff` sem consumidor direto provado. Os candidatos de vínculo desta bateria não incluem esses 31; seu uso runtime continua não comprovado. Nenhum asset removido e nenhum suporte estrito enfraquecido.

## Testes executados

- Debug completo, incluindo `top_gl`: exit 0 (`build/sk218j-final-configure.log`, `build/sk218j-final-build.log`). CTest completo: **147/147**, exit 0 (`build/sk218j-ctest.log`).
- Dois novos CTests: identidade da prévia/CSV e audit Python com **sete casos**, todos executados com binários fornecidos, sem skips. Cobrem qualificação opt-in, flight sem ator inventado, CSV, matriz não finita, ausência de trace, índices/nomes exatos, geometria vazia, dummy ausente, uso do rastreio no runtime sintético PAR/EFF/LGO e isolamento dos saves. A fixture com sistema/filho anterior descartado confirma os índices originais 1/1.
- A primeira versão da fixture de isolamento tentou executar um script temporário e recebeu PermissionError. Foi substituído pelo processo filho C++ compilado do teste, mantendo as mesmas verificações de cópia/gravação. Nenhum teste anterior foi alterado.
- ASan/UBSan Debug: build focal exit 0, CTest G/I/J **4/4**, exit 0 (`build/sk218j-asan-ctest.log`). Leak detection desabilitada com `detect_leaks=0`; UBSan usa `halt_on_error=1`.
- Testes existentes de framebuffer sob Xvfb: blend e filtros, ambos exit 0. Blend cobriu os 19 pares e a política de alfa do destino; filtro produziu nearest=255, linear=128 e restauração legacy=128 (`build/sk218j-blend-gpu.log`, `build/sk218j-filter-gpu.log`). Não são comparação artística com DX9.

## Reproduzir, desligar e reverter

Da raiz:

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure

build/top_effect_binding_audit --client Client --out build/effect-binding-sk218j
xvfb-run -a -s '-screen 0 1280x720x24' python3 -B sources/Tools/run_binding_previews.py \
  --candidates build/effect-binding-sk218j/bound_billboard_channels.csv \
  --out build/binding-preview-new
python3 -B sources/Tools/binding_trace_audit.py \
  --log build/binding-preview-new/combined.log --scope preview \
  --candidates build/effect-binding-sk218j/bound_billboard_channels.csv \
  --out build/binding-preview-new-observations
```

O diretório `--out` do runner deve ser novo, para não confundir capturas antigas com execução nova. `--character 1/2/3/4`, `--time 0..10` e `--timeout` configuram o experimento, não regras históricas do motor. O runner deriva todos os casos do CSV e registra o hash do executável, do CSV, flags e códigos de saída. Não execute múltiplas baterias que escrevam no mesmo diretório.

Para uma sessão normal, sem o runner:

```bash
TOP_HISTORICAL_BINDING_TRACE=1 \
TOP_HISTORICAL_BOUND_MODEL_MATRIX=1 \
TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1 \
TOP_HISTORICAL_DUMMY_BIND=1 \
TOP_HISTORICAL_TARGET_DUMMY_BIND=1 \
TOP_HISTORICAL_ITEM_DUMMY_BIND=1 \
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile 2> build/binding-gameplay.log
```

Executar uma habilidade no jogo e depois usar o leitor com `--scope gameplay` registra uma sessão declarada de combate. O leitor não transforma uma prévia em combate por alterar esse argumento. `TOP_HISTORICAL_SKILL_PREVIEW_BIND=1` só é necessário para `--skill-preview`; não altera o fluxo normal de combate. Para desligar J, usar `TOP_HISTORICAL_BINDING_TRACE=0` e `TOP_HISTORICAL_SKILL_PREVIEW_BIND=0` ou remover as variáveis. Trace pode reduzir FPS por escrever em stderr; não usá-lo para conclusões de desempenho.

Checklist visual: comparar a mesma habilidade, personagem/equipamento, câmera, pose e instante com DX9; conferir centro e orientação no dummy e comportamento no chão; alternar G e I separadamente. Não aprovar o conjunto de uma skill a partir de uma parte observada. Preservar os flags de textura/blend já usados; não aumentar escalas para compensar diferenças.

`SK218J.patch` é incremental sobre I, com caminhos `sources/`. Em outra cópia da base: `git apply --check SK218J.patch`, `git apply SK218J.patch`. Reverter: `git apply -R SK218J.patch`. O workspace atual já contém as alterações: não reaplicar. A checagem de patch usa uma cópia da base pré-J capturada, não os ZIPs antigos que não contêm SK2.18.

## Próximo passo

Corrigir o limite de oito filhos EFF em patch separado, derivando a contagem do documento decodificado e conservando os limites de segurança do parser. **Um ID de skill comprovadamente afetado neste cruzamento: 282**, com oito combinações cortadas; o alcance global do limite ainda precisa ser medido.
