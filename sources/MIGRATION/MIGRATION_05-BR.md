# Migração 05-BR — destinos do teleporter Jovial

O diálogo do Jovial em Argent agora consulta a mesma tabela histórica de destinos usada pelos tickets 4603 (Shaitan), 4604 (Icicle) e 3828 (Thundoria). Isso remove coordenadas duplicadas, evita que a viagem dessas três cidades dependa do spawn padrão e impede que o menu do Jovial seja completado com portais/mapas arbitrários.

O conjunto corresponde às três opções permanentes de `TeleportCity()` em `Client/scripts/offline/maps/script/MisScript/NpcScript02.lua`. Dream Island e as páginas de Haven/record-spawn são condicionais ou fluxos separados e ainda não foram portados para essa grade simples. Outros teleporters continuam consumindo os destinos de seus portais locais; o caminho sem coordenadas exatas ainda pode usar o spawn do mapa e exige auditoria individual.

Validação: compilação de `top_gl`, `ctest` 70/70, `top_gl --check` e `git diff --check` aprovados.

## Escopo restante maior

Esta correção não completa criação/seleção original de personagens, stats e classes avançadas, callbacks/animações de todas as skills, bancos, modelo/altura/física naval, nem a investigação de pausas no movimento. Em particular, o modal chamado “Escolha seu personagem” ainda seleciona uma das quatro raças e reabre o mesmo perfil offline; não é a tela histórica de múltiplos personagens, criação ou escolha de nascimento. Esses pontos continuam abertos no porte.
