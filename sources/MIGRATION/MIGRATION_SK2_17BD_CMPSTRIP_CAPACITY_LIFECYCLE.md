# SK2.17-BD — ciclo de vida e limite de capacidade do CMPStrip

## Resumo

Adiciona TOP_HISTORICAL_CMPSTRIP_LIFECYCLE=1, desligado por padrão, para aproximar o armazenamento/encerramento do CMPStrip DX9 nos caminhos actor/item do jogador já suportados pelo GL. O audit de âncoras mediu seis controladores direct-strip em seis PARs, alcançados estaticamente por 16 skills. Esta etapa não amplia os donos suportados: owners não-player e os controladores MPEffectCtrl continuam fora desse caminho.

## Evidência — DX9 original × porte GL × mudança

| Original DX9 | Porte GL antes desta etapa | Mudança |
|---|---|---|
| CMPStrip::GetTrack para quando o buffer alcança m_iMaxLen se m_bLoop==false; se true, cresce 100 vértices por vez e para após ultrapassar 2000 (arquivo/fontes/Engine/include/MPModelEff.h:820-835). | legacy_direct_strip::append descartava os pares mais antigos ao passar de maxLength/2, mantendo uma janela móvel (sources/Engine/include/top/LegacyDirectStrip.h:79-96). | No modo opt-in, os pares não são removidos para abrir espaço; no limite, o strip não-loopado é marcado parado e emit não o desenha. O helper também reproduz o crescimento por 100 vértices para um caller que forneça loop=true. |
| O DX9 envelhece cada par e calcula alpha, mas mantém e submete os vértices ao draw mesmo quando transparentes (MPModelEff.h:782-795; MPModelEff.cpp:1351-1368). Render retorna quando o strip para (MPModelEff.cpp:1313-1316). | O GL removia pares cuja idade alcançasse life e omitiria do lote a geometria antiga (LegacyDirectStrip.h:93-96,104-117). | Com a opção ligada, os pares expirados continuam contando para maxLength e seus triângulos seguem no lote com alpha zero. Ao atingir o limite e parar, a malha inteira deixa de ser emitida. |
| CMPStrip inicia com m_bLoop=false; CopyStrip copia parâmetros, mas não copia uma flag de loop (arquivo/fontes/Engine/src/MPModelEff.cpp:1221-1245,1452-1463). SetStripCharacter só anexa personagem (MPParticleCtrl.cpp:326-332). | Os eventos GL cobertos consultam somente matrizes actor/item do jogador (sources/Engine/src/top/EffectSystem.cpp:1676-1686). | O caminho actor GL passa loop=false explicitamente, de acordo com o objeto DX9 recém-construído/copiado. |
| EffectObj::SetEffect chama SetStripItem(item) sem argumento de loop (arquivo/fontes/Client/src/EffectObj.cpp:1048-1060); o default declarado é false (MPParticleCtrl.h:331). Alguns caminhos MPEffectCtrl passam true (MPEffectCtrl.cpp:313,419). | O GL de direct strips só reconhece cast do jogador; não transporta a flag de MPEffectCtrl. | O caminho item de jogador passa loop=false explicitamente. A variante MPEffectCtrl com loop true permanece desconhecida/não integrada ao porte. |

## FATOS, INFERÊNCIAS E DESCONHECIDOS

- **FATO:** a auditoria atual decodificou 1.031 PARs, seis direct strips em seis arquivos, zero falhas de decode, um arquivo sem dependência e seis registros presentes no catálogo de scene effects. Os comprimentos são 80 ou 100 vértices e os passos 0,03 ou 0,05 s. Os vínculos estáticos alcançam 16 IDs de skill.
- **FATO:** o GL converte as duas matrizes DX9 em endpoints OpenGL X/Y-altura/Z pela permutação já existente; esta mudança não altera a transformação, blend, UV, culling ou winding.
- **INFERÊNCIA:** preservar pares já transparentes faz o consumo de capacidade acompanhar a regra DX9 melhor que a janela móvel. A auditoria de dependências não prova que cada recurso permaneça ativo tempo suficiente para alcançar seu limite.
- **DESCONHECIDO:** execução de cada recurso, frequência real de amostragem dos anchors em todas as sessões, owners de NPC/monstro e os caminhos de loop true em MPEffectCtrl. Não houve comparação visual.

## Alterações e teste

- A nova variável é lida apenas pelo helper LegacyDirectStrip; ausente ou 0 conserva a janela móvel anterior.
- Fixture exercita strip não-loopado no limite, preservação de pares já expirados, parada da emissão e crescimento de loop em incrementos de 100.
- O teste focal passou 1/1; o build Debug completo incluiu top_gl e CTest passou 138/138. A fixture passou também com ASan/UBSan (detect_leaks=0, UBSAN_OPTIONS=halt_on_error=1).
- Repetir o audit de âncoras:

~~~~bash
build/top_direct_strip_anchor_audit \
  --client Client --out build/sk217bd-direct-strip-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --scene-effects scripts/table/sceneffectinfo.txt \
  --dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp \
  --dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp \
  --dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp \
  --dx9-client-effect-obj arquivo/fontes/Client/src/EffectObj.cpp \
  --gl-effect-system sources/Engine/src/top/EffectSystem.cpp \
  --gl-actor-skin sources/Engine/src/top/ActorSkin.cpp
~~~~

Repetir o build/test focal:

~~~~bash
cmake --build build --target top_ui05sk215_tests -j2
ctest --test-dir build -R ui05sk215_direct_controller_cmpstrip --output-on-failure
~~~~

## Como ligar e reverter

~~~~bash
TOP_HISTORICAL_CMPSTRIP_LIFECYCLE=1 \
TOP_HISTORICAL_CMPSTRIP_STRICT_STEP=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
./build/top_gl --profile
~~~~

TOP_HISTORICAL_CMPSTRIP_STRICT_STEP=1 mantém a regra DX9 de amostrar somente quando o timer é estritamente maior que o passo. Para desligar somente o novo ciclo, defina TOP_HISTORICAL_CMPSTRIP_LIFECYCLE=0. Reversão do patch: git apply -R SK217BD.patch.
