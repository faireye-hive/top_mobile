# SK2.15-A — CMPStrip diretamente no CMPPartCtrl (.par)

## Objetivo

O cliente DX9 possui duas coisas distintas que costumavam ser confundidas:

* `CMPPartSys` do tipo 6 (STRIP): sistema convencional de partículas, já parcialmente migrado.
* `CMPStrip` dentro do controlador `CMPPartCtrl` (`m_iStripNum` a partir da versão 7): faixa que registra **dois dummies reais** de um personagem ou modelo de item ao longo dos quadros. Não é um sistema `type=6`.

Referências originais: `MPParticleCtrl.cpp::CMPPartCtrl::LoadFromFile`,
`MPModelEff.cpp::CMPStrip::LoadFromFile`, `::Play`, `::GetTrack`, `::FrameMove`, `::Render`.

## Modificações

1. `LegacyDirectStrip.h` decodifica o trailer após os sistemas `.par`: limite em vértices, IDs de dois dummies, cor, vida, passo de amostragem, textura e par D3DBLEND. Possui validação de tamanhos, NaN e truncamento.
2. `EffectSystem::loadParticles` preserva controladores com **zero** sistemas de partículas e pelo menos uma faixa CMPStrip, como `swordlight02.par`.
3. Cada instância coleta pares de posições (em coordenadas globais) apenas de matrizes reais. O OpenGL recebe dois triângulos por segmento, UV longitudinal crescente, cor e desaparecimento gradual por idade de cada par, preservando o blend histórico.
4. `ModelFile::loadGeom` guarda todos os dummies válidos de um modelo de item, além do dummy 0 usado anteriormente para alinhamento da empunhadura.
5. `ActorSkin::playerItemDummyWorldMatrices` converte os dummies do item principal equipado para o espaço global usando **a mesma matriz de mão e de mundo aplicada na renderização das armas**. Prefere slot 9; recorre ao slot 6 se não houver o principal. O item sem dois dummies válidos não recebe uma faixa fictícia.
6. `ActorSkin::playerDummyWorldMatrices` também alimenta os controladores de faixas corporais sem ativar as correções opt-in de outros efeitos.
7. `top_fx_coverage_audit` passa a descrever CMPStrip como `partial_anchor_dependent`, sem alegar paridade visual ou cobertura de monstros.

## Evidências

No pacote histórico de `Client/effect`, a leitura confirmou **1031 arquivos .par válidos e 6 faixas diretas**. A lista original do inventário SK2.12 continua sendo a referência para as seis definições:

* `01000012.par` — dummies 1 e 2
* `01030010.par` — dummies 0 e 1
* `mosterlight.par` — dummies 9 e 15
* `swordlight02.par` — dummies 0 e 1
* `xzsb1.par`, `xzsb2.par` — dummies 3 e 6

As seis faixas são decodificáveis; isso **não** significa que seus donos estejam renderizáveis. A implementação nesta etapa trata as origens de **habilidades do jogador** (ator e item equipado), quando ambas as matrizes existem. Não foi implementada associação aos modelos de monstros, itens no mapa ou efeitos de cena em geral.

## Instalação

No repositório com SK2.14 aplicada:

```bash
git apply --check SK215A.patch
git apply SK215A.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

O patch não altera dano, alcance, saves, escala global, timeline SK2.7 ou o impacto do Illusion Slash (2,5×).

## Diagnóstico em execução

```bash
TOP_HISTORICAL_BLEND=1 TOP_TRACE_SKILL_FX=1 TOP_TRACE_DIRECT_STRIP=1 \
  ./build/top_gl --profile 2>&1 | tee sk215.log
grep '\[direct-strip\]' sk215.log
```

* `strips=1 owner=item`: o recurso foi identificado e enviado para a instância.
* `missing_dummies=0,1`: não foram fornecidas as duas matrizes pela arma atual; sem improvisar faixa genérica.
* `pairs=2 triangles=2`: dois pares históricos amostrados geraram o primeiro segmento visível (e serão expandidos conforme a animação da arma).

Se `swordlight02.par` produzir `missing_dummies`, conferir se o modelo equipado tem ambos os dummies 0 e 1. A implementação não transforma dummies de esqueleto em falsos dummies de arma.

## Verificação feita no ambiente de implementação

* Leitura de todos os **1031 `.par`** fornecidos, com **6** CMPStrip válidas e **0** falhas do decodificador de trailer.
* Nova suíte CPU-only `ui05sk215_direct_controller_cmpstrip` aprovada: leitura, rejeição de truncamento, UV, vida/alpha e isolamento de dummies ator/item.
* Regressões selecionadas (SK2.10, SK2.11, SK2.12, SK2.14, SK2.15): **6/6** aprovadas em Debug.
* Reexecução da auditoria sobre 1031 `.par` e 1227 `.eff`: **19 lacunas, drift=0**, agora CMPStrip = `partial_anchor_dependent`.
* `top_gl` completo não compilado aqui: faltam `GL/gl.h` e libGL de desenvolvimento. Renderização e aparência dos efeitos ainda não foram verificadas em um contexto OpenGL.

## Limitações explícitas

* O movimento da **meia-lua de voo** `04000009.eff` é separado do CMPStrip do canal de item. A SK2.15-A não adiciona uma curva artificial de crescimento da meia-lua; o efeito visual precisa ser comparado no jogo.
* O trail é amostrado por quadro de renderização e limiar `_fStep`. O DX9 mantém um acumulador em `FrameMove` e modifica a idade dos `track` em `Render`. A implementação aproxima o desaparecimento pela idade em segundos; diferenças de tempo em FPS não constante ainda são possíveis.
* A escolha do item principal usa o slot 9 como prioridade e o 6 como fallback. Casos de duas armas, monstros e dummies de equipamentos alternativos precisam de associação explícita.
* A renderização de `CMPStrip` de um personagem distante ou de um item sem as matrizes relevantes **não** é fabricada.
