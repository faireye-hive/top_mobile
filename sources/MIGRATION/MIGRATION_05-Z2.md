# 05-Z2 — backup recuperável de personagem

O save principal continua sendo gravado por temporário adjacente, `fsync` e `rename`. Os novos saves incluem o trailer opcional `CRC1`, calculado sobre toda a linha anterior, para detectar corrupção no conteúdo e não apenas no cabeçalho. Antes de substituir um arquivo existente, o cliente copia a versão anterior para `arquivo.bak` por outro temporário, sincroniza a cópia e o diretório e só então confirma o novo save. Se o principal estiver ausente, truncado, com CRC inválido ou não passar a validação do cabeçalho legado, `load` tenta o backup e informa a recuperação na mensagem do jogo.

Se a sessão foi carregada do backup, o primeiro save não substitui esse backup pela cópia principal defeituosa. Após o novo principal ser confirmado, o estado de recuperação é limpo; o backup conhecido-bom permanece disponível até a próxima gravação normal.

O teste `ui05v_play_state_regression` cobre rotação de duas gravações, corrupção do CRC mantendo o cabeçalho, recuperação do ouro salvo, preservação do backup após recuperação e recusa de substituir o principal quando a rotação do backup falha. Também passaram os testes de gameplay offline, persistência de UID/instâncias e forja. `top_gl --check` validou o cliente offline atual.

Este formato permanece compatível com os saves antigos: arquivos sem `CRC1` continuam aceitos mediante cabeçalho e limites básicos. Ainda faltam injeção de falhas em cada etapa de disco e estratégia de migração formal para versões futuras.
