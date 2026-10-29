# Tales of Pirates offline — migração 05-AC

## O que mudou

A fase 05-AC acrescenta uma **bancada visual de forja** ao porte Linux/OpenGL. Ela usa as regras históricas de gemas/forja/perfuração que já foram implantadas na 05-Z e 05-AA, sem criar novos resultados, bônus ou probabilidades.

- **Alt+B** abre/fecha a bancada e abre a bolsa ao lado automaticamente. Usar Alt+B **não embarca no barco** (B sem Alt continua responsável pelo barco).
- Três abas: **Forjar** (equipamento, gema tipo 49, Refining Gem tipo 50), **Combinar** (Gem Composition Scroll tipo 47, gema A e gema B idênticas) e **Encaixe** (equipamento, estabilizador 890 e catalisador 891; os dois materiais aceitam ordem inversa).
- Selecione os três itens **clicando** seus ícones na bolsa na ordem exibida, ou **arrastando** cada ícone até o espaço desejado. Clique no espaço ocupado para limpá-lo; alterar de aba limpa as seleções.
- O painel indica ouro disponível, custo da operação, chance e **motivo de invalidez**. O botão de revisão só fica ativo quando as verificações de `Play::previewWorkbench` são satisfeitas.
- O item permanece na bolsa enquanto está *selecionado* na bancada. O clique/arraste não o equipa, descarta nem vende. O **modal Confirmar/Cancelar** segue sendo a única maneira de executar uma operação.
- Todas as seleções são **por UID e ID**. Se a bolsa mudar de ordem, o ponteiro é resolvido para o índice corrente da mesma instância. Se o item desaparecer, deixa de ser válido. A confirmação confere os **UIDs dos três itens** e revalida a operação antes de consumir materiais.
- A forja e a combinação avisam que **uma falha consome ouro e pedras**. Um resultado de falha na rolagem **não é** bug da interface.
- O código existente de `forgeBag`, `combineGems`, `socketEquipment` permanece responsável por debitar ouro e materiais e gravar o save; a bancada não altera o estado persistente diretamente.
- Saves da 05-AB e anteriores permanecem compatíveis. Não foi introduzido novo formato de save. Nenhuma alteração foi feita no pipeline gráfico/terreno/animação.

## Limites conhecidos

- Esta é uma **interface funcional de preparação e confirmação**, ainda **não a recriação pixel a pixel** do formulário histórico do Blacksmith e de suas texturas `MUI`.
- A bancada pode ser aberta em qualquer lugar pelo atalho offline Alt+B para facilitar testes; o vínculo com NPC Blacksmith histórico é um passo posterior.
- O fluxo de perfuração usa as mesmas regras do `forge.lua` da 05-AA. As taxas da forja/combinação são mostradas antes de confirmar e a operação continua sujeita a falha aleatória.
- Uma atualização externa do inventário enquanto o modal está aberto pode invalidar os materiais; neste caso a operação é cancelada, não executada com outro item.
- A disposição das janelas é otimizada para resoluções comuns como 1280×720 e 1920×1080; em resoluções pequenas o usuário poderá precisar fechar outros painéis.

## Execução

Faça backup dos saves antes de operações de forja:

```bash
cp -a Client/user "Client/user.backup-05AC-$(date +%Y%m%d-%H%M%S)"
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl --profile
```

Dentro do jogo: pressione **Alt+B**, escolha uma aba, clique em três itens da bolsa na ordem indicada ou arraste-os aos espaços. Leia custo e chance, clique em **Revisar e confirmar** e então no botão **Confirmar**. Cancelar não consome recursos.

Testes:

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Novo teste `ui05ac_forge_workbench_uid_and_preview` cobre hitboxes, três abas, seleção de UIDs, gemas repetidas, mudanças de índice e desenho da bancada. O `ui05v_play_state_regression` também foi ampliado para verificar o preview de forja e abertura de encaixe.

### Verificação neste ambiente

**43/43 testes CTest passaram**, com todos os executáveis de teste compilados nesta fase (dois builds incrementais concluídos). A sintaxe C++17 com `-O2` e `-Werror` dos arquivos alterados de HUD e `main.cpp` passou. `Play.cpp` passou na compilação real do teste integrado. O executável OpenGL `top_gl` não pode ser finalizado aqui porque faltam `GL/gl.h` e `GL/glx.h` neste ambiente; os **headers/driver OpenGL da máquina do usuário** são necessários para concluir essa validação.
