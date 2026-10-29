# SK2.18-H — auditoria autenticada da tabela de efeitos do Client

## Escopo e o que já existe

G recuperou uma regra compartilhada de centro do billboard vinculado, atrás de `TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=1`, com 22 combinações candidatas em 8 IDs estáticos. D/E cobrem o controlador Part_trace e a consulta da grade de colisão; F preservou a flag PAR de direção e auditou o vínculo. Nenhum desses mecanismos é reimplementado aqui.

O audit G deixou 13 canais sem resolução de recurso. H verifica se essa lacuna é do renderer, da resolução de nomes ou dos dados, incluindo o BIN realmente selecionado pelo padrão do fonte DX9. É uma ferramenta somente-leitura, sem mudança de runtime, API ou Client; não precisa de novo switch de comportamento. Não confundir os 13 canais com 13 mecanismos faltantes, nem os 8 candidatos G com 8 habilidades integralmente recuperadas.

## Original DX9 × porte × mudança

| Regra | Original DX9 | Porte atual / mudança H |
|---|---|---|
| Escolha da tabela | `arquivo/fontes/Client/src/GameConfig.cpp:10-14` inicializa `g_bBinaryTable=TRUE`; `arquivo/fontes/Libraries/common/include/TableData.h:270-292` força o ramo BIN mesmo quando o callsite pede TXT. `GameAppInterface.cpp:1351-1359` carrega sceneffectinfo. | `sources/Engine/include/top/SceneEffectCatalog.h:24-49` lê TXT. H compara os dois modos antes de concluir que um recurso falta: `sources/Tools/scene_table_audit.py:173-219`. Nenhum leitor runtime foi trocado. |
| Binário ativo | `arquivo/fontes/Libraries/common/src/TableData.cpp:3-85` autentica AES-GCM com tag de 12 bytes e copia registros nativos Win32; este `.cpp` está no projeto (`Libraries/common/Proj/Common.vcxproj:126`). O leitor Base64 no header está dentro de comentário iniciado em `TableData.h:420`. | O audit extrai parâmetros criptográficos do `.cpp` ativo e só interpreta o payload depois da autenticação (`scene_table_audit.py:56-73,144-170`). Não copia constantes do Client para o código da ferramenta. |
| Layout e semântica | `TableData.h:39-51` define CRawDataInfo; `arquivo/fontes/Client/src/EffectSet.h:108-149` define CMagicInfo. `EffectSet.h:195-223` define colunas/flags/dummies. `TableData.h:611-640` fornece ID/nome e deslocamento de ParamList. | Layout e índices de colunas são extraídos dessas declarações/atribuições (`scene_table_audit.py:33-97`). Tipos/pointers seguem ABI Win32 de 32 bits, mesmo em Linux 64 bits. Campos desconhecidos/contratos não suportados interrompem o audit em vez de assumir offsets. |
| Busca de arquivo | `arquivo/fontes/Engine/src/MPResManger.cpp:2085-2097` usa `stricmp` sobre o **nome completo**, retorna -1 se não encontrar; `:2153-2200` enumera `*.par`. `arquivo/fontes/Client/src/EffectObj.cpp:801-805,853-863` usa o nome da tabela e falha se o controlador não existe. | H distingue ausência literal de diferença apenas de caixa (`scene_table_audit.py:199-219`), sem corrigir extensão. GL já elimina recursos declarados não desenháveis (`sources/Engine/src/top/EffectSystem.cpp:1071-1074`); regressão existente `sources/tests/UI05GTests.cpp:363-397`. |
| ID sem tabela | `EffectObj.cpp:669-710` separa magias/grupos e consulta GetMagicInfo para IDs de cena; registro ausente retorna FALSE. | `SceneEffectCatalog.h:53-55` retorna nullptr se o ID não existe. O audit mantém o diagnóstico distinto de arquivo ausente (`scene_table_audit.py:220-238`). Tabela do servidor é evidência separada, não fallback. |

### Derivação e limites do formato

O layout encontrado tem base de 108 bytes, registro total de 208 bytes e 14.000 slots. Não se usa `sizeof(void*)` do Linux para ler o pointer serializado. O audit confirma nIndex/nID do slot ativo como **checagem adicional de coerência desta tabela**, não como regra universal de validade de qualquer tabela DX9. Autenticação, tamanho e coerência são verificados antes de criar os relatórios.

Os campos comparados são nome do recurso, nome descritivo, photo, tipo de efeito/objeto, quantidade/lista de dummies, dummy secundário, altura, duração, luz e tamanho-base. `nPhotoTexID` é cache de runtime, não coluna TXT. Estados de cache/ponteiros da base também não são comparação semântica TXT/BIN. Inteiros seguem prefixo `atoi`; floats seguem prefixo `atof` e arredondamento float32, conforme `arquivo/fontes/Libraries/Util/include/util2.h:148-155`. Separadores repetidos seguem `Libraries/Util/src/util2.cpp:297-348`. A ferramenta suporta as declarações do fonte fornecido; não é compilador C++ ou leitor universal de todas as variantes de tabelas.

## Medição real

Comando executado da raiz, exit 0:

```bash
python3 -B sources/Tools/scene_table_audit.py \
  --client Client --original arquivo/fontes \
  --out build/scene-table-audit-sk218h \
  --binding-channels build/effect-binding-sk218g/bound_billboard_channels.csv \
  --server-table arquivo/servidor/server/GameServer/resource/sceneffectinfo.txt
```

Requer Python 3 e `python3-cryptography` disponíveis no ambiente. Relatórios: `scene_records.csv`, `scene_differences.csv`, `unresolved_channels.csv`, `summary.json`. O JSON contém hashes dos dois inputs Client e dos fontes examinados. O parâmetro servidor é opcional e nunca altera a resolução do Client. Sem `--binding-channels`, o audit continua comparando o catálogo completo, sem inventar canais.

