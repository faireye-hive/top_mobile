# Tales of Pirates — 05-X5: cache exato de geometria de atores na GPU

Base: **05-X4**. Cliente offline OpenGL/C++17. Nao modifica formatos de saves,
interface, camera, mapas, efeitos, golpes, IA, equipamentos ou animacoes.

## Motivacao

No perfil real do Ryzen 5 4600G, a 05-X4 reduziu o tempo de processamento de
personagens para ~3,4-6,2 ms, mas o render ainda envia em torno de 2,5-4,35
MiB de geometria dinamica por quadro. Esse envio e necessario quando o modelo
muda, mas nao quando os vertices sao identicos aos do quadro anterior.

## Implementacao

- `ActorGpuCache.h`: guarda uma copia exata (bytes) dos lotes de vertices
  enviados ao OpenGL, separados por indice, textura e modo de renderizacao.
- O lote sem alteracao reutiliza seu VBO. Um lote com nova animacao, posicao,
  equipamento, geometria, cor, textura ou modo e atualizado integralmente.
  Uma comparacao exata (`memcmp`), em vez de somente um hash, evita colisao.
- `GfxDevice::uploadActorMesh`: VBO retido com `GL_DYNAMIC_DRAW`. O upload
  substitui a memoria anterior para evitar bloquear o processamento de quadros
  ainda em andamento. Buffers sao liberados quando os lotes desaparecem, numa
  troca de mapa ou no encerramento.
- Fallback seguro: caso o upload nao forneca VBO valido, usa o `draw()` original.
- O cache e independente do cache estatico de terreno/cenario da 05-X2/05-X3.

## Comparacao A/B (mesmo local e configuracoes)

```bash
./build/top_gl --profile
./build/top_gl --profile --no-actor-cache
```

A linha adicional `profile cache atores: enviado ... MiB | reutilizado ... MiB`
indica o volume evitado. No segundo comando, `OFF` aparece e os atores usam
`GfxDevice::draw()` em todos os quadros. `--no-gpu-cache` continua desativando
apenas o cache do terreno/cenario; e possivel combinar as duas flags.

Meça apos estabilizar o jogo e no mesmo local; a GPU integrada utiliza RAM
compartilhada. Comparacoes de uma regiao movimentada com uma regiao vazia nao
permitem atribuir um ganho ao cache.

O cache pode ter custo de CPU/memoria para comparar e guardar vertices. Por
isso, nao afirmamos um ganho de FPS antes da medicao real. Se houver regressao,
use `--no-actor-cache` para desativar sem voltar de versao.

## Testes

`ui05x5_actor_gpu_batch_cache` exercita com dados sinteticos:
reutilizacao, cambio de pose/posicao, textura, modo de blend, troca de ordem
de lotes, limpeza, falha de upload e retentiva de recursos. O teste nao abre
um contexto OpenGL. As 36 suites anteriores nao tiveram codigo-fonte alterado,
foram reexecutadas a partir dos executaveis validados da 05-X4.

A compilacao e validacao visual de OpenGL precisam ser realizadas no Linux do
usuario. Verifique tambem posicao das duas espadas, sombras, HP dos monstros,
poses apos teleporte e integridade do chao ao caminhar.
