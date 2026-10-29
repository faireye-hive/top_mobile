# Criar missões offline próprias

As missões ficam em `sources/Tools/custom-missions.tsv` e são oferecidas pelo **Argent Mission Board**. O arquivo é texto separado por tabulações; uma linha iniciada com `#` é comentário. Não é necessário editar Lua nem tocar nos scripts históricos.

Antes de abrir o jogo, valide o arquivo com `python3 sources/Tools/validate_custom_missions.py`. O verificador confere colunas, IDs, objetivos, recompensas e referências aos registros de monstros e itens. Ele só lê os dados e não cria nem altera missões.

Cada linha ativa contém oito colunas nesta ordem:

`id  título  texto  nível mínimo  objetivos  EXP  ouro  recompensas`

Use uma tabulação entre colunas. O ID deve ser único e ficar entre `900000` e `1000000`, inclusive. O jogo e o validador aplicam esse mesmo limite para manter as missões próprias separadas dos IDs históricos.

Objetivos podem ser combinados com `;`:

- `kill:ID_DO_MONSTRO:QUANTIDADE`
- `item:ID_DO_ITEM:QUANTIDADE`

Exemplo: `kill:268:5;item:4474:2` exige derrotar cinco monstros 268 e entregar dois itens 4474 ao quadro. Cada quantidade de objetivo pode ser de 1 a 255.

Recompensas de item usam `ID_DO_ITEM:QUANTIDADE`, também separadas por `;`. Deixe a última coluna vazia para não conceder itens. EXP e ouro podem ser zero. A missão fica associada ao **Argent Mission Board**; aceite, acompanhamento, entrega, recompensa e persistência usam o fluxo de missões offline existente.

## Exemplo copiável

```text
900001<TAB>Teste de coleta<TAB>Entregue três poções ao quadro.<TAB>1<TAB>item:8:3<TAB>50<TAB>100<TAB>15:1
```

Substitua cada `<TAB>` por uma tabulação real. Salve o arquivo e entre novamente em Argent para carregar a missão. Mantenha uma linha enquanto houver personagem com essa missão ativa; o save referencia o ID. O exemplo permanece comentado no arquivo de dados para não adicionar uma missão de demonstração automaticamente.

## NPCs de teste em Argent

Os vendedores `Test Apparel & Armor`, `Test Weapons`, `Test Chests` e `Test General & Quest Items` são adicionais aos NPCs históricos. Eles usam os IDs e preços de `iteminfo.txt`; o vendedor geral inclui os materiais das promoções históricas para facilitar testes de inventário e objetivos. O **Class Test Trainer** permite alterar a profissão diretamente e gravar o personagem. Essa ação é explicitamente um atalho de QA e não representa a regra histórica de promoção.

No treinador, escolha uma profissão para aplicá-la imediatamente. O menu inclui todas as profissões 1–18. Abra **More professions... → Advanced professions... → All skills (free +)** para ver o catálogo completo de skills: percorra as páginas com a roda do mouse e use `+` para aprendê-las/aumentar seu nível até 10. Nesse modo não são exigidos livros, pontos TP ou nível. Ao aprender pela primeira vez uma skill profissional com ferramenta inicial definida nos dados históricos, o treinador também concede essa ferramenta com os atributos iniciais históricos; sem dados ou espaço na bolsa, o nível não muda. A opção pode ser desligada pelo mesmo NPC; o aprendizado normal volta a exigir seus requisitos.

O **Hairstylist - Cartel** abre os estilos de `hairs.txt` compatíveis com a raça atual. Escolher um estilo consome os materiais e o ouro definidos naquela linha, aplica o modelo e grava a aparência no save. A base de dados não informa a chance histórica de falha nem uma regra de prévia; por isso, no OpenGL a seleção é imediata e tem sucesso garantido depois de validar os requisitos. Essa parte está identificada como `PORT_APPROX` até a chance original ser encontrada.
