# SK2.18-M — velocidade inicial do barco derivada das tabelas DX9

## Resumo

O deslocamento naval no porte deixava de consultar os dados do casco e usava `10.f` como velocidade fixa. Agora `ActorSkin` soma a velocidade base do `shipinfo` e as velocidades das peças padrão que o DX9 seleciona, e `main.cpp` usa esse valor para mover o barco. A soma é convertida para metros por segundo dividindo por 100, alinhada à unidade usada pelo porte de movimento terrestre.

## Evidência: DX9 x OpenGL

| Original DX9 | Porte antes | Mudança | Referências |
|---|---|---|---|
| `xShipSet::_ReadRawDataInfo` fixa `byIsUpdate=1` e lê velocidade base do navio e `sSpeed` de peças (`ShipSet.cpp`, linhas 19-23 e 80-123). `CCharBoat::GetData` acumula `dwSpeed` das partes, incluindo peças condicionais a `byIsUpdate` (`CharBoat.cpp`, linhas 1410-1505 e continuação até 1598). | O laço de movimento usava `10.f` em modo barco (`main.cpp`, linha 2515 no HEAD). `setBoatMode` carregava casco/altura do mar, sem expor velocidade. | Somar velocidade base + casco + primeira opção de fore/engine/cannon/equipment e motores do engine; usar a soma como velocidade do barco. `main.cpp` consulta `actors.boatMoveSpeed()`. | Porte: `ActorSkin.cpp` linhas 1610-1706 no HEAD; `main.cpp` linha 2515 no HEAD. Dados Client: `Client/scripts/table/shipinfo.txt`, linha 2; `shipiteminfo.txt`, linha 16. |
| `Character.cpp` usa `getMoveSpeed()` nas rotinas de movimento (`2319`, `2343`). `ShipSet.cpp` especifica `sSpeed` como o último atributo numérico da tabela de peça. | Nenhuma conversão dos atributos de barco para a velocidade de movimento do porte, pois havia constante fixa. | O código divide o total por `100.f`. Para Guppy, a opção inicial é motor/remo 15, velocidade 450, resultando em 4,5 m/s. | Porte: `Play.cpp:1135` converte a velocidade de `characterinfo` por 100; `ActorSkin.cpp` calcula e converte em `boatMoveSpeed_`. |

### Fatos, inferências e desconhecidos

- **FATO:** DX9 soma `sSpeed` das peças em `Data.dwSpeed` quando `byIsUpdate` está ativo; `ShipSet.cpp` força esse modo ativo.
- **FATO:** o Client lista o Guppy com casco 43 e primeira opção de engine 15; `shipiteminfo.txt` registra 450 para a peça 15.
- **FATO:** o DX9 seleciona as primeiras opções ao criar o barco (`CharBoat.cpp:486-500`); o porte soma essas opções disponíveis.
- **INFERÊNCIA:** `/100` converte atributo da tabela para unidades de mundo do porte. É coerente com a conversão terrestre `Play.cpp:1135`, mas falta medição pareada de deslocamento em DX9 para provar a escala.
- **DESCONHECIDO:** seleção de peças melhoradas após compra/equipamento. A UI offline atual guarda o ID do barco, mas não a configuração completa das peças; esta mudança usa as opções padrão.

## Como testar

```bash
cmake --build build --target top_gl -j4
./build/top_gl --map magicsea --check
```

A verificação de barco do `--check` confere a velocidade de 4,5 m/s do Guppy antes de conferir a contagem mínima de atores. Na execução registrada, essa asserção específica não falhou; o comando terminou com código 1 mais tarde porque o ambiente da checagem não tinha dois atores carregados. Build do alvo `top_gl`: código 0. CTest completo não foi executado nesta etapa. Nenhuma comparação visual com DX9 foi feita.

## Limitações

Não representa upgrade de componentes selecionados pelo jogador. O divisor de unidade é inferido, não medido no cliente DX9. A verificação total `--check` segue falhando pela condição de atores do ambiente. Mudança funcional do movimento do barco não foi comparada visualmente.
