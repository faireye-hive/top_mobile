# SK2.10 — Curvas de partículas, primitivas MODEL/STRIP e timelines aninhadas

Data: 24/10/2026. Aplicar depois de SK2.9.

## Correções fundamentadas no DX9

1. **Curvas `.par`: N intervalos, não N−1.** `CMPParticle::GetCurFrame` e os `_Create*` em `arquivo/fontes/Engine/src/MPParticleSys.cpp` definem `m_fFrameTime = m_fLife / _wFrameCount`; a última chave permanece constante no último intervalo. `LegacyParticleCurves.h` agora usa essa fase para tamanho, RGBA e rotação. Por exemplo, com três chaves, a segunda começa em 1/3 da vida, não em 1/2. O teste antigo que consagrava N−1 foi corrigido. Tamanho zero deixa de receber o mínimo artificial de 0,01.

2. **MODEL/STRIP não são uma nuvem genérica.** `_CreateModel` e `_CreateStrip` forçam uma instância. `MoveTo` subtrai metade da faixa; `_FrameMoveModel` soma essa metade de volta, mas `_FrameMoveStrip` mantém o canto. Retirados o ruído e a translação por velocidade/aceleração nesses dois tipos. MODEL reinicia sua curva após a vida; STRIP usa a primeira chave fixa, conforme o original.

3. **Primitiva do `.par`.** Os 202 sistemas MODEL/STRIP que declaram `RectPlane` passam a usar seu modelo e flag de billboard: 199 declaram billboard; três são planos no espaço. Preservam textura, cor, rotação das primitivas e blend já disponíveis. O helper de billboard recebe meia largura, mantendo a extensão da primitiva. Para billboard, a rotação de partículas não é aplicada, como no ramo DX9 que usa a matriz da câmera. Não se criaram malhas novas ou efeitos substitutos. A conversão espacial anterior de 0,3 é preservada; sua escala absoluta ainda requer validação.

4. **Ciclos `.eff` aninhados por controlador.** Antes, cada camada fazia `fmod` em sua própria duração, repetindo camadas curtas cedo demais. Agora, no voo, o conjunto reinicia pelo máximo das durações das trilhas; no Cast/Result não se repete automaticamente. `CMagicCtrl::Render` inicia controladores de voo com `Play(0)`, e `Stop` inicia Result com `Play(1)`. Modelos de uma chave com duração positiva também encerram na hora correta. A vida dos modelos diretos usa a soma das durações das chaves, evitando corte antecipado quando o metadado `length` está desatualizado. Chaves inteiramente de duração zero conservam o comportamento estático anterior por compatibilidade; essa exceção ainda merece auditoria.

As referências são `_FrameMoveModel`, `_FrameMoveStrip`, `MoveTo`, `Play`, `RenderSoft` e `CMPModelEff::FrameMove`, nos fontes arquivados. O impacto provisório do Illusion Slash permanece em **2,5×**. O Frozen Arrow permanece sem ampliação adicional por padrão.

## Inventário e execução reproduzíveis

`Tools/skill_fx_probe.cpp` executa os canais declarados das 411 habilidades em CPU, sem combate, alterações de save ou contexto GL. Amostra Cast/Flight/SkillHit em 0, 0,1, 0,35, 0,7, 1,1 e 2 segundos, com destino estático e água para incluir todos os canais de superfície. Conta vértices, texturas e valores não finitos. Gerar geometria não prova que todos os componentes foram renderizados ou que a composição corresponde ao DX9.

`Tools/skill_migration_status.py` relê tabelas/recursos atuais, executa os leitores compilados e produz `SKILLS_MIGRATION_STATUS.csv`, inventário SHA-256 e resumos. Não reutiliza conclusões visuais das auditorias antigas. A ferramenta antiga de blend fornece valores e contagens; seus rótulos `par_ignored_by_loader`/`implemented_alpha` são anteriores à SK2.6 e **não** descrevem o renderer atual.

A auditoria confirmou 1.227 `.eff`, 1.031 `.par`, 3.476 sistemas, 411 habilidades e 19 pares de fatores de blend. Há 31 `.eff` cuja estrutura não foi decodificada. A leitura dos sistemas `.par` termina antes dos campos adicionais do controlador em 1.021 arquivos. A auditoria adicional identificou esses campos: seis trilhas diretas `CMPStrip`, nenhum `CChaModel` e nenhum byte restante sem explicação. Essas trilhas **não são** o tipo de partícula STRIP=6.

O caso `swordlight02.par`, usado entre outros pelo Illusion Slash, tem zero sistemas e uma trilha direta com dummies 0/1, vida 0,5 e passo 0,03. O runtime atual rejeita o controlador por não conter sistemas. É um mecanismo não interpretado pelo renderer, não um arquivo ausente ou corrompido. Os seis registros estão em `DIRECT_CONTROLLER_STRIPS.csv` e são prioridade da próxima etapa junto à ponte de dummies.

O CSV distingue ausência de arquivos, falhas de interpretação dos sistemas e mecanismos implementados parcialmente. Arquivos `.lgo` são incluídos por referência; o relatório não certifica todos os materiais internos, texturas desses materiais nem bancos `.lab` por personagem. A comparação visual por habilidade permanece pendente.

## Testes e limites

`UI05SK210Tests.cpp` usa fixtures binárias para verificar N intervalos, retenção da última chave, RGBA/rotação, tamanho zero, cardinalidade MODEL/STRIP, posições exatas, plano versus billboard, largura correta e camadas curtas aninhadas. `UI05HTests` e `UI05SK27Tests` foram atualizados nos comportamentos comprovadamente incorretos.

A suíte final passou em 83/83 testes. O cliente completo compilou. `top_gl --check` passou; um teste GL em display virtual gerou dois quadros de cena com `TOP_HISTORICAL_BLEND=1`, com saves copiados para `/tmp`. Esses quadros verificam inicialização, cena e movimento; **não** constituem validação das habilidades contra o original. Logs completos no pacote.

Persistem aproximações em SNOW/FIRE/BLAST/RIPPLE/WIND/ARRAW/ROUND/BLAST2/BLAST3/SHRINK/SHADE/RANGE/RANGE2/DUMMY/LINE_SINGLE/LINE_ROUND. A maioria das referências `.eff` nesses tipos ainda não recebe geometria aninhada. Emissão, aleatoriedade/vida por partícula, limites de quantidade, matrizes parentais das malhas aninhadas e estados gráficos além do blend também estão pendentes. Esta etapa não declara a migração completa.
