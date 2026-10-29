# SK2.12 — Berserk: câmera, composição de transformações e âncora no chão

## Referência e versão de partida

Foram inspecionadas as 11 imagens PNG de `berseker.zip`, fornecido pelo usuário. Os arquivos são referência visual, não instruções para executar ações. O manifesto da entrada, com hashes dos PNGs e do ZIP, está em `deliverables/SK2.12/reference/INPUT_MANIFEST.json`. Não há intervalo entre quadros verificado; a sequência permite comparar formas e ordem aparente, mas não medir FPS, duração exata ou comparar o mesmo quadro de duas execuções.

Os 19 fontes/documentos da entrega SK2.11 conferiram com `RESULT_SHA256.json` antes de editar. Não foram removidas as correções de blend, espadas, geometria, UVs ou resultado de buffs daquela etapa. Os assets e saves históricos não foram editados.

Nas imagens, o brilho envolve o corpo sem ficar achatado pela inclinação da câmera. Nos quadros finais, os feixes têm inclinações distintas e partem da base do efeito. A execução OpenGL anterior achatava a aura, desenhava os feixes com o mesmo eixo vertical e elevava o efeito de chão à altura aproximada de efeitos no corpo.

## 1. Billboard orientado pela câmera completa

`CMPResManger` calcula `_MatBBoard` pela inversa da matriz de visão e remove sua translação. `CMPModelEff::RenderVS/RenderSoft` e os controladores de partículas usam essa rotação para billboards. O OpenGL recebia apenas a direita no plano X/Z e usava `(0,1,0)` como cima. Em uma câmera elevada, o quadrilátero não ficava voltado para a câmera e um brilho circular virava uma elipse.

`EffectViewBasis` extrai direita e cima da matriz de visão ortonormal. A cena passa a matriz usada no próprio quadro. Tanto billboards diretos quanto os aninhados usam os dois vetores completos; proporções e UVs permanecem os declarados. A sobrecarga antiga `build(map,rightX,rightZ)` permanece para consumidores existentes e testes sem câmera.

Não houve aumento da escala global, edição de texturas ou criação de um círculo substituto. A mudança visível de altura na tela decorre da orientação do plano.

Referências: `arquivo/fontes/Engine/src/MPResManger.cpp`, inicialização e atualização de `_MatBBoard`; `MPModelEff.cpp::RenderVS/RenderSoft`; `MPParticleSys.cpp::RenderSoft`.

## 2. Transformações do controlador pai e ordem de rotação

O DX9 compõe a transformação da chave do `.eff` com escala/rotação/translação do `CMPModelEff`, que recebe a pose do `CMPPartSys`. O OpenGL só multiplicava o tamanho da chave pelo tamanho da partícula: ignorava as três rotações do pai e não transformava corretamente a posição animada do filho.

As malhas primitivas e externas aninhadas agora aplicam a transformação do pai ao conjunto de posição e geometria. Quando o controlador usa billboard, a base da câmera também é usada na geometria nativa. A posição dos billboards vinculados respeita a substituição de translação feita por `BindingBone` no ramo original correspondente; isso não implementa a ponte de ossos do personagem.

Em `firegush.par`, as chaves iniciais dos controladores dos feixes incluem componentes de ±0,2 rad e combinações de ±0,14 rad. Esses valores agora inclinam as malhas. Não foram inventados ângulos por ID de skill.

Também foi corrigida a ordem de `D3DXMatrixRotationYawPitchRoll`: Z (roll), X (pitch), Y (yaw). A implementação anterior aplicava X, Y, Z, que só coincide em alguns casos com um único eixo. Referências: `MPModelEff.h::CEffectCortrol::GetTransformMatrix`, `CMPModelEff::GetTransMatrix`, `_FrameMoveStrip`, `CMPPartSys::RenderSoft` e a [documentação primária de D3DXMatrixRotationYawPitchRoll](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixrotationyawpitchroll).

