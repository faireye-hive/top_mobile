# Migração 05-BP — regressão de save atômico

## Estado atual auditado

`Play::save` já gravava em um arquivo temporário criado no mesmo diretório, verificava `fflush`/`ferror`, sincronizava os dados, fazia `rename` atômico e sincronizava o diretório. A análise anterior ainda descrevia a implementação antiga de escrita direta; foi corrigida para refletir o código presente.

Esta etapa acrescenta ao `Play::selfTest` uma falha de criação do temporário induzida por limite de nome do filesystem. O alvo anterior continua sendo um arquivo válido. O teste exige que `save()` falhe e que o conteúdo anterior permaneça intacto, sem depender de permissões (que não falhariam quando os testes rodam como root). O limite do componente é consultado por `pathconf`.

## Verificação

- Build de `top_gl` e do teste de integração `ui05v_play_state_regression`: aprovado.
- `ctest --test-dir build --output-on-failure -j2`: 70/70 aprovados.
- A falha testada ocorre antes do commit. Ainda não há injeção para falhas após `fflush`, `fsync` do arquivo ou sincronização do diretório.

## Limites restantes

Este teste não implementa cópia recuperável, envelope/versionamento global, recuperação automática nem rollback das alterações em memória após falha de save. Essas são partes ainda necessárias para fechar integralmente a persistência do cliente.
