#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_root"
code_only=0
if [[ "${1:-}" == "--code-only" ]]; then code_only=1; shift; fi
if ! command -v emcmake >/dev/null 2>&1 && [[ -f .local-tools/emsdk/emsdk_env.sh ]]; then
    source .local-tools/emsdk/emsdk_env.sh
fi
if ! command -v emcmake >/dev/null 2>&1; then
    echo "Ative o Emscripten SDK antes de compilar. Consulte offline/web/README.md." >&2
    exit 1
fi
emcmake cmake -S sources -B build-web -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build-web --target top_web -j"${TOP_BUILD_JOBS:-2}"
web_output="${TOP_WEB_OUTPUT:-dist-web}"
if [[ -e "$web_output/.git" ]]; then
    echo "Escolha uma pasta de pacote, fora da raiz de um repositorio Git." >&2
    exit 1
fi
if [[ -e "$web_output" && (! -f "$web_output/index.html" || ! -f "$web_output/assets-index.json") ]]; then
    echo "A pasta de saida existente nao e um pacote web reconhecido: $web_output" >&2
    exit 1
fi
copy_runtime() {
    local web_destination="$1"
    cp build-web/index.html build-web/index.js build-web/index.wasm "$web_destination/"
    for web_file in asset-store.js mobile-ui.js app.css manifest.webmanifest icon.svg; do
        cp "sources/Platform/web/$web_file" "$web_destination/"
    done
    if [[ -f build-web/font-license.txt ]]; then cp build-web/font-license.txt "$web_destination/"; fi
    cp -R offline/web/licenses "$web_destination/"
    mkdir -p "$web_destination/.github/workflows"
    cp offline/web/github-pages.yml "$web_destination/.github/workflows/pages.yml"
}
if [[ "$code_only" == 1 ]]; then
    if [[ ! -f "$web_output/assets-index.json.gz" ]]; then
        echo "Gere o pacote completo uma vez antes de usar --code-only." >&2
        exit 1
    fi
    copy_runtime "$web_output"
    echo "Codigo atualizado em $web_output/. Os recursos existentes foram reaproveitados."
    exit 0
fi
mkdir -p .local-tools
web_stage="$(mktemp -d .local-tools/web-stage.XXXXXX)"
trap 'if [[ -n "${web_stage:-}" ]]; then rm -rf -- "$web_stage"; fi' EXIT
python3 sources/Tools/package_web_assets.py --client Client --output "$web_stage" --compact
copy_runtime "$web_stage"
if [[ -e "$web_output" ]]; then
    web_previous=".local-tools/previous-web-output"
    if [[ -e "$web_previous" ]]; then
        if [[ ! -f "$web_previous/assets-index.json" ]]; then
            echo "Backup anterior nao reconhecido: $web_previous" >&2
            exit 1
        fi
        rm -rf -- "$web_previous"
    fi
    mv -- "$web_output" "$web_previous"
fi
if ! mv -- "$web_stage" "$web_output"; then
    if [[ -n "${web_previous:-}" ]]; then mv -- "$web_previous" "$web_output"; fi
    exit 1
fi
web_stage=""
echo "Versao web pronta em $web_output/. Para abrir: bash offline/web/serve.sh"
