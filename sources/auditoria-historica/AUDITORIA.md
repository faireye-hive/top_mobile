# Auditoria técnica dos efeitos históricos

**Escopo:** leitura estática do cliente histórico, tabelas, arquivos `.eff`/`.par`, manifesto de texturas e implementação OpenGL. Nenhum arquivo existente foi alterado e não houve execução de testes gráficos ou recompilação completa. Os quatro arquivos CSV deste diretório são os dados detalhados que fundamentam este resumo.

## Resultados do inventário

- **Habilidades examinadas:** 411 linhas válidas de `Client/scripts/table/skillinfo.txt` (IDs podem incluir registros de monstros/ilusões e não equivalem a habilidades jogáveis únicas).
- **Arquivos de efeito identificados como dependência direta:** 288 únicos: 52 `.eff` e 236 `.par`. A contagem inclui os efeitos simples referenciados pelos grupos; arquivos `.eff` aninhados são listados quando resolvidos em dependências.
- **Dependências registradas:** 1.139 relações entre skill/canal, ID mágico ou de grupo e recursos. A tabela também contém referências dos subefeitos reconhecidos.
- **Tabelas históricas:** 411 habilidades lidas; `magicsingleinfo.txt`, `magicgroupinfo.txt` e `sceneffectinfo.txt` são usados para resolver IDs, grupos e controladores de cena. Cada linha CSV registra caminho/linha de origem sempre que possível.
- **Arquivos ausentes/incompatíveis com nome declarado:** 42 relações estão marcadas `MISSING` ou `UNMAPPED`; são relações, não 42 arquivos únicos. Recursos omitidos por formatos incompreendidos podem elevar esse total. Arquivos `.par` são catalogados por existência, sem alegar validação individual do parser.
- **Texturas:** a resolução consultou `Client/texture/texture_manifest.csv` antes de marcar uma textura como ausente. Exemplo: `ice1` do resultado Frozen Arrow resolve para `Client/texture/effect/Ice1.TGA.png` (manifesto preserva caixa e conversão).
- **Modelos externos:** o modelo do recurso `icearrowh.eff` é `Client/model/effect/ice.lgo` e existe. O efeito tem versão 7 e seis subefeitos; os seis usam textura `ice1` e modelo `ice.lgo`. Os caminhos aparecem em `dependencies.csv`.

## Distinções de evidência

- **Recurso existe:** caminho físico encontrado no checkout do cliente.
- **Recurso interpretado:** o leitor reconheceu a estrutura. Para `.eff`, `effects_catalog.csv` distingue `full`, `header_only` e `unknown`; o catálogo contém 51 estruturas `.eff` completas e a entrada adicional `icearrowh.eff`, também completa, totalizando **52 `.eff` associadas e 52 completas**.
- **Recurso suportado:** há caminho de carregamento/renderização OpenGL correspondente. Isso não significa fidelidade integral.
- **Recurso validado:** há evidência concreta de funcionamento. O único caso declarado visualmente validado no escopo fornecido é Illusion Slash (skill 81). Testes automatizados citados no repositório não substituem validação visual DX9/OpenGL; nenhum foi executado nesta tarefa.

## Casos de referência

- **Illusion Slash (81):** tabela aponta animação `14`, ator `186`, item `218`, voo `1018`, impacto em alvo `195`; ID mágico 18 aponta para `04000009.eff`. O `.eff` é versão 5, uma peça, 3 quadros, `RectPlane` nativo e textura `eff0009`. A tabela de magia informa zero partículas: o voo não depende de `.par` separado. A validação visual prévia é informação fornecida pelo usuário, não repetida nesta auditoria.
- **Frozen Arrow (93):** animação `7`, efeito de conjuração/arma `296`, voo `1020` (magia 20); a magia aponta para `icearrowf.eff`, `icearrowf(a).eff`, controlador `icearrowf` e resultado `icearrowh`. Os arquivos `icearrowh.eff` e `icearrowh.par` existem. O resultado `.eff` foi interpretado: seis subefeitos usam `ice.lgo` e `ice1`; o modelo existe e a textura convertida consta no manifesto. O código tem escala experimental específica para esta skill e a documentação de migração diz que requer verificação visual; a diferença visual conhecida permanece não validada nesta auditoria.
- **Dual Shot (90):** voo visual `2002` resolve pelo grupo 2, que contém três componentes: magias 10, 25 e 26, uma ocorrência de cada, em modo sequência. São referências a `runningattack.eff`, `runningattack2.eff` e `runningattack3.eff`; a composição e atrasos têm implementação no OpenGL. Isso confirma expansão dos três componentes, não paridade visual final.

## Cobertura do formato e do renderizador

### `.eff`

O inspetor diferencia cabeçalho reconhecido de documento completo. O decodificador estrutural aceita versões 1–7 e lê subefeitos, nomes, tipo, blend, duração, keyframes (tempo, escala, ângulo, posição, cor), trilhas UV/regiões, textura, modelo, billboard, quadros de textura e parâmetros animados de Cone/Cylinder. A estrutura preserva essas trilhas; a renderização é restrita.

