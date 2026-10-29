# 05-BJ — ocupação das mãos por equipamentos

`iteminfo.txt` distingue `item can equip location` (coluna 28) de `item switch location` (coluna 29). O cliente DirectX usa a segunda para liberar todos os encaixes exigidos ao trocar equipamento; IDs 0008/0009 e o arco 0034, por exemplo, declaram as duas mãos, enquanto armas de uma mão declaram `-1`. O porte lia apenas a coluna 28, permitindo que uma arma de duas mãos coexistisse com um escudo.

O cliente OpenGL agora carrega a coluna 29, remove os itens incompatíveis das posições afetadas ao equipar e também libera uma arma de duas mãos quando outro item ocupa a mão secundária. A remoção só altera o estado equipado: os itens continuam na bolsa, como no fluxo de troca do cliente antigo.

`Play::selfTest` cobre equipar escudo sobre arma de duas mãos e arma de duas mãos sobre escudo. Validação desta etapa: build `top_gl`, `top_gl --check`, CTest e `git diff --check`.
