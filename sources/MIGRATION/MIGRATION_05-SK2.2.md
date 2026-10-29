# Migracao 05-SK2.2 — efeitos de grupo e escala do Frozen Arrow

## Motivacao

* **Dual Shot, skill ID 90**: `skillinfo.txt` usa visual de voo **2002**.
  No DirectX, `CMagicEff::Create()` roteia IDs `[2000,3000)` para
  `CreateGroupMagic()`, lendo `magicgroupinfo.txt` (2002 = grupo 2).
  O grupo 2 contem magias simples 10, 25 e 26, com movimento de
  **sequencia** (`Part_sequence`, atrasos 0.0/0.2/0.4 s).
* **Frozen Arrow, skill ID 93**: visual 1020 = magicsingle ID 20,
  recursos `icearrowf.eff`, `icearrowf(a).eff`, `icearrowf.par` e
  o efeito complementar `icearrowh`. A etapa SK2.1 ja carrega multiplos
  modelos mas aplicava escala mundial padrao 0.3 aos primitivos.

## Alteracoes

1. `MagicGroupCatalog.h`: leitura defensiva do `magicgroupinfo.txt`,
   expansao dos membros com limite de 32 e tipos de movimento 0 (leque)
   e 1 (sequencia). Nenhuma nova dependência externa.
2. `EffectSystem`: cria uma instancia por elemento do grupo, com atraso
   de 0.2s na sequencia; usa dispersao angular de 0.75 rad no leque,
   conforme `Part_fan()` do DirectX. Instancias ainda nao iniciadas
   nao sao desenhadas nem geram particulas.
3. Quando uma skill declara explicitamente um recurso nao renderizavel,
   nao desenha o antigo tornado/placeholder. Habilidades sem referencia
   historica preservam os placeholders legados.
4. Frozen Arrow: escala mundial experimental de 2.5x aplicada somente
   aos canais da skill 93, conjuracao 296 e voo 1020. Ajustavel com
   `TOP_FROZEN_ARROW_FX_SCALE=1.5` (aceita 0.5–6.0). **Ainda requer
   verificacao visual para determinar a escala fiel ao original.**
5. `TOP_TRACE_SKILL_FX=1` mostra os canais resolvidos, quantidade de
   modelos/particulas e a ordem de disparo. O trace antigo do Illusion
   Slash permanece disponivel.

## Regressao

Os testes headless cobrem: 3 disparos do Dual Shot, seus atrasos,
   ausencia de placeholders, tiros em leque, aumento dimensional no
   Frozen Arrow, modelos multiplos, combate e selecao explicita de alvo.

## Limitacoes

* O material real `icearrowf.eff`, `icearrowf(a).eff` e seus controladores
  nao foram disponibilizados para validacao visual neste ambiente.
* Ainda faltam outros tipos de particulas/dummies e animacao historica
  completa. SK2.2 nao declara as 411 habilidades convertidas.
* O motor OpenGL integral precisa de teste no Ubuntu do usuario.
