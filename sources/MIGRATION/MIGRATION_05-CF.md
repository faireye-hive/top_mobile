# Migração 05-CF — namespaces de efeitos de skill

## Evidência

`skillinfo.txt` usa colunas diferentes para os canais de efeito: conjuração e
impacto apontam para IDs de `sceneffectinfo.txt`, enquanto o efeito de voo usa
IDs de `magicsingleinfo.txt` codificados como `1000 + ID`. Illusion Slash é um
exemplo verificável: conjuração 186, voo 1018 (efeito 18) e impacto 195. Os IDs
186 e 195 possuem `.par` em `Client/effect/`; o voo 18 usa sua definição em
`magicsingleinfo.txt`.

O renderer anterior tratava todo ID como `magicsingleinfo`, deixando os canais
de cena sem resolução e escolhendo o visual genérico mesmo quando havia um
recurso original. Isso não era uma ausência de asset: era uma mistura de
namespaces.

## Alterações

- Adicionado `SceneEffectCatalog`, isolando o parser de
  `sceneffectinfo.txt` e validando nomes de arquivos antes de abrir recursos.
- `EffectSystem` resolve cast/impacto em `sceneffectinfo`, carrega o `.par` da
  pasta `effect/` e normaliza referências de voo `1000 + ID` antes de consultar
  `magicsingleinfo`.
- Efeitos procedurais de cast/impacto deixam de ser sobrepostos quando o
  recurso histórico correspondente está renderizável. Voos mantêm o projétil
  procedural quando o `.eff` só fornece textura e ainda não oferece geometria
  animada, preservando o teste de atraso do `.par`.
- Para skills ofensivas, o evento Cast agora aguarda o início real da pose de
  ataque. Se o alvo estiver fora do alcance e o personagem precisar se aproximar,
  a conjuração não aparece nem expira prematuramente durante a caminhada. O
  evento Flight nasce no mesmo início de pose; o impacto continua no quadro
  calculado da animação.
- O relatório `top_gl --check` agora mostra quantos efeitos de cena foram lidos.

## Verificação

- Testes de efeitos `ui05f` a `ui05l`: 7/7 passaram. `ui05g` cobre fixtures de
  `sceneffectinfo`, `.par` de cast e ID de voo 1018 resolvido para
  `magicsingleinfo` 18; `ui05k`/`ui05l` confirmam que geometria histórica toma
  o lugar do fallback procedural em vez de ser duplicada.
- `ui05v_play_state_regression`: passou com a regressão de Illusion Slash que
  confirma ausência de Cast antes de atacar e Cast/Flight no início do ataque.
- O teste também verifica que o efeito respeita o atraso do controlador e não
  desenha o anel procedural antes do início do `.par`.
- `top_gl --check`: terminou com código 0 e informou 1.093 efeitos de cena
  lidos da tabela local.
- O próximo teste é visual: comparar Illusion Slash e as skills do mesmo grupo
  no OpenGL com a referência DX9, incluindo enquadramento e orientação de cada
  emissor.