**FATO medido:** autenticação aprovada; 1.093 registros ativos no BIN e 1.093 no TXT; 0 IDs diferentes, 0 campos semânticos diferentes. No catálogo BIN há 73 nomes sem arquivo literal em `Client/effect` e zero casos resolvidos somente por diferença de maiúsculas/minúsculas. Esses 73 são registros, não contagem de skills ou prova de execução.

| Efeito | TXT e BIN do Client | Canais pendentes do audit G | Conclusão estática |
|---|---|---|---|
| 113 | `heallight.pa` (`Client/scripts/table/sceneffectinfo.txt:37`) | skill 7 target; 97,117,124,279 actor | Nome literal ausente. `heallight.par` existe, mas o original não acrescenta extensão; não há alias comprovado. |
| 145 | `01000017.par` (`sceneffectinfo.txt:69`) | 20 target; 61 actor | Arquivo ausente. Não substituir por outro PAR visualmente semelhante. |
| 221 | Sem registro nos dois modos | 331,344,353,362,371 actor | No servidor existe `fushe.par` (`arquivo/servidor/server/GameServer/resource/sceneffectinfo.txt:145`), mas o Client não consulta essa tabela. Não importar esse mapeamento automaticamente. |
| 880 | Sem registro nos dois modos | 460 actor | Não foi encontrado mapeamento histórico suficiente para criar o efeito. |

Total: 13 canais em 13 IDs de skill, sendo 7 nomes de arquivo ausentes e 6 IDs sem registro. O CSV preserva cada canal individual e marca `runtime_use_proven=no`. Todos têm o mesmo impedimento em TXT/BIN; H não encontrou divergência de catálogo que justificasse mudar o carregamento GL para resolver esses casos.

Também foram examinados os índices dos compactados `arquivo/Antigo.zip`, `sources.zip`, `sources_251146.zip`, `ALLPATCH.zip` e `variospatch.zip`, sem extrair pacotes inteiros para o repositório. Não apareceu o recurso `01000017.par` nem um recurso chamado `heallight.pa`. A cópia `arquivo/cliente/Client` contém os mesmos TXT/BIN do Client atual. A tabela do servidor com ID 221 foi tratada separadamente. Isso descreve os materiais encontrados, não prova ausência em qualquer distribuição do jogo.

**INFERÊNCIA:** estes 13 canais não representam uma funcionalidade nova a implementar no renderer para este conjunto de assets: a cadeia DX9 também não resolve seus nomes/IDs literais. **DESCONHECIDO:** execução efetiva dessas skills/canais em sessão, overrides de configuração ou dados de outra versão do Client. O padrão BIN foi comprovado no fonte; não foi observado um processo DX9 em execução.

## Testes executados

- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON`: exit 0. Cache confirma Debug, C++ debug `-g`, sem `-DNDEBUG` imposto pelo build de Release.
- `cmake --build build -j4`: exit 0, incluindo `top_gl`. Log `build/sk218h-build.log`.
- `ctest --test-dir build --output-on-failure`: 144/144, exit 0. Log `build/sk218h-ctest.log`.
- O novo CTest `sk218h_scene_table_audit` executa 10 casos unittest com fontes sintéticos, sem Client nem pasta arquivo: autenticação, roundtrip dos campos, tag corrompida, registro parcial autenticado, ID/slot divergente, fonte não suportado, diferenças entre modos, diferença de caixa, extensão literal, servidor separado, ausência em um modo e conversões/dummies. Não enfraquece teste antigo.
- CMake registra esse teste se Python3/cryptography estiverem presentes e informa explicitamente quando não estão; não apresenta ausência da dependência como teste aprovado.
- ASan/UBSan não foram reaplicados nesta etapa: não houve mudança de C++ e o novo audit/teste é Python. A fixture C++ de vínculo em G teve execução focal com ASan/UBSan registrada no relatório G.
- Não houve sessão jogável nem comparação visual DX9/OpenGL. As 31 recusas EFF strict-only do audit G continuam separadas dos canais ausentes: 31 recuperáveis pelo parser diagnóstico, 30 com PAR direto, uma sem consumidor estático direto comprovado; nenhum asset foi removido. Este audit de tabela não mede execução de EFF.

## Aplicar/reverter e checklist

`SK218H.patch` é incremental sobre o estado pós-G, apenas com caminhos `sources/`. Não inclui modificações locais anteriores. Em uma cópia da base pós-G:

```bash
git apply --check SK218H.patch
git apply SK218H.patch
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Reversão: `git apply -R SK218H.patch`. O workspace atual já contém H: não reaplicar aqui. A ferramenta não grava no Client; arquivos de saída ficam no diretório passado em `--out`.

Checklist visual: H não altera o desenho e não oferece uma nova aparência para aprovar. Para comparar a regra compartilhada G, manter os switches de vínculo e de textura/blend habituais, alternar **somente** `TOP_HISTORICAL_BOUND_BILLBOARD_CENTER=0/1`, e seguir o checklist de `MIGRATION_SK2_18G_BOUND_BILLBOARD_CENTER.md` com mesmo ator, alvo, pose e câmera. Não usar os 13 canais sem recurso como amostra de aprovação visual.

## Próximo mecanismo comum

O topo medido do ranking continua `type:6` (1.041 instâncias, 504 arquivos e 242 IDs estáticos alcançados, audit G). Investigar a composição de matriz dos filhos **não-billboard**, distinguindo o ramo `MPModelEff.cpp:845-846` da substituição de centro do filho billboard já coberta por G, e medir consumidores elegíveis antes de implementar. O número de skills desse subcaso ainda é desconhecido; os 242 são alcance do tipo inteiro.
