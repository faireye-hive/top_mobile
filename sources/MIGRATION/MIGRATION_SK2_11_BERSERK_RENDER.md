# SK2.11 — Fundos pretos, desenho dos efeitos e armas durante skills

Esta etapa continua os fontes locais com SK2.9/SK2.10. As alterações estão aplicadas no projeto e o patch é incremental sobre o estado encontrado no início desta execução. Não há alteração de saves, dano, custos, bônus ou regras de missões/itens/NPCs.

## 1. Retângulos pretos: composição e alfa do destino

O Berserk atual é a skill 84: ação **12**, conjuração 117 (`spire01.par`) e efeito de chão 236 (`firegush.par`). O número 46 da linha é outro campo, não a ação. Os arquivos de brilho `spirea00`, `spirec00`, `spireh00`, `spirei00` possuem alfa 255 em todos os pixels, inclusive no fundo preto. Exemplo medido: `spirec00.tga.png` contém 10.516 pixels pretos opacos entre 16.384 pixels. Portanto, não basta procurar transparência na imagem para compor o efeito.

O `.eff` declara `(SRCALPHA, DESTALPHA)=(5,7)`. Com destino alfa=1, preto contribui zero e mantém a cor do cenário; o modo de compatibilidade usava transparência convencional e substituía o cenário pelo fundo preto opaco. O teste GL reproduz o defeito e confirma a composição correta por leitura de pixels.

- O tradutor de enumerações SK2.6 foi **preservado**. Os fatores históricos agora são utilizados por padrão. `TOP_HISTORICAL_BLEND=0` mantém a composição antiga para comparação.
- Efeitos históricos não herdam o descarte de alfa do shader de objetos transparentes. Há combinações em que um fragmento de alfa zero ainda modifica o destino; descartá-lo troca a operação declarada no arquivo.
- Uma textura que falha ao decodificar/carregar não é submetida como textura GL zero: isso amostrava preto opaco.
- Não se removeu preto por limiar de RGB e não se editaram as texturas históricas.

Há um segundo defeito mesmo com os fatores certos: sombras desenhadas com transparência podem reduzir o alfa de uma janela GL RGBA; o próximo efeito DESTALPHA escurece o cenário dentro do seu polígono. `MPRender::Init`, no DX9, escolhe o formato do display como backbuffer. Adotou-se a política de um backbuffer RGB, comum em X8R8G8B8, preservando alfa=1 após o clear. Isso é uma escolha fundamentada no código de inicialização e no modo RGB histórico, **não uma medição de um cliente DX9 executado nesta máquina**. Para comparar com o caminho RGBA anterior: `TOP_HISTORICAL_DEST_ALPHA=framebuffer`. A máscara de escrita preserva o alfa, mantendo os mesmos fatores enviados à GPU.

