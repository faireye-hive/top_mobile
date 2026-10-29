#pragma once
#include <string>
namespace top {
// Retorna o caminho fisico correspondente ao nome historico da textura.
// Usa Client/texture/texture_manifest.csv quando disponivel.
// Retorna vazio se nao encontra o recurso.
std::string resolveTexturePath(const std::string& historicalPath);
}
