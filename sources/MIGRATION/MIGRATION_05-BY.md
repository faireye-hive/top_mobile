# 05-BY — Identidade independente para saves de personagens

## Problema

O cliente offline gravava em `offline-save-{raceId}.txt`. Como o ID usado pelo `characterinfo` também identifica o modelo/raça, dois personagens da mesma raça compartilhariam o mesmo progresso. Isso impede a tela original de seleção com vários personagens e torna inseguro criar outro personagem da mesma raça.

## Alteração

- `Play::bind` agora aceita um `profileId` independente do ID de raça/modelo e um nome de perfil opcional.
- Todos os saves emitidos pelos módulos de gameplay usam a identidade de perfil ativa, inclusive inventário e missões.
- Chamadas legadas sem `profileId` preservam o caminho anterior por raça, evitando mudar os fluxos de diagnóstico existentes.
- A importação de `user/offline-save.txt` continua limitada ao Lance legado quando ainda não existe um save próprio.
- A regressão cria dois saves com a mesma raça e IDs de perfil distintos, recarrega ambos e verifica que ouro/progresso não se misturam.

## Limite desta etapa

Isto prepara o contrato de persistência, mas ainda não implementa o catálogo persistente de personagens nem os botões Criar/Excluir/Entrar/Sair da tela original. Também não migra automaticamente saves antigos por raça para um perfil nomeado. Essa interface deve usar IDs de perfil novos e não reutilizar os IDs 1–4 de modelo/raça.

## Verificação

- Build completo CMake: aprovado.
- `top_gl --check`: aprovado; 52 mapas históricos válidos.
- CTest: 70/70 aprovados, incluindo a regressão de saves da mesma raça.