O ramo software original usa uma composição distinta que omite `m_SMatTempRota`; a referência do caminho VS e os ângulos nos assets fundamentam a implementação. As imagens fornecidas são compatíveis com feixes inclinados, mas não identificam a configuração gráfica daquele executável.

## 3. Canais de chão separados da altura do corpo

`EffDelay.cpp::CHitRepresent::Exec` envia os canais de chão/água a `CGameScene::CreateEffect`; `Scene.cpp::CreateEffect` usa `GetGridHeight`. O canal de alvo usa `SelfEffect` com seu dummy. O OpenGL havia perdido essa distinção ao reduzir os canais a IDs e somava 0,9 à altura de todos.

A solicitação e a instância preservam a natureza de superfície. O canal de chão/água usa a altura do terreno, enquanto o canal do corpo mantém sua política anterior até a integração completa dos dummies. IDs iguais nos canais de alvo e superfície deixam de ser deduplicados entre si. No Berserk, o efeito 236 é um canal de chão e deixa de flutuar no centro do corpo.

Isso não resolve ainda a identidade de cada dummy/item nem a distinção completa entre superfícies elevadas, água e piso de pontes.

## 4. Fim absoluto dos controladores MODEL/STRIP

`CMPPartSys::IsDelay` compara o relógio desde o início com `_fDelayTime` e `_fPlayTime`. O segundo valor positivo é o instante de parada do controlador, não uma duração que começa depois do atraso. `Stop` desativa imediatamente MODEL/STRIP.

O OpenGL agora interrompe esses sistemas em `age >= playTime`, em vez de permitir `delay + playTime + life`. Exemplo: atraso 0,3 e fim 2,3 indicam dois segundos de atividade. O teste usa atraso 0,2, fim 0,6 e verifica que o modelo já parou em 0,61. Pools com emissão e morte progressiva ainda precisam de uma implementação própria, e não receberam essa regra indiscriminadamente.

## Verificações

`UI05SK212Tests.cpp` usa fixtures binárias `.eff`/`.par` e a produção de geometria real para verificar:

- Billboards diretos e aninhados com câmera inclinada: arestas preservadas, ortogonais e no plano da câmera, sem achatamento.
- Resultado conhecido de três rotações sucessivas: `(1,2,3)` → `(1,-3,2)` para 90° em cada eixo, na ordem DX9.
- Inclinação de um modelo filho e transformação de sua posição pela escala/rotação do pai.
- Separação de 0,9 entre o fallback do corpo e o canal de chão, sem alterar o tamanho dos modelos.
- Mesmo ID em alvo e chão produz duas instâncias.
- Atraso e parada absoluta de MODEL/STRIP.

A suíte existente cobre também as correções anteriores, incluindo as duas espadas na ação 12. O probe das 411 entradas agora exercita a câmera inclinada; gerar geometria finita não comprova fidelidade visual.

Os resultados executados, comandos, capturas e limitações estão no README e nos logs da entrega SK2.12. As prévias mostram canais isolados da skill 84; não são uma reprodução integral da sequência em combate.

## O que ainda difere / permanece pendente

Esta etapa recupera mecanismos demonstrados no código e melhora a comparação qualitativa, mas não declara o Berserk idêntico à referência. Faltam a matriz completa e o acompanhamento do dummy 2 do ator, emissão/vida individual dos pools, composição completa dos demais callbacks/estados e comparação de profundidade, filtros e ordem dos materiais.

Os coeficientes espaciais herdados da migração, como 0,3 nas malhas, foram preservados. A equivalência dessas unidades e a altura/largura exatas dos feixes ainda precisam de validação; não se aumentaram todos os efeitos para compensar a diferença visual. O tamanho dos billboards também tem uma convenção de semieixos herdada que precisa ser conciliada com o RectPlane original. O multiplicador provisório de impacto de Illusion Slash permanece em 2,5× e os assets de Frozen Arrow não foram ampliados.

A coluna de validação visual para Berserk indica comparação parcial com PNGs fornecidos pelo usuário, sem alinhamento temporal. Nenhuma skill foi marcada como completamente fiel ao DX9.
