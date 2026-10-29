# Código do porte Linux/OpenGL

Leia [ESTADO_DO_PORTE.md](ESTADO_DO_PORTE.md) para o diagnóstico atual, prioridades, estimativa de progresso e organização recomendada. Leia o [guia](../offline/docs/guia.md) para compilar e verificar.

CMakeLists.txt define o código ativo. Os diretórios originais em arquivo/ são referência histórica e não são substituídos por este porte.

A versão web usa a mesma base C++ do Linux. Consulte [compilação e uso web](../offline/web/README.md).
O loop compartilhado fica em `Platform/common/GameMain.cpp`; `Platform/linux/main.cpp` é apenas a entrada Linux.

## Documentação histórica

MIGRATION_*.md e os CSVs com identificação de migração registram o que foi observado ou entregue naquela etapa. Expressões como “pendente”, “próximo passo” e números de testes nesses arquivos não descrevem necessariamente a versão atual. Não executar seus antigos prompts de continuação como roteiro vigente.

ITEM_TICKETS_AUDIT_05-AY.md é uma referência específica de mapeamento de tickets. Os relatórios gerais antigos e os resumos de itens superados foram removidos na consolidação de 23/10/2026.

Atualize ESTADO_DO_PORTE.md ao concluir uma frente. Registre evidências por comportamento e evite criar diagnósticos gerais concorrentes.
