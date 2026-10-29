# PORT-04 — catálogo histórico de produção

Este lote adiciona o leitor dos registros literais `Manufacturing`, `Crafting`
e `Cooking` de `Client/scripts/offline/script/calculate/variable.lua` e as
faixas de seleção por energia usadas pelo callback histórico `Blueprint`.

O arquivo contém 280 receitas de Manufacturing, 904 de Crafting e 40 de
Cooking. O callback histórico só sorteia índices Crafting até 902 na faixa de
energia 8; as linhas 903 e 904 permanecem no catálogo, mas não são selecionadas
por essa regra. O porte conserva essa fronteira em vez de ampliar a tabela do
callback por conta própria.

O leitor conserva ID da receita, produto, nível, três materiais, peso, estado
ativo e sinalizador GoodItem. O sorteio ponderado respeita as faixas históricas
do callback, sem executar Lua nem modificar a forja, o encaixe ou os efeitos de
skills.

## PORT_APPROX

- **Sequência aleatória:** o histórico usa `math.random`; o porte usa o gerador
  determinístico offline já existente em `Play::roll`. Tipo, chances, energia,
  faixas, pesos e limites são históricos, mas uma semente DX9 não produz a mesma
  sequência de receitas. Pendência: comparar distribuições e adotar uma rotina
  compatível com o gerador de origem sem afetar os demais usos de `roll`.
- **Parâmetros de instância:** os atributos escritos pelo callback ficam nos
  metadados Y3 por item. Saves Y1/Y2 continuam aceitos; Y3 só acrescenta os
  campos `VAL_STA` em décimos e `Part2/4/6`.

## Cobertura atual

Os itens 1000–1003 já geram um blueprint individual com os atributos e os três
parâmetros randômicos do callback histórico; o item fonte só é consumido se o
blueprint couber na bolsa e seus dados existirem. O teste cobre save/reload.
O callback histórico de processamento também foi localizado em
`Client/scripts/offline/script/calculate/forge.lua` (`can_manufacture_item_main`,
`begin_manufacture_item` e `end_manufacture_item`). Ele define a entrada da
máquina: fada no slot 1 (tipo 59 e `URE` positivo), blueprint 2300–2302 no slot
2 e ferramenta correspondente obrigatória no slot 3. A ferramenta precisa ter
durabilidade positiva e corresponder ao blueprint (1068/2300, 1069/2301,
1067/2302); o nível dela não pode exceder `1 + 3 ×` o nível da habilidade
profissional gravada na fada. O nível profissional do personagem também deve
alcançar o nível do blueprint. O blueprint fornece três IDs de materiais
em `VAL_STR/CON/DEX`, quantidades em `Part2/4/6`, produto em `VAL_AGI`, nível em
`URE`, energia em `MAXENERGY-100` e usos em `VAL_STA`. A validação exige espaço
para o resultado; o fechamento consome os materiais e moedas de fada (item 855,
quantidade `MAXURE`), reduz os usos do blueprint e, quando aplicável, gasta
durabilidade e concede experiência à ferramenta.

O callback calcula a qualidade a partir do nível profissional, nível do
blueprint, nível da ferramenta e energia. O fluxo de interface também inclui
uma etapa de palpite de dados antes de chamar `end_manufacture_item`.

O OpenGL agora expõe uma bancada **Substance Generator** separada em `Alt+V`;
`Alt+B` e as três operações históricas da forja mantêm seu layout. A prévia confere
fada, blueprint, três materiais, moedas de fada e espaço na bolsa. Confirmar
executa sobre uma cópia do inventário, consome ingredientes e moedas, reduz os
usos do blueprint, atualiza a durabilidade e experiência da ferramenta obrigatória, entrega o
produto quando há sucesso e salva o estado. Erros de validação preservam os
itens. A cobertura de gameplay exercita sucesso, consumo e save/reload; o teste
da bancada separada verifica seleção, layout e hitboxes. Os atributos especiais
dos produtos 1067–1070 e 2236 são aplicados conforme o callback Lua.

## PORT_APPROX

- **Minijogo de palpite:** o callback original recebe o resultado da animação e
  do palpite visual. Essa interface não está disponível nos recursos offline;
  a aba usa um resultado randômico no intervalo esperado pelas regras Lua.
- **Minijogo:** a sequência randômica local reproduz os intervalos e as regras
  de sucesso/falha do Lua, mas não a roleta visual do DX9.

### Atributos do produto confirmados

`end_manufacture_item` chama `MakeItem(role, new_num, num_x, 4)`. No servidor,
o quarto argumento é `byAddType`, encaminhado como `chType` a
`CFightAble::ItemInstance`; não é uma qualidade. `Creat_Item` trata os eventos
0, 1, 2, 3 e os eventos especiais, mas não tem ramo para o evento 4. Assim,
nesse fluxo a criação conserva os atributos-base de `ItemInfo`, sem sorteio de
afixos. O OpenGL deixou de gravar a qualidade falsa 4 e inicializa durabilidade
e energia dos produtos não empilháveis com os valores-base da definição. As
alterações especiais de 1067–1070 e 2236 continuam sendo aplicadas depois,
como no callback histórico.

O limite restante de PORT_APPROX é somente o resultado do minijogo de palpite
e a sequência diferente do RNG; os atributos-base do evento 4 não dependem de
aproximação.

## Próximo trecho vertical

Confrontar o fluxo de entrada, número de usos, consumo em falha e experiência
da ferramenta com uma sessão DX9 reproduzível. Depois, substituir a
aproximação da roleta somente quando houver evidência suficiente para a
sequência do minijogo.
