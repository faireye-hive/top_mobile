# Migração 05-J — texturas animadas dos efeitos e fechamento correto da janela X11

Base: 05-I. Cliente C++17/OpenGL offline; sem alterações de protocolo/rede ou componentes de interface.

## Implementado

- Leitura **estruturada e limitada** dos quadros de `CTexCoordList` (coordenadas UV) e `CTexList` (regiões de textura), presentes em `I_Effect::LoadFromFile()` do código histórico.
- Leitura do período de animação de `CTexFrame`; os nomes de arquivos já lidos na 05-I passam a ser resolvidos no carregamento do recurso, com seleção por tempo da instância.
- Renderização das UVs originais para **billboards retangulares com quatro vértices**, respeitando a ordem original do `RectPlane`. Para outros formatos, mantém o fallback e não inventa topologia.
- Interpolação dos quadros de UVs e seleção discreta das regiões do atlas, conforme as rotinas históricas `CTexCoordList::GetCurCoord` e `CTexList::GetCurTexture`.
- Se uma textura animada estiver ausente, o billboard mantém a textura base resolvida em vez de desaparecer.
- `top_eff_inspect` exibe contagens de quadros UV, regiões e texturas animadas por subefeito.
- Linux X11: o `GlxWindow::open()` registra `WM_DELETE_WINDOW` com `XSetWMProtocols`, e o `pump()` só aceita a notificação correspondente. `DestroyNotify` é tratado; a janela não é destruída duas vezes. O loop de renderização sai imediatamente quando o fechamento é solicitado.

## Limitações deliberadas

- As curvas UV são aplicadas somente aos billboards de **quatro vértices**. Não foram portadas malhas DirectX arbitrárias, animação de vértices, cilindros, shaders ou todos os modos de mistura do motor original.
- `Client/effect/hpushf04.eff` contém um RectPlane reconhecido na 05-I; a sequência de arquivos de textura só ficará visualmente evidente em efeitos que efetivamente usam `CTexFrame` e possuem todos os arquivos. Os testes 05-J criam amostras binárias com esse layout, mas não substituem testes visuais com assets reais.
- O `XIO: fatal IO error` provocado por um gerenciador de janelas que **mata o cliente X11** normalmente é evitado ao registrar `WM_DELETE_WINDOW`. Isso não evita interrupções externas do servidor X11 (reinício, encerramento de sessão etc.).
- A compilação integral OpenGL/GLX deverá ser validada no Linux, já que este ambiente não possui `GL/gl.h`/`GL/glx.h`.

## Instalação (raiz com `sources/` e `Client/`)

```sh
rm -rf build build-ui-tests
cmake -S sources -B build
cmake --build build -j4
./build/top_gl --check
./build/top_gl
```

Verifique o fechamento clicando no **X** da janela. Espera-se um retorno normal ao terminal, sem aviso fatal de XIO se o servidor X11 continuar ativo.

Testes:

```sh
cmake -S sources -B build-ui-tests -DTOP_BUILD_UI_TESTS=ON
cmake --build build-ui-tests -j4
ctest --test-dir build-ui-tests --output-on-failure
./build/top_eff_inspect Client/effect/hpushf04.eff
```

O teste `ui05j_uv_atlas_and_texture_frames` valida interpolação, seleção de atlas e arquivo de textura, arquivo ausente, e binário truncado. A regressão atual: 15/15 testes aprovados neste ambiente.

## Próximas etapas

Decodificar geometria de modelos `.eff` não-billboard a partir dos formatos `CEffectModel`/`MPModelEff` e validar com arquivos de efeitos reais. Manter a rede opcional para o final.