Referências: `arquivo/fontes/Engine/src/MPRender.cpp::Init`, `lwDeviceObject.cpp::InitState`, `MPModelEff.cpp::Render`, `MPParticleSys.cpp::RenderSoft`; documentação primária de [D3DBLEND](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dblend), [alfa do render target](https://learn.microsoft.com/en-us/windows/win32/direct3d9/render-target-alpha), [D3DFORMAT](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat) e [GetAdapterDisplayMode](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3d9-getadapterdisplaymode).

## 2. Modelos, UVs e escalas independentes

`CMPPartSys::Create/RenderSoft` aceita modelos `.eff` em vários tipos de partículas. O OpenGL só os resolvia para MODEL/STRIP. Nos demais tipos, o quadrilátero genérico podia usar `1.tga`: uma imagem auxiliar com RGB inteiramente preto, que não substitui o modelo e sua textura.

O mesmo leitor e os mesmos caminhos de malhas/billboards/modelos externos agora recebem as referências `.eff` dos demais tipos que utilizam o ramo de modelo. SHADE é explicitamente separado, pois o original renderiza `CMPShadeCtrl` e ignora esse ramo. Existem **375 sistemas adicionais** com referências `.eff` fora de MODEL/STRIP no acervo; nem todos são usados pelas habilidades e isso não certifica fidelidade visual desses sistemas.

Se o modelo declarado não existe ou não é interpretado, o runtime não inventa um quadrilátero preto com a textura auxiliar. O diagnóstico de recurso ausente/não interpretado permanece disponível em `TOP_TRACE_SKILL_FX=1`.

Billboards aninhados passam a usar seus UVs animados. Billboards diretos e aninhados preservam largura e altura independentes das chaves `.eff`, aplicando escala antes da rotação; antes, eram transformados em quadrados ou usavam apenas uma dimensão. Removidos os mínimos artificiais dessas dimensões quando a chave determina tamanho zero. Os coeficientes espaciais anteriores foram mantidos: não há aumento global de efeitos.

Para RIPPLE, `_CreateRipple` define uma posição dentro da faixa e `_FrameMoveRipple` anima escala/cor sem translação. O OpenGL deixa de aplicar velocidade/direção inventadas nesse tipo e respeita a primitiva/flag de billboard. RIPPLE nativo não recebe a curva adicional de crescimento do quadrilátero de chão genérico. Emissão, aleatoriedade de vida e matrizes parentais ainda precisam de trabalho.

## 3. Espadas acompanham a pose de habilidade

`STAttack.cpp::_UseSkill` chama `CCharacter::SwitchFightPose` antes da ação. No OpenGL, o local de anexação dependia apenas da área: fora de combate, espadas podiam continuar nos dummies das costas enquanto os braços executavam a skill.

`weaponAttachmentDummy` usa os dummies das mãos durante ataque e conjuração (estados visuais 2/5), nas duas mãos, independentemente da política de guardar armas no estado ocioso. `ActorSkin::build` continua usando a matriz completa do dummy do quadro `.lab`, incluindo orientação e o inverso do dummy de empunhadura do item. Não há correção por ID da skill, arma ou posição fixa.

O teste passa pela produção de geometria de `ActorSkin`: um esqueleto sintético executa a ação 12 com dummies de mão e de costas distintos. As duas espadas usam as mãos, avançam de posição no quadro seguinte e retornam às costas ao ficar ocioso. As capturas reais carregam os modelos e a ação 12 da tabela atual; não substituem comparação com DX9.

## 4. Componente final de buffs não era emitido

O caminho de self-cast emitia apenas `Cast`. No DX9, `CHitRepresent::ActionExec` verifica `sActionHarm==1` e `chTargetEffectTime`, despachando o resultado também para ações de buffs. No Berserk, o efeito de chão 236 usa `ACTION_END_HIT=-2`.

`Play` agora preserva as colunas 45/64 e a presença dos canais 63/65/66. Para self-casts sem voo, agenda o evento de resultado independentemente da conjuração, em início/chave/fim conforme o arquivo. Usa os tempos da ação e o cancelamento por ciclo já existentes; não duplica a aplicação do bônus ou o dano. A regressão verifica o resultado do Berserk ao fim de uma conjuração repetida, cancelamento do evento anterior e ausência de repetição do resultado.

Referências: `arquivo/fontes/Client/src/STAttack.cpp::ActionBegin/ActionFrame/ActionEnd`, `EffDelay.h::CHitRepresent::ActionExec`, `EffDelay.cpp::CHitRepresent::Exec`, `arquivo/fontes/Libraries/common/src/SkillRecord.cpp`.

## 5. Verificação e reprodução

```sh
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
xvfb-run -a ./build/top_effect_blend_gpu_tests
python3 sources/Tools/skill_migration_status.py --client Client --build build --out migration-coverage
```

O teste GL é executado explicitamente em ambiente com display; não é imposto à suíte headless. Ele verifica os 19 pares dos arquivos atuais pela equação de mistura, reproduz o retângulo preto antigo, valida alfa zero e testa uma sombra seguida de um efeito DESTALPHA.

Prévia visual de um canal e da ação histórica, sem aprender a skill nem consumir SP:

```sh
./build/top_gl --skill-preview 84 --skill-preview-time .35 --shot /tmp/berserk.ppm
./build/top_gl --skill-preview 84 --skill-preview-channel hit --skill-preview-time .25 --shot /tmp/berserk-result.ppm
TOP_HISTORICAL_BLEND=0 ./build/top_gl --skill-preview 84 --skill-preview-time .35 --shot /tmp/berserk-compat.ppm
```

A prévia mostra **componentes isolados**. `--skill-preview-channel` aceita cast (padrão), hit e flight; canais não declarados são rejeitados. Ela exige `--shot` e não substitui a timeline do combate normal. As capturas desta entrega usaram cópias temporárias de `Client/user` e `offline`.

## Limitações mantidas

O efeito do Berserk já não apresenta o retângulo preto reproduzido na comparação de composição, e as regressões verificam as correções implementadas. Isso não certifica todas as skills nem todos os estados gráficos. Ainda faltam a ponte completa de matrizes para os próprios efeitos, rastros CMPStrip diretos, cinemática/emissão exata dos vários pools, estados persistentes de efeitos e comparação visual lado a lado com um cliente DX9 funcional. A política de alfa do destino deve ser comparada se uma referência usar backbuffer ARGB. O impacto provisório de Illusion Slash permanece em 2,5× e Frozen Arrow não foi ampliado.
