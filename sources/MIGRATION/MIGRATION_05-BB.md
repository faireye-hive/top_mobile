# 05-BB — Catálogo local de personagens e pontos de nascimento

## O que foi portado nesta etapa

- Adicionado o modelo de perfil local separando `profileId` da raça/modelo do `characterinfo`.
- Catálogo TSV versionado com limite histórico de quatro personagens, nomes únicos, selecionar/excluir e salvamento atômico dos arquivos de catálogo e perfil ativo.
- O cliente Linux carrega o perfil ativo do catálogo e passa seu ID/nome ao `Play::bind`, isolando saves por perfil em vez da raça.
- A janela agora apresenta até quatro slots com nome/raça/cidade e ações Criar, Excluir, Entrar e Sair. Criar percorre raça e as três cidades iniciais; Entrar carrega o mapa/ponto salvo no perfil.
- O campo de nome aceita até 24 caracteres ASCII (letras, números, espaço, `_` e `-`) pelo teclado; nomes sem caracteres alfanuméricos são rejeitados.
- Na primeira abertura sem catálogo, um save legado por raça — ou o save único `offline-save.txt` do Lance — é registrado usando o ID antigo, sem sobrescrever o arquivo de origem.
- Parser restrito das linhas `AddBirthPoint` e escolha do primeiro ponto registrado para uma cidade, sem executar Lua nem servidor.
- As cidades e coordenadas de referência são lidas da cópia Linux de `Client/scripts/offline/maps/script/birth/birth_conf.lua`; o ponto ainda precisa passar pela validação do terreno no fluxo de criação.
- Teste cobre quatro raças, limite de quatro, duas cidades/mapas, persistência, seleção, exclusão e rejeição de catálogo corrompido.

## Ainda não concluído

O fluxo principal está conectado. Ao selecionar um perfil ou uma raça na criação, o cliente agora carrega o modelo 3D real correspondente pelo pipeline OpenGL; a prévia é temporária e não troca o perfil ativo nem grava save. Ao cancelar/sair, o roster de combate e o modo de barco anterior são recompostos. Ainda não reproduz o cenário 3D original de seleção nem os controles de rosto/cabelo. A recuperação da última posição explorada continua pendente. Os pontos históricos são selecionados e validados no terreno, mas a distribuição aleatória original entre pontos da mesma cidade ainda não está reproduzida.

## Fonte

`Client/scripts/offline/maps/script/birth/birth_conf.lua` é a cópia mantida no cliente novo da tabela histórica `arquivo/servidor/server/GameServer/resource/script/birth/birth_conf.lua`. O comportamento de escolha da cidade e dos quatro slots também foi conferido em `arquivo/fontes/Client/src/CreateChaScene.cpp` e `CreateChaScene.h` (`MAX_CHA_NUM=4`, `MAX_CITY_NUM=3`).

## Verificação desta atualização

- Build do `top_gl`: aprovado.
- `top_gl --check --character 1..4`: aprovado para os quatro modelos raciais; UI exercitou slots, ações, raça, cidades, edição de nome e confirmação de exclusão.
- CTest: 71/71 aprovados, incluindo importação do ID legado, não reutilização de IDs de saves órfãos e leitura dos pontos reais de nascimento.
- Não foi feita comparação visual interativa com a tela 3D original; o palco/câmera, as quatro posições simultâneas e a edição de rosto/cabelo permanecem pendentes. A prévia individual do modelo OpenGL foi validada por `--check` nas quatro raças.
