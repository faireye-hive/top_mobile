# Porte parcial — fadas: ovos, rações e livros de skills

O callback histórico `FairyRation` lê `Server.Fairy.Ration[itemId]`, multiplica
o valor por 50 e o entrega à stamina `URE` da fada equipada no slot 16,
limitando pelo `MAXURE`. O cliente offline agora lê esses valores da tabela Lua,
aplica a recuperação à mesma instância de fada mantida na bolsa/equipamento,
consome uma ração e grava stamina e inventário juntos no save.

As verificações históricas cobertas incluem navegação, fada ausente ou inválida,
stamina cheia e a restrição especial de Angela/Angela JR para itens abaixo do
ID 5644. Rações sem dados ou sem uma instância de fada equipada são preservadas.

O callback histórico `ItemUse_Egg` também foi ligado: cruza o ID do ovo com
`Server.Fairy.Egg`, exige os dois espaços livres definidos pelo script, consome
um ovo e cria a fada mapeada com qualidade 4. Os limites de stamina e energia
vêm do `iteminfo` da fada e a instância recebe UID estável para equipar, salvar
e alimentar.

**Aproximação identificada:** a inicialização de `MakeItem` executada pelo
servidor não está nos scripts offline disponíveis. Portanto, ao abrir o ovo,
stamina/energia atuais e atributos aleatórios da fada começam em zero; não
afirmamos paridade dessa inicialização. A auditoria marca os ovos como
`fairy_egg_PORT_APPROX` até que a rotina histórica de criação seja recuperada.

Auto Ration 2312 e Great Auto Ration 6842 também foram ligados à regra de
`Server.Fairy.AutoFeed`: a unidade no terceiro espaço da bolsa é consumida
quando a fada equipada chega a 50% de stamina ou menos, respeitando o limite
histórico e salvando o estado. O auditor marca esses itens como
`fairy_ration_auto_PORT_APPROX`, pois o cliente verifica a condição a cada
atualização em vez do ciclo histórico de nível/experiência da fada; o estado
offline ainda não simula esse ciclo completo. A aproximação pode consumir a
ração antes do próximo tick que o servidor executaria.

Frutas, posse e estados avançados da fada também permanecem fora deste lote.
Nenhum efeito de skill de combate ou da fada foi alterado.

Os 27 livros `AddFairySkill` mapeados em `Server.Fairy.Skill` agora permitem
aprender skills na fada equipada: nível inicial 1, progressão sequencial, no
máximo três espaços, sem consumir o livro em caso de pré-requisito ausente,
fada ausente ou espaços completos. Os três pares históricos de ID/nível ficam
no save da instância da fada. O formato Y3 foi estendido, mantendo a leitura
dos registros de instância anteriores. Os efeitos e cálculos das skills da
fada não foram implementados nem modificados neste lote.

`PlaySelfTest` cobre aplicação da ração, limite, ausência de fada, persistência
da instância após save/reload, o fluxo ovo → equipar fada → alimentar, a ração
automática no limite de 50%, a preservação acima desse limite, a sequência de
aprendizado de skill e os limites de slots. Também verifica que ovo sem espaço
e livro sem pré-requisito não são consumidos.
