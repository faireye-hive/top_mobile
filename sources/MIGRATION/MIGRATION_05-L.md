# 05-L - malhas externas de efeitos MindPower (.lgo)

- A implementacao original (`CMPResManger::LoadTotalMesh`) localiza geometria adicional em `Client/model/effect/*.lgo`.
- Reutilizamos `Model::loadGeom()` (mesmo leitor de .lgo dos personagens/objetos) em vez de criar um decodificador paralelo.
- O adaptador `LegacyExternalEffectMesh.h` limita modelos a uma geometria nao-skinada, ate 8192 vertices, 36000 indices e 16 subsets, validando nome, extensao, indices, UV e valores finitos.
- Cada subset usa sua textura de material em `Client/texture/effect`; quando esta ausente, tenta a textura do efeito e seus quadros animados. Sem textura, nao inventa textura.
- Matrizes locais e animacoes de matriz sao amostradas no modelo; keyframes de `.eff` e conversao MindPower Z-up para OpenGL Y-up compartilham `legacy_mesh::emit` da 05-K.
- Modelos desconhecidos, ausentes, invalidos, com skin ou acima dos limites nao sao desenhados; o fallback 05-G continua funcionando. O sistema nao considera um asset convertido ate conseguir carregar geometria real.
- Esta etapa NAO cobre modelos animados por ossos, hierarquias complexas, transparencias por subset ou todas as variacoes de material e shader DX9. O teste usa um arquivo .lgo sintetico no formato conhecido, nao os `.lgo` reais da instalacao do usuario.
- Nao modifica inventario, interface, rede, armas nem a logica do jogo offline.

## Verificacao Linux

```bash
cmake -S sources -B build && cmake --build build -j4
./build/top_gl --check
./build/top_gl
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
```

Espera-se `ui05l_external_effect_lgo` na lista; `top_gl` completo requer headers GL/X11 do Linux.
