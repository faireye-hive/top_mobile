# SK2.3 — auditoria executável dos controladores `.par`

A SK2.3 é **somente leitura**. Não muda a renderização, escala, animação, efeitos
ou lógica de skills. Sua finalidade é inventariar todos os `.par` de `Client/effect/`,
independentemente de estarem referenciados pelas tabelas de habilidades.

## Compilar e executar

Na raiz do repositório (`TalesOfPirateDX9/`):

```bash
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build --target top_par_audit top_sk23_par_tests -j$(nproc)
ctest --test-dir build -R ui05sk23_all_par_audit --output-on-failure
./build/top_par_audit --client Client --out build/auditoria-par-sk23
```

Caso queira usar a auditoria numa integração que falhe diante de erros de leitura
ou dependências ausentes, acrescente `--strict`. Sem `--strict` o comando termina
com código 0 quando o inventário foi produzido, mesmo contendo `.par` inválidos
ou arquivos de dependência ausentes. Código 2 indica argumento ou diretório
inválido. A auditoria não escreve em `Client` nem permite `--out` dentro dele.

## Resultados

- `par_files.csv` — arquivo, tamanho, versão, nome do controlador, sistemas,
  bytes lidos, bytes restantes e erros. `decoded` não significa efeito visível.
- `par_systems.csv` — **uma linha por sistema**, tipo histórico numérico, contagem
  original (sem limitar a 100 ou 24), curvas, tempos, referências e flags.
- `par_dependencies.csv` — referências a texturas, `.eff`, `.lgo` e nomes não
  classificados, com status. Nomes como `RectPlane` não são marcados `missing`.
- `par_failures.csv` — falhas de decodificação e offset aproximado.
- `par_summary.json` — totais, versões, tipos, referências não encontradas.

A busca de texturas considera variantes convertidas `arquivo.tga.png`,
`arquivo.dds.png` e `arquivo.png`, bem como diferenças de maiúsculas/minúsculas
no diretório `Client/texture/effect`. Não reescreve referências nem inventa
aliases: casos ambíguos permanecem sem resolução.

O parser segue o layout e limites estruturais presentes em
`Engine/src/top/EffectSystem.cpp::loadParticles` (versões 2–15, 100 sistemas,
256 quadros, 200 pontos de caminho). Mantém valores crus para permitir medir
perdas provocadas pela normalização do renderizador. **É uma auditoria
independente, não uma segunda implementação de efeitos no gameplay.**

## Interpretação e limitações

- `decoded`: os campos conhecidos foram lidos; **não** implica suporte de
  renderização, semântica fiel, modelo vinculado utilizável nem validação visual.
- `empty_controller`: a estrutura tem zero sistemas, portanto o loader atual
  rejeitaria o conteúdo para fins de renderização.
- `trailing_bytes`: bytes não consumidos pelo layout conhecido. Alguns `.par`
  históricos possuem 8 bytes adicionais; isso **não** os torna inválidos.
- `model_unclassified`: nome de primitiva ou outro modelo sem extensão.
  Não é erro de recurso ausente.
- Referências `.eff` são verificadas por existência, não por decodificação
  profunda ou renderização. `par_dependencies.csv` é um grafo de primeira camada.
- Não cruza, nesta etapa, os tipos de partícula com as 411 skills nem valida
  equivalência com o DirectX; essa análise poderá ser feita com estes CSVs.
- Requer host little-endian (o mesmo contrato binário do leitor atual).

## Observação para SK2.4

Use `par_summary.json` + `par_systems.csv` para determinar quais **tipos**
são mais frequentes. Cruze com `dependencies.csv` do Luna para priorizar tipos
compartilhados por muitas habilidades. Nunca aumente globalmente escalas como
substituto para geometria, partículas ou fase `Result` ausentes.
