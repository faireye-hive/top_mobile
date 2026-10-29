# SK2.15-B — Ciclo dos modelos `.eff` nos STRIP convencionais

## Base e escopo

Incremental **após SK2.15-A**. Mantém a trilha direta `CMPStrip` de
`swordlight02.par`, Taunt, Berserk, Frozen Arrow e o impacto provisório do
Illusion Slash em `2.5×`. Não muda geometria, filtro, blend ou tamanho.

A auditoria SK2.14 encontrou 1.041 sistemas `.par` com `type=6`.
Na amostra `par_systems.csv` enviada, **875 referenciam `.eff`** e os
outros **166 usam `RectPlane`**. A correção aqui atinge especificamente
os filhos `.eff` nos sistemas do tipo 6, não os 1.041 igualmente.

## Divergência constatada

Código DX9: `MPParticleSys.cpp`, `_CreateStrip`, `_FrameMoveStrip`,
`CMPPartSys::Play`. Um sistema STRIP convencional utiliza **uma partícula**,
tamanho, cor e ângulo do **primeiro quadro do controlador** e um
`CMPModelEff` que atualiza seus próprios quadros. Quando iniciado com
`Play(0)`, o filho também entra no modo de repetição e a sequência de
subefeitos recomeça **em conjunto**, depois de encerrar o ciclo mais longo
(`CMPModelEff::FrameMove`).

Antes, o porte utilizava `loopModel = effect.kind == Flight || !singleModel`.
Isso fazia qualquer STRIP em conjuração/impacto (`singleModel=true`)
parar de mostrar seu filho `.eff` após o primeiro ciclo, mesmo quando o
sistema pai continuava vivo. A SK2.15-B aplica o período do **grupo inteiro**
ao tempo do filho `.eff`; subcamadas curtas continuam interrompidas até a
reinicialização comum, em vez de cada uma repetir isoladamente.

A correção não altera o modo anterior de `MODEL` (tipo 5) em `Cast` e
`SkillHit`, nem o modo `Flight` já repetido antes.

## Controle e diagnóstico

Por padrão, o STRIP convencional usa o novo comportamento. Para testar o
comportamento antigo sem reverter o patch:

```bash
TOP_STRIP_NESTED_LOOP=0 TOP_HISTORICAL_BLEND=1 ./build/top_gl --profile
```

Para verificar o modo novo:

```bash
TOP_TRACE_STRIP_SYSTEM=1 TOP_HISTORICAL_BLEND=1 \
  ./build/top_gl --profile 2>&1 | tee sk215b.log
grep '\[strip-system\]' sk215b.log
```

Registro esperado ao carregar `.par` com filhos `.eff`:

`[strip-system] type=6 model=<arquivo>.eff tracks=N cycle=... replay=1`

A opção é avaliada no início do processo. Reinicie o jogo ao alternar.

## Testes

* `ui05sk215_direct_controller_cmpstrip`: mantém SK2.15-A e testa
  geometricamente uma conjuração `type=6`, com um `.eff` que termina aos
  `0.2 s`, ainda desenhado em `0.27 s` e `0.47 s`.
* `ui05sk215b_conventional_strip_cycles`: comprova reinício do **grupo**
  com subefeitos de durações diferentes, tipo 5/voo preservados e opt-out.

Esses testes são CPU-only e não certificam fidelidade visual. Compare o
Taunt, Berserk, Frozen Arrow e os efeitos do Illusion Slash no Ubuntu.

## Limites e próximos passos

Essa etapa **não** reimplementa as geometrias ou a emissão de todos os
1.041 STRIP. A duração de `CMPPartCtrl::Play(iTime)` pode variar por
chamador histórico, mas o porte atual não conserva essa flag por evento;
o modo de repetição por padrão corresponde à execução `Play(0)`.
Há também suporte pendente de filtros POINT/LINEAR por sistema, dummies
completos de entidades diferentes do jogador e os modos restantes de
partículas.

O crescimento da meia-lua `04000009.eff` do Illusion Slash **não é
resolvido por esta mudança**: seu voo é um `.eff` em
`magicsingleinfo`, não um filho `.eff` de STRIP convencional.
