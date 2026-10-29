# SK2.7 — tempos de quadros-chave `.eff` (DX9 I_Effect)

Esta etapa corrige uma diferença comprovada entre `MPModelEff.cpp::FrameMove`
(original) e o amostrador de `LegacyEffFormat.h` (OpenGL).

## Alterações

- `Key::time` é a **duração do quadro**, não um timestamp absoluto.
- O trecho `quadro[i] -> quadro[i+1]` usa a duração de `quadro[i]`.
- O último quadro interpola para o primeiro até sua duração terminar; em seguida
  o subefeito deixa de desenhar. Isso é aplicado aos billboards, primitivas
  nativas, modelos `.lgo` e modelos `.eff` aninhados dentro de `.par`.
- Os parâmetros animados de Cone/Cylinder e demais modelos usam **a mesma fase**.
- Quadros com duração zero são pulados sem divisão por zero.
- UVs e frames de textura mantêm suas **próprias** durações e mecanismos;
  não são alterados por esta migração.
- Não foram modificados blends, modos de cor, posição, alcance, escala,
  combate ou duração da animação do personagem.

## Instalação

Aplique o patch na raiz do projeto **depois de SK2.6.1**:

```bash
git apply --check tales_05_SK2_7_timeline_eff_duracoes_DX9_RAIZ.patch
git apply tales_05_SK2_7_timeline_eff_duracoes_DX9_RAIZ.patch
cmake -S sources -B build -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Comparação A/B

O **modo DX9 de durações** é o padrão:

```bash
TOP_HISTORICAL_BLEND=1 TOP_TRACE_SKILL_FX=1 ./build/top_gl --profile 2>&1 | tee sk27-dx9.log
```

Reinicie o processo usando o amostrador antigo, apenas para comparar:

```bash
TOP_EFF_TIME_MODE=absolute TOP_HISTORICAL_BLEND=1 TOP_TRACE_SKILL_FX=1 ./build/top_gl --profile 2>&1 | tee sk27-antigo.log
```

Aparecerá no início: `[eff-timeline] mode=duration-dx9` ou
`[eff-timeline] mode=absolute-legacy-gl`.

## O que esperar — sem prometer fidelidade visual completa

O `04000009.eff` (voo Illusion Slash) possui durações `0.9, 1.8, 0.9`
(3,6 s) e **tamanho constante 2×2×2**. Logo a SK2.7 **não faz a meia-lua
crescer por si só**: se o DX9 a amplia durante o deslocamento, a causa está
na transformação/escala do controlador, projeção ou eventos de animação.

No `01010003.eff` (parte dos modelos do impacto), as três trilhas duram
respectivamente `0.58`, `0.48`, `0.27` s e usam chaves de tamanho/alpha que
variam no tempo. Essas animações agora respeitam durações sucessivas e param
após sua sequência.

Ainda existem multiplicadores calibrados anteriormente (voo do Illusion Slash
2,75×; impacto 5×); **não foram removidos**, para não misturar modificações de
escala com a correção de tempo. A fidelidade absoluta ainda requer comparar
capturas de quadros com o DX9, câmera e momento equivalentes.

O DX9 original descarta o excesso de tempo ao avançar um quadro, enquanto
esta implementação amostra uma timeline cumulativa usando segundos contínuos;
isso evita dependência da taxa de quadros, mas não reproduz exatamente o
comportamento em FPS muito baixo.
