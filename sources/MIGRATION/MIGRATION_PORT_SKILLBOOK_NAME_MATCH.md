# Skillbooks sem entrada em `Server.Player.Skill`

Há 13 itens com callback `AddPlayerSkill` sem linha correspondente em `Server.Player.Skill`. Para esses casos, o porte associa o livro à skill apenas quando o nome completo do item em `iteminfo.txt` coincide, sem ambiguidades, com um único nome em `skillinfo.txt`.

O vínculo por nome é `PORT_APPROX`, pois o pacote Lua histórico não confirma diretamente o par item→skill. O restante do fluxo usa as regras já carregadas de `skillinfo.txt`: classe, nível, pré-requisito e custo de TP. Falhas preservam livro e pontos; sucesso consome o livro, aprende a skill e salva. Nenhum efeito de skill foi alterado.

O self-test cobre o match de todos os 13 IDs pendentes e um uso completo de Eagle's Eye (3189 → 89), além da recusa para uma classe sem acesso.
