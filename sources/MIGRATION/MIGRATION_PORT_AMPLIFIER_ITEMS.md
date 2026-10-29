# Porte provisório dos amplificadores

O `iteminfo.txt` lista 18 itens com callback `AmplifierItem`, mas a cópia de scripts não contém uma definição Lua desse callback. Os itens 48, 56, 898, 993, 998, 1006, 1128, 3039, 3094–3097 e 3844–3845/3880–3882 têm multiplicador, duração e, quando descrito, faixa de nível em suas descrições históricas. Esses 17 itens agora ativam amplificador temporário de EXP ou drop, afetam respectivamente a recompensa de monstros e a chance de drop, e recusam fora da faixa de nível sem consumir o item.

O vínculo por descrição é `PORT_APPROX`: a rotina de servidor que implementa `AddState`, a regra de renovação/empilhamento e a chance extra de Lucky Strike para 3095/3880 não estão no pacote. O cliente mantém o maior multiplicador e a maior duração ativa da mesma categoria; esse comportamento precisa ser comparado ao servidor para D3. Os bônus temporários não são gravados no save. Nenhum efeito de skill foi alterado.

O item 578 (Fruit of Growth) promete duplicar o crescimento da fada, mas esse ciclo de crescimento ainda não existe no cliente. O item é preservado e a auditoria o mantém pendente, sem conceder um efeito de EXP fictício.

Self-test cobre todos os 18 IDs, uso funcional de EXP e drop, expiração, recusa por nível e preservação do Fruit of Growth.
