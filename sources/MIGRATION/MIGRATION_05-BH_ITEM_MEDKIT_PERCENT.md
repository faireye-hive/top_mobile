# 05-BH — consumíveis ItemMedKit e estados de comidas

`ItemMedKit` agora carrega `Amount`, `Percentage` e `Cooldown` das linhas
`Server.Restore.Life` em `Client/scripts/offline/calculate/variable.lua`.
O cálculo aplica `Amount + máximo * Percentage`, limita HP/SP aos máximos,
recusa uso durante navegação ou com HP/SP zerado e mantém cooldown local por
item. O consumo e o salvamento continuam no fluxo normal de `clickBag`.

Isso corrige especialmente Gyoza (3909) e Fresh Meat, Seafood e Mix Dumpling
(6590–6592), que usam restauração percentual; antes a ausência de valor
absoluto acionava um fallback genérico de 15 HP. Esse fallback foi removido.

Também foram portados os estados temporários 165–167 das comidas: aumento de
HP máximo/defesa, ataque e SP/STA. Os níveis e durações vêm de `Restore.Life`;
comida igual ou inferior à ativa é recusada e uma de nível superior a substitui.
Ao expirar, os atributos são recalculados. Esses estados são temporários de
runtime e não são serializados no save, assim como os buffs temporários de
skills existentes. O recálculo dos atributos respeita a separação dos estados
de comida; nenhum callback ou fórmula de efeito de skill foi modificado.

Para evitar cura gratuita em uma tentativa recusada, o cliente offline valida
antes de aplicar cura se já existe estado de comida igual/superior. O script
histórico aplica `Amount`/`Percentage` antes dessa validação; essa ordem de erro
foi tornada transacional no offline. O consumo recusado é preservado.

Validação direcionada em `PlaySelfTest` cobre cálculo percentual, cooldown,
recarga, consumo e save/reload; estados 165–167, bônus, recusa de nível menor e
expiração também têm cenários. O binário `top_ui05v_play_tests` compila e executa
esses casos; depois termina em uma falha preexistente de keyframe de animação
(`05-V`), fora deste lote. `top_gl` também compila com o porte integrado.
