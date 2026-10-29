# Migração 04-J — estabilização do carregador de fonte (incremental)

Base: TalesOfPirates_04-I_Completo.zip.

- `Engine/src/top/Font.cpp`: valida largura, altura e passo (pitch) do bitmap FreeType antes de copiar os pixels para o atlas de fontes. Caso retornem valores incompatíveis com a fonte de 11 px, interrompe a carga de fonte em vez de continuar processando dimensões suspeitas.
- Não modifica renderização 3D, mapas, minimapa, rede nem regras offline.

Validação local: `top_ui04d_tests` a `top_ui04i_tests` compilados e 6/6 passaram pelo CTest. A falha `std::bad_alloc` informada no Linux do usuário não se reproduziu aqui, portanto a origem ainda não está demonstrada e a alteração é defensiva, não uma correção comprovada.

Teste no Linux:

```sh
find sources -type f -exec touch {} +
rm -rf build-ui-tests
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui04h_tests -j2
ctest --test-dir build-ui-tests -R ui04h_layered_visibility --output-on-failure
```

Se falhar novamente, colete backtrace com:

```sh
gdb -batch -ex run -ex 'bt 25' --args ./build-ui-tests/top_ui04h_tests
```

`Clock skew detected` refere-se a timestamps de arquivos, não comprova erro de compilação; `touch` corrige datas futuras dos arquivos.
