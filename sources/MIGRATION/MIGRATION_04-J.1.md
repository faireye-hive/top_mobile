# Tales of Pirates — Migração 04-J.1: correção de `std::bad_alloc` no HUD

## Diagnóstico

No Ubuntu, o `top_ui04h_tests` encerrava com `std::bad_alloc`. O GDB mostrou:

```
std::basic_string::reserve
std::operator+(std::string const&, char const*)
top::ui::HudComposer::layer(...)
drawSkills(...)
main(...)
```

A classe `HudComposer` guardava `const std::string& root_`, mas os callers podiam passar
um literal (`"Client"`) convertido em `std::string` temporária. A referência ficava
pendurada após o construtor; qualquer concatenação em `layer()` passava a acessar
memória inválida. Isso é uso após fim de vida (*dangling reference*), e pode causar
`bad_alloc`, falhas aleatórias ou caminhos de textura inválidos.

## Alterações

- `Engine/ui/HudComposer.h`: troca `const std::string& root_` por `std::string root_`;
  a classe passa a possuir uma cópia de seu caminho-base.
- `tests/UI04HTests.cpp`: dois testes de regressão comprovam que o compositor
  mantém o caminho correto após a alteração da string original e ao receber
  uma `std::string` temporária.
- O restante da lógica de UI, gráficos 3D, mapas, combate e rede não foi alterado.

## Reprodução e validação

Os novos testes foram executados sobre a versão anterior: um deles falhou por
produzir um caminho de textura incorreto. Depois da correção, `top_ui04h_tests`
passou e todas as 6 suítes de regressão (04-D a 04-I) passaram neste ambiente.
O teste 04-H também passou com AddressSanitizer e UndefinedBehaviorSanitizer.

```bash
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests --target top_ui04h_tests -j4
ctest --test-dir build-ui-tests -R ui04h_layered_visibility --output-on-failure
# Todas as suites:
cmake --build build-ui-tests --target top_ui04d_tests top_ui04e_tests top_ui04f_tests top_ui04g_tests top_ui04h_tests top_ui04i_tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Para o jogo (recomendado limpar o build em caso de arquivos de migrações antigas):

```bash
rm -rf build
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

A execução OpenGL do cliente não foi validada aqui. A correção deste pacote é
localizada na gestão do tempo de vida de uma string em `HudComposer`.
