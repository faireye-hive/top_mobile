# Tales of Pirates OpenGL — Migração 05-AA

## Objetivo e fonte histórica

Implementar a **perfuração de encaixes** (*milling/fusion*) do `forge.lua` original, sem alterar a câmera, renderizador, otimizações de FPS, sistema de forja de gemas nem formato de saves da 05-Z.

Fontes usadas: `Client/scripts/offline/script/calculate/forge.lua`, funções `can_milling_item_main`, `get_milling_money_main`, `begin_milling_item`, `Check_Jiaguji`, `Check_Cuihuafen`, `Check_CG_damo`, `Damo_ChengGong`; `Client/scripts/offline/calculate/variable.lua`, campos literais `Server.Socket.Limit = 3` e `Server.Socket.Cost = 50000`. As mesmas regras constam do GameServer original que o usuário enviou. Nenhum Lua é executado.

## Operação implementada

Para abrir **um** novo encaixe na **instância** do equipamento, utilizar:

- Equipamento em slot próprio da bolsa, com `count=1`, não *apparel* (valor `ITEMATTR_MAXURE == 25000`, no `iteminfo.txt` coluna 76).
- Capacidade de encaixes da tabela `iteminfo.txt` (coluna 77); também respeita o teto configurado por `Server.Socket.Limit` e o formato persistente de 0 a 3 encaixes.
- Uma unidade de **Equipment Stabilizer, ID 890**, e uma de **Equipment Catalyst, ID 891**, em posições distintas; ambas as ordens são aceitas.
- Ouro `novo encaixe × Server.Socket.Cost`: **50.000**, **100.000**, **150.000**.
- O script histórico presente no acervo define **100% de chance** de abertura dos encaixes 1–3 (`Percentage_Random(1.00)`). A versão offline reproduz esse resultado determinístico.

Todas as condições são verificadas **antes** de retirar materiais ou ouro. Em sucesso, apenas `socketCount` aumenta; `UID`, IDs e níveis das gemas já aplicadas, propriedades individuais, posição equipada e atributos não mudam. A operação é salva pelo formato `Z1` introduzido na 05-Z. Saves antigos continuam legíveis.

Um erro não consome ouro ou materiais. Não são introduzidas probabilidades extras, equipamentos fictícios ou comandos de GameServer.

## Atalho com confirmação (interface provisória)

Para testar sem cliente/servidor, indique os três **índices atuais da bolsa**, começando em zero:

```bash
./build/top_gl --socket-equip 0 --socket-stabilizer 1 --socket-catalyst 2
```

Depois pressione **Alt+M**. Quando os índices identificam equipamento e materiais válidos, o cliente abre a janela existente de **Confirmar / Cancelar**, mostrando nome do equipamento e custo. A confirmação valida novamente UID do equipamento e dos dois materiais para evitar trabalhar numa cópia errada se a bolsa mudou.

**M sem Alt** continua a viajar entre mapas. **Alt+O** mantém a abertura de encaixes por GM, exclusivamente para testes; **Alt+L** continua concedendo ouro de teste, e `--gm-item 890`, `--gm-item 891` fornecem os materiais em execuções distintas do cliente, mediante **Alt+U**.

A UI do ferreiro **ainda não é a janela histórica de Fusion/Blacksmith** e ainda depende de índices fornecidos na linha de comando; é um procedimento de teste com confirmação. A recuperação visual completa (arrastar os três itens para posições do painel do ferreiro, seleção do NPC, custos e prévia) fica para uma migração seguinte.

Faça backup de `Client/user/` antes de efetuar operações de forja/GM.

## Arquivos alterados

- `Engine/include/top/SocketRules.h`: parser de configuração literal, custo, limites, materiais e chance histórica; falha fechada se a configuração não for encontrada.
- `Engine/include/top/Play.h`, `Engine/src/top/Play.cpp`: tipo de equipamento, quantidade de encaixes permitidos, método `socketCost`, `socketEquipment`, aplicação por UID, gravação normal e regressão integrada.
- `Platform/linux/main.cpp`: opções `--socket-*`, **Alt+M**, confirmação, guardas de UID, preservação do atalho M.
- `tests/UI05AATests.cpp`, `CMakeLists.txt`: 41ª suíte de regressão.

## Validação no ambiente de desenvolvimento

- Teste novo `ui05aa_historical_socket_fusion` passou: configurações extraídas literalmente, custos, materiais em ambas as ordens, restrição a apparel, limites e ausência de arquivo configurado.
- `Play::selfTest()` **recompilado a partir da 05-AA** passou: duas espadas iguais com UIDs distintos, três operações consecutivas, consumo correto de materiais empilhados, recusas sem cobrança, persistência de três encaixes e rejeição de apparel.
- 41/41 suítes CTest passaram: as outras 39 suítes independentes não alteradas reaproveitaram executáveis da 05-Z, o teste `Play` alterado e o teste 05-AA foram recompilados.
- `Play.cpp` e `Platform/linux/main.cpp` passaram na verificação de sintaxe C++17 `-O2`.
- Este ambiente **não dispõe de headers OpenGL**; compilação gráfica real, feedback dos botões e teste visual exigem o Linux do usuário.

## Execução

```bash
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --socket-equip 0 --socket-stabilizer 1 --socket-catalyst 2

cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

**Clock skew** em builds `make`: os arquivos extraídos podem vir com horários futuros por diferenças de relógio/horário entre máquinas. Se persistir, após extração, sincronize o relógio do SO e execute `find sources -type f -exec touch {} +` antes de reconstruir. O aviso de *clock skew* não é evidência de falha de forja.
