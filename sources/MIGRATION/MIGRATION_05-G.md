# Migração 05-G — Recursos históricos por ID

Base: Migração 05-F. Sem alterações de rede, de UI, da lógica do dano ou dos saves.

## Funcionalidade entregue

* `MagicEffectCatalog.h`: lê `Client/scripts/table/magicsingleinfo.txt` em vez de
  presumir que todos os IDs visuais representam o efeito 38.
* `EffectSystem::emit()` resolve sob demanda o ID de efeito retornado por
  `SkillVisualCatalog`. Se o ID estiver em `magicsingleinfo.txt`, localiza seus
  arquivos de modelo `.eff` e controladores de partículas `.par` (inclusive o
  `Result` de impacto). A resolução evita caminhos `../` e limita entradas em
  cache, quantidade de subefeitos e tamanho dos nomes.
* Quando uma referência de textura do `.eff` pode ser identificada pelo
  leitor existente, seu próprio atlas/textura convertida é usada na renderização
  daquele efeito. Quando um `.par` válido está presente, suas partículas e
  texturas alimentam os billboards/ribbons OpenGL já existentes.
* Quando o recurso não existe, não está em uma tabela compatível, foi
  convertido parcialmente ou não pode ser interpretado, mantemos o efeito
  genérico **sem alterar a habilidade e seu momento de dano**.
* O `--check` mostra a quantidade de registros de `magicsingleinfo.txt`.

## Limitações explícitas

O leitor de `.eff` **não é ainda um decodificador completo** dos modelos de
`CMPModelEff`: extrai referências ASCII validadas e tenta obter as texturas.
Não interpreta geometria, shaders ou hierarquia de movimentos `.eff`.
Arquivos `.par` usam o decodificador parcial de `CMPPartCtrl` já existente na
05-F; não são executados exatamente como no DirectX 9.

Os IDs de `skillinfo.txt` e `magicsingleinfo.txt` pertencem a domínios
históricos diferentes e **nem sempre têm correspondência 1:1**. Somente
IDs presentes no catálogo são resolvidos; outros usam fallback. Os arquivos
binários `.eff`/`.par` de cada efeito são necessários no diretório
`Client/effect/`, e suas texturas convertidas devem estar em
`Client/texture/effect/` ou no `texture_manifest.csv`.

O `Client.zip` de scripts usado para inspeção contém `magicsingleinfo.txt`
com aproximadamente 53 registros de efeito, mas não contém todos os
binários dos efeitos ou texturas. O teste automatizado usa arquivos binários
mínimos como fixtures para verificar resolução e fallback, **não comprova
fidelidade visual** com o cliente original.

## Instalação e validação

Na raiz do projeto, com as pastas `sources/` e `Client/`:

```sh
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

```sh
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui05f_tests top_ui05g_tests -j4
ctest --test-dir build-ui-tests -R 'ui05(f|g)_' --output-on-failure
```

Para executar *todos* os testes registrados, compile todos os executáveis:

```sh
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

## Próxima etapa sugerida

Aprimorar o decodificador binário de `.eff` com layouts de `CMPModelEff`
verificados no código original, usando os arquivos binários completos do
cliente, depois migrar mais tipos de emissor e a sincronização de efeitos
ligados aos pontos de fixação e ao terreno.
