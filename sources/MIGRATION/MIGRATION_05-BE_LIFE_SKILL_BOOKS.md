# PORT-05-BE — livros de habilidades profissionais

## Escopo

Porta o callback histórico `AddLifeSkill` para os 46 livros definidos em
`Client/scripts/offline/script/calculate/variable.lua`. Habilidades
profissionais têm armazenamento separado das skills de combate e não são
colocadas na hotbar nem afetam efeitos de combate.

## Fluxo incluído

- Confere nível atual exato e pré-requisito profissional.
- Exige um ponto de habilidade profissional e preserva o livro em caso de falha.
- Consome o livro, sobe o nível profissional, concede o material inicial quando
  definido pela tabela e salva os dados em um campo opcional `LS1`.
- Saves anteriores sem `LS1` continuam carregando com zero pontos e sem níveis
  profissionais.

## Limite histórico conhecido

O cliente Lua consulta `ATTR_LIFETP` para autorizar o livro, mas o caminho
histórico que atribui esses pontos (`Lifelv_Up`) não possui chamada identificada
no material offline atual. Por isso, este lote não inventa concessão de pontos;
até portar o ciclo de progressão que chama essa rotina, o uso de livros exige
estado `LIFETP` que ainda não é produzido pelo fluxo comum. O UI de teste de
skills segue separado e não deve ser tratado como fonte histórica.

Há também um serviço de NPC separado em `NpcSdk.lua::LifeSkillUpgrade`:
`Type=1` aprende o nível inicial sem custo e concede a ferramenta inicial para
as profissões 338–341; `Type=2` exige `Level * Resources.EXP` pontos de
`ATTR_CLIFEEXP` (o valor de `Resources.EXP` é 1.500), subtrai esse custo e sobe
um nível até 10. Esse serviço não está ligado ao OpenGL. O material fornecido
referencia `LifeSkillBegin` nas opções de NPC, mas não contém uma definição Lua
para esse callback; portanto, ainda falta confirmar a ligação histórica que
escolhe a profissão e o tipo da operação. Não substituir por livros nem criar
um NPC de produção fictício.

Os materiais 1067–1070 recebem os atributos documentados (`VAL_STR=1`,
`MAXENERGY=10000`, `ENERGY=1`). `VAL_STR` é armazenado em `Stack.extra.str` como
aproximação do modelo de instâncias atual e deve ser validado junto ao porte do
sistema de produção.