O OpenGL renderiza billboards, sete primitivas nativas (`Triangle`, `TrianglePlane`, `Rect`, `RectPlane`, `RectZ`, `Cone`, `Cylinder`) e geometria externa `.lgo` de `Client/model/effect/` sob limites estruturais. Tem amostragem de transformação, keyframes e UVs. Modelos não reconhecidos, geometrias `.lgo` fora dos limites ou referências sem textura resolvida são ignorados. Só o par de blend `source=5,dest=7` tem mapeamento específico auditado; outros caem no fallback alpha/additivo. Cabeçalho identificado não significa que geometria DirectX, shaders ou todos os modos de transparência foram reproduzidos.

### `.par`

`EffectSystem.cpp::loadParticles` é o leitor existente: aceita versões 2–15, de 0 a 100 sistemas por controlador, e lê tipo, contagem, textura, modelo, faixa, quadros de tamanho/rotação/cor, vida, velocidade, direção/aceleração, passo, offset, atraso, duração e caminhos nas versões que possuem esses campos. `LegacyParticleCurves.h` interpola tamanho, RGBA e rotação por idade normalizada. O inventário não executou esse leitor para cada controlador individual; assim, não informa quantos `.par` foram estruturalmente aceitos.

Os tipos 5 e 6 vinculam modelos `.eff`. Para STRIP (6) e MODEL (5), o código resolve até oito trilhas aninhadas, limitadas a billboards ou primitivas nativas reconhecidas com textura. Existem ramificações específicas para tipos 1, 2, 3, 4, 7, 9, 10, 11, 12, 16, 17 e 18. Os tipos restantes que não entram no caminho de modelo acabam no quad genérico. Portanto, o campo `tipo` reconhecido no binário não garante semântica histórica equivalente.

### Canais e dados de resultado

`skillinfo.txt` oferece canais de ator, item, item extra, voo, alvo, chão e água. O runtime seleciona canais por fase, mas o próprio comentário de `EffectSystem.cpp` identifica a composição de keyframes/dummies individuais de item como trabalho incompleto. `magicsingleinfo.Result` é um controlador complementar: `icearrowh` é resolvido adicionalmente, e não deve ser descartado por Frozen Arrow não ter ID de impacto separado nos campos de `skillinfo`.

## Incompatibilidades agrupadas e alcance

| Tipo | Evidência e suporte | Quantidade afetada |
|---|---|---|
| `.eff` sem geometria reconhecida ou sem textura resolvida | Implementação pula subefeito; caminho confirmado no `EffectSystem.cpp` | Não calculada com confiança para todas as 411 skills |
| Modos de blend além do par explicitamente mapeado | Usa fallback alpha/additivo | Não calculada; depende das peças `.eff` efetivamente usadas |
| `.par` não decodificado ou fora da cobertura semântica | Leitor existe, porém inventário individual não executado; tipos não tratados usam quad genérico | Não calculada; `.par` por existência apenas |
| STRIP/MODEL com dependência/forma fora dos limites | Limite de oito trilhas e suporte parcial a geometrias | Não calculada |
| Sincronização de item por dummy/keyframe | Canais existem, anexação temporal individual segue incompleta | Potencialmente compartilhada; quantidade confirmada indisponível |
| Expansão de grupo | IDs deduplicados e máximo de oito visuais por evento | Grupo 2002 tem exatamente 3 membros; demais ocorrências não somadas nesta auditoria |

Não se atribui uma contagem por habilidade inferida a partir de arquivos compartilhados. Assim, as linhas acima dão contagens confirmadas apenas quando a relação foi explicitamente resolvida (por exemplo, os 3 membros de Dual Shot); os demais alcances são estimativas potenciais e foram deixados como “não calculado”.

## Cinco prioridades técnicas para a próxima etapa

1. **Criar um inventário executável do leitor `.par`** que rode cada controlador referenciado e exporte versão, sistemas, tipo, quantidade, curvas, tempos, modelo e textura, incluindo falha do parse. Isso transforma dependências existentes em cobertura interpretada mensurável.
2. **Medir cobertura e falhas de STRIP/MODEL por dependência compartilhada**, incluindo `.eff` aninhado, `.lgo`, textura e trilhas recusadas. Priorizar modelos reutilizados por mais habilidades.
3. **Completar a equivalência de tipos de partícula com base no enum e no código histórico**; trocar o quad genérico por semântica específica onde houver evidência e calcular habilidades afetadas a partir do catálogo.
4. **Auditar os canais de habilidade e grupos**, especialmente associação de dummies/keyframes de itens e limites de expansão; medir alcance via `skillinfo.txt` antes de mudar comportamento.
5. **Mapear todos os pares de blend e recursos de material/shader `.eff`** contra a implementação OpenGL, preservando escalas e valores autorados; priorizar pares compartilhados e validar visualmente em conjunto controlado DX9/OpenGL.

A prioridade imediata de coleta é a primeira, pois reduz desconhecidos em cascata e permite quantificar de modo reproduzível as prioridades 2–4. Não se recomenda aumentar efeitos: o objetivo deve continuar sendo reproduzir recursos, escalas, trajetórias e temporização históricos.

## Limitações desta execução

- O texto histórico do cliente DirectX original não foi localizado em `sources/`; os significados foram inferidos dos comentários/documentação de migração existentes e dos leitores atuais.
- Não houve decodificação por arquivo de cada `.par`, apesar de a implementação do leitor ter sido examinada.
- Os caminhos de efeitos de cena foram analisados como IDs/controladores nas tabelas; não foi construído grafo completo dos pontos de chamada de cada efeito de cena no gameplay.
- Não foram realizados testes visuais nem compilação integral, conforme as restrições da tarefa.
