# Tales of Pirates — 05-W1: interações para testes offline

Base: migração 05-W. Esta etapa é intermediária, ANTES de 05-X.

## Implementações

- Clique no corpo de monstros com volumes 3D aproximados, sem depender do ponto de contato com o terreno.
- NPCs com clique direto no corpo; se estiver longe, tenta andar para perto e conversar. Tecla E continua funcionando.
- Itens de chão: selecionáveis por volume maior, marcador aumentado e recolha por clique (auto-loot por proximidade desligado).
- Bolsa: clique normal usa/equipa; Shift+clique com loja aberta vende **1 unidade** por metade do preço da tabela (valor offline); Shift+clique fora da loja descarta **1 unidade** no chão. Não é possível vender/descartar um equipamento vestido sem removê-lo primeiro.
- Skills aprendidas: Shift+clique na janela K consome 1 ponto de skill e aumenta um nível, até 10 (regra simplificada do porte offline). Clique simples continua atribuindo à barra.
- GM offline: Alt+G soma XP, Alt+H restaura HP/SP, Alt+J invoca um monstro próximo e Alt+U concede um item escolhido por ID.
- Câmera: mantém giro horizontal invertido; inclinação limitada a 0.36..0.60 e zoom 5..22 metros, sem liberar câmera completamente vertical. Settings antigos são limitados automaticamente.

## Configuração dos testes de GM

Exemplo:

```bash
./build/top_gl --gm-mob 701 --gm-item 4418 --gm-xp 100000
```

Pressione **Alt+J** para invocar o monstro ID 701 (até seis por mapa), **Alt+U** para adicionar uma unidade do item ID 4418, **Alt+G** para ganhar 100.000 XP e **Alt+H** para restaurar HP/SP. Os IDs precisam existir nas tabelas históricas do cliente. Sem argumentos, Alt+J invoca ID 201 e Alt+G fornece 100.000 XP; Alt+U requer `--gm-item`.

A invocação é temporária e é limpa na troca de mapas. Itens e XP de GM alteram o save offline normal; **faça backup de `Client/user/` antes dos testes**.

## Testar no jogo

1. Passe o cursor sobre o corpo de monstros e NPCs, não somente os pés, e clique.
2. Abra o inventário com I, deixe cair um item com Shift+clique fora de uma loja; clique no item do chão para recolhê-lo.
3. Abra uma loja usando um NPC comerciante, com a janela aberta use Shift+clique em um item da bolsa para vender uma unidade. O clique simples não vende mais itens não equipáveis automaticamente.
4. Abra K; após ganhar TP por níveis, Shift+clique em habilidade APRENDIDA para evoluir; clique normal coloca na barra.
5. Rode com `--gm-mob 201`, Alt+J, e confira o alvo. Alt+G aumenta a experiência; Alt+H cura. Opcionalmente passe `--gm-item N` para testar itens de níveis altos.
6. Gire com o botão direito e use roda para zoom: a inclinação está restrita, evitando visualizar o mundo de cima/laterais sem céu.

## Testes

- `ui05w1_screen_ray_selection`: interseção com volumes de atores, itens e oclusão.
- `ui05v_play_state_regression`: recompilado com Play::selfTest(), que agora testa venda intencional, descarte, loot sob demanda, XP/cura/item de GM, mais regressão existente.
- 30/30 suites CTest passaram no ambiente headless. Suites UI04d..04i recompiladas; 23 binários antigos de suites independentes foram reaproveitados da 05-W; a suíte UI05w foi compilada. Compilação de sintaxe C++17 do main/ActorSkin/Play/SkillsUI passou.
- Este ambiente não possui GL/gl.h: compilação completa de `top_gl`, GLX e validação visual ainda devem ser feitas no Linux do usuário.

## Limites importantes

- Os volumes de seleção são aproximados (esferas ao longo do corpo); não fazem ray-triangle picking das malhas históricas.
- A coleta de drops ainda é representada por marcadores geométricos simples; modelos/ícones de loot com renderização fiel ao original são etapa separada.
- Venda por metade do preço e custo de um TP por evolução de skill são regras temporárias do jogo offline, não fórmulas Lua históricas integralmente portadas.
- Invocação GM não executa scripts nem eventos do servidor original; servirá apenas para teste.
- Não houve alteração intencional dos dados de mapas, rede, slots nem UI de equipamentos.
