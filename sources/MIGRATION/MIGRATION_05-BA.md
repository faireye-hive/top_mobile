# Tales of Pirates OFFLINE — atualização 05-BA

## Base e escopo

Continuação dos fontes locais mais recentes, usando `TalesOfPirates_05-AZ_Completo.zip` como referência da base 05-AZ. O ZIP recebido não foi extraído por cima do diretório de trabalho: a pasta local já contém alterações posteriores, inclusive o contrato inicial `ItemInstantiationRules` e o teste 05-BA. Nenhum save em `Client/user/` foi alterado.

## Atualização posterior — itens e baús

O estado descrito abaixo é o registro histórico da etapa 05-BA. No código atual, os itens Newbie Chest 436–443 já possuem fluxo transacional para as profissões suportadas por `ItemInstantiationRules::materialize`; o baú 443 não está mais globalmente bloqueado. Esses oito itens são classificados como `PORT_APPROX`, pois a geração de qualidade/atributos e os eventos históricos `Creat_Item`/`MakeItem` ainda não têm paridade comprovada. Quando uma profissão não possui regra de materialização, a caixa é preservada sem consumo. A auditoria atual também reconhece Birth Cake 3240/3338 como suporte funcional para +3 AP, com recusa no barco e no limite conservador.

## Teleporte de Argent

- O NPC correto é `Argent Teleporter - Jovial`, associado ao script `TeleportCity`; Senna continua sendo o Newbie Guide.
- A lista de cidades é desenhada como botões na janela do NPC e usa a mesma geometria da função de clique. Selecionar uma cidade carrega o mapa offline; quando não há coordenada explícita, é usado o ponto de nascimento/entrada segura do mapa.
- As teclas `1` a `8` e `F1` a `F8` acionam skills mesmo com o diálogo aberto. A cidade é escolhida pelo mouse, sem conflito com skills.
- O teleporte físico por portais/tickets e as proteções de terreno/barco permanecem no fluxo existente.

## Itens e limite de equivalência

`item/ItemInstantiationRules` registra defaults comprovados de `CFightAble::ItemInstance`: durabilidade/energia máximas, negociação, expiração e origem válida. Ainda não reproduz `Creat_Item`, portanto não gera atributos aleatórios/qualidade do servidor antigo. Os eventos 11 (`PLAYER_XSBOX`) e 95 (`QUEST_AWARD_SCBOX`) acionam regras Lua de qualidade/atributos; não são bônus fixos. Na etapa 05-BA, Newbie Chest 443 continuava bloqueado; a implementação posterior o habilitou para profissões com regra de materialização, mantendo a aproximação explicitamente classificada até a paridade do gerador histórico.

## Validação

- `cmake --build build -j2`: concluído; o executável OpenGL compilou.
- `ctest --test-dir build --output-on-failure`: **70/70** testes passaram.
- `./build/top_gl --check`: passou em Argent; catálogo de 52 mapas e assets/modelos do mapa verificado sem falhas.
- Não foi feita validação visual interativa de mouse no Ubuntu; Jovial → botão de destino e skills por número ainda precisam de confirmação no cliente gráfico.

## Lacunas principais

1. Reproduzir com segurança `Creat_Item` e `MakeItem` (qualidade, atributos, forge e modos por origem); depois reavaliar a caixa 443 e os callbacks restantes.
2. Completar condições, flags e ramificações de missões históricas.
3. Comparar skills, estados, custos, efeitos e animações com scripts/tabelas originais.
4. Validar destinos de teleporte em cada cidade, sobretudo mapas marítimos/barcos e outros teleporters.
5. Continuar auditoria de comportamento/recursos mapa a mapa. `--check` é diagnóstico de assets, não prova equivalência total.

O porte ainda não está completo; não declarar equivalência integral até essas lacunas serem auditadas e testadas no cliente gráfico.
