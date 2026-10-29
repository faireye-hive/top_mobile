# Recuperação de modelos de cena — auditoria 2026-10-23

## Resultado

A revisão lista **78 modelos `.lmo` distintos ausentes**, somando **1.944 instâncias** nos mapas `ctf`, `kyjj_1`, `kyjj_2`, `kyjj_3`, `l4map`, `room`, `starwalkway` e `tribulation`. A busca encontrou **0/78 origens exatas** em arquivos extraídos sob `arquivo/` e nos arquivos ZIP examinados. Não há origem verificável para copiar; portanto, nenhum candidato foi copiado e nenhuma substituição por modelo parecido foi considerada. O CSV anexo contém destino esperado, ocorrências por mapa e total de instâncias por modelo.

## Escopo pesquisado

- Árvore local `arquivo/` (incluindo recursivamente o cliente arquivado) e `Client/`, por nome exato de arquivo.
- Índices de membros de ZIPs sob `arquivo/`, `ZIPMIGRAÇÂO_ignorar/`, `backup_ignorar/` e ZIPs na raiz do repositório; comparação do basename com os 78 nomes `.lmo` esperados.
- Catálogos de quatro arquivos RAR encontrados; o conteúdo é código-fonte e o 7-Zip não consegue testar a extração por método de compressão não suportado. Nenhum membro `.lmo` correspondente foi listado.
- `ZIPMIGRAÇÂO_ignorar/sources_backup.tar.zst` lista apenas 49 itens de fontes; não é um pacote de assets de cliente.

A busca não percorreu diretórios de credenciais nem diretórios fora do repositório. ZIPs não foram extraídos para o projeto. Como nenhuma correspondência exata apareceu, não há bytes ou SHA-256 de origem a validar.

## Impacto por mapa

| Mapa | Instâncias referenciando modelos ausentes |
| --- | ---: |
| `ctf` | 16 |
| `kyjj_1` | 203 |
| `kyjj_2` | 241 |
| `kyjj_3` | 358 |
| `l4map` | 205 |
| `room` | 19 |
| `starwalkway` | 544 |
| `tribulation` | 358 |

## Fontes de posicionamento ausentes

A auditoria também não encontrou estes arquivos `.obj`; eles são os próprios arquivos de posicionamento e sua estrutura não pode ser reconstruída a partir dos modelos ausentes:
- `map/binglang2.obj` (mapa `binglang2`)
- `map/heilong.obj` (mapa `heilong`)
- `map/hell2.obj` (mapa `hell2`)
- `map/hell3.obj` (mapa `hell3`)
- `map/hell5.obj` (mapa `hell5`)
- `map/lobby.obj` (mapa `lobby`)

Outros quatro mapas têm apenas divergência de maiúsculas/minúsculas na referência (`CTF.obj`, `DreamIsland.obj`, `PKmap.obj`, `sdBoss.obj`); isso é um problema separado de sensibilidade a caixa, e não fonte de modelo recuperada.

## Destinos esperados e confirmação após integração

Cada arquivo, se encontrado futuramente em outro acervo, deve ser verificado como o mesmo caminho e nome `.lmo`, com tamanho e SHA-256 registrados antes da integração. O destino previsto é `Client/` + caminho da coluna `destino_relativo` no CSV. Após a integração aprovada, regenere `revisao-atual-assets.json` com `top_asset_audit` usando `--client Client`; confirme que `scene_models_missing_used` e `scene_missing_instances` caem, e confira mapa a mapa os registros `scene_used`. Arquivos `.lmo` corretos não garantem compatibilidade de formato DX9/OpenGL ou carregamento em tempo de execução, conforme a ressalva do próprio relatório de auditoria.
