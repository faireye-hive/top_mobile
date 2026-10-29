#!/usr/bin/env bash
set -u

root="$(cd "$(dirname "$0")/../.." && pwd)"
bin="$root/build/top_gl"
if [ ! -x "$bin" ]; then
  echo "top_gl nao encontrado: execute o build primeiro" >&2
  exit 2
fi

failed=0
total=0
for path in "$root"/Client/map/*.map; do
  [ -f "$path" ] || continue
  name="${path##*/}"
  name="${name%.map}"
  total=$((total + 1))
  if ! "$bin" --check --map "$name" >"/tmp/top-check-$name.log" 2>&1; then
    echo "FAIL $name"
    tail -n 8 "/tmp/top-check-$name.log"
    failed=$((failed + 1))
  fi
done

echo "mapas verificados: $total  falhas: $failed"
[ "$failed" -eq 0 ]
