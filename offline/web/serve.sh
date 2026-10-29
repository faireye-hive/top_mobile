#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ ! -f "$project_root/dist-web/index.html" ]]; then
    echo "Compile primeiro: bash offline/web/build.sh" >&2
    exit 1
fi
echo "Abra http://localhost:${1:-8080} no navegador. Ctrl+C encerra o servidor."
python3 -m http.server "${1:-8080}" --bind 127.0.0.1 --directory "$project_root/dist-web"
