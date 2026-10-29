#include "top/MapFile.h"
#include "top/TerrainMesh.h"
#include "top/HistoricalRange2Ground.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

void require(bool value, const char* message) {
    // Deliberadamente ativo em Release/NDEBUG tambem.
    if (!value) throw std::runtime_error(message);
}

bool near(float a, float b) { return std::fabs(a - b) < .0001f; }

struct Fixture {
    std::string path;
    Fixture() {
        char name[] = "/tmp/top_sea_terrain_XXXXXX";
        const int fd = ::mkstemp(name);
        require(fd >= 0, "criar arquivo sintetico");
        ::close(fd);
        path = name;
    }
    ~Fixture() { std::remove(path.c_str()); }
    void save(const std::vector<std::uint8_t>& bytes) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        require(out.good(), "gravar arquivo sintetico");
    }
};

void append32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

std::vector<std::uint8_t> header(int width, int height, const std::vector<std::uint32_t>& offsets) {
    std::vector<std::uint8_t> bytes;
    for (std::uint32_t value : {780627u, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 8u, 8u}) {
        append32(bytes, value);
    }
    for (std::uint32_t offset : offsets) append32(bytes, offset);
    return bytes;
}

void addCoast(std::vector<std::uint8_t>& bytes) {
    // SNewFileTile: 15 bytes, altura int8/10, LAND=1 e quatro bloqueios.
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            std::vector<std::uint8_t> tile(15, 0);
            tile[4] = 5;
            tile[5] = tile[6] = 255;
            tile[7] = static_cast<std::uint8_t>(x < 4 ? 10 : -10);
            tile[8] = x < 4 ? 1 : 0;
            if (x == 4 && y == 2) tile[11] = 128;
            // Limiar original de alpha: exatamente -0.5 m ainda e opaco.
            if (x == 4 && y == 0) tile[7] = static_cast<std::uint8_t>(-5);
            bytes.insert(bytes.end(), tile.begin(), tile.end());
        }
    }
}

const top::TerrainBatch* batch(const top::TerrainMesh& mesh, int mode, int tex = 0) {
    for (const auto& entry : mesh.batches) {
        if (entry.mode == mode && (mode == 7 || entry.texId == tex)) return &entry;
    }
    return nullptr;
}

void checkDefault(const top::Tile& tile) {
    // MPTile::Init + MPMap construtor, sem inventar regiao ou bloqueio.
    require(tile.valid && near(tile.height, -2.f), "secao vazia deve ter fundo a -2 m");
    require(tile.layer[0].tex == 22 && tile.layer[0].alpha == 15, "textura submarina historica 22/15");
    require(tile.layer[1].tex == 255 && tile.layer[1].alpha == 0, "marcador historico de tile padrao sem overlay");
    require(tile.r == 255 && tile.g == 255 && tile.b == 255, "fundo padrao branco");
    require(tile.region == 0 && tile.island == 0 && !tile.blocked, "defaults historicos de regiao/ilha/bloqueio");
}

void testEmptySea() {
    Fixture fixture;
    fixture.save(header(16, 16, {0, 0, 0, 0}));
    top::MapFile map;
    require(map.open(fixture.path), "abrir mapa de mar vazio");
    checkDefault(map.at(8, 8));
    checkDefault(map.at(8, 8)); // Cache preserva a distincao Empty/Invalid.
    require(near(map.heightAt(8.25f, 8.75f), -2.f), "interpolar fundo submarino vazio");
    for (float x : {8.25f, 8.75f}) {
        for (float y : {8.25f, 8.75f}) {
            require(map.sailableAt(x, y), "quatro subtiles vazios navegaveis");
            require(map.blockedAt(x, y), "personagem em terra nao anda no mar vazio");
        }
    }
    top::TerrainMesh mesh;
    top::buildTerrainMesh(map, 4, 4, 8, mesh);
    const auto* floor = batch(mesh, 1, 22);
    require(floor && floor->verts.size() == 8 * 8 * 6, "malha de fundo submarino em toda secao vazia");
    for (const auto& v : floor->verts) require(near(v.y, -2.f), "altura de cada vertice do fundo");
    const auto* water = batch(mesh, 7);
    require(water && water->verts.size() == 4 * 6, "mar em quatro quadras de 4 m");
    for (const auto& v : water->verts) {
        require(near(v.y, 0.f) && near(v.a, 207.f / 255.f), "altura e alpha de mar aberto historicos");
        require(near(v.r, 140.f / 255.f) && near(v.g, 140.f / 255.f) && near(v.b, 220.f / 255.f), "cor do mar historica");
        require((near(v.u, 0.f) || near(v.u, 1.f)) && (near(v.v, 0.f) || near(v.v, 1.f)), "UV repete quadro por quadra");
    }
    require(!batch(mesh, 2, 255), "marcador de default nao vira overlay");
    top::buildTerrainMesh(map, -4, -4, 8, mesh);
    water = batch(mesh, 7);
    require(water && water->verts.size() == 6, "origens de quadras fora do mapa sao ignoradas");
    map.close();
}

void testCoastAndNavigation() {
    Fixture fixture;
    auto bytes = header(16, 8, {28, 0});
    addCoast(bytes);
    fixture.save(bytes);
    top::MapFile map;
    require(map.open(fixture.path), "abrir costa sintetica");
    const auto land = map.at(1, 1);
    require(land.valid && land.layer[0].tex == 5 && near(land.height, 1.f), "terreno presente preservado");
    require(!map.blockedAt(1.25f, 1.25f) && !map.sailableAt(1.25f, 1.25f), "LAND caminhavel e nao navegavel");
    require(map.at(4, 2).blocked, "bit de bloqueio agregado preservado");
    require(!map.sailableAt(4.25f, 2.25f) && map.sailableAt(4.75f, 2.25f), "bloqueio continua por subtile no mar presente");
    checkDefault(map.at(8, 2));
    require(map.sailableAt(8.25f, 2.25f), "barco atravessa limite presente/vazio");
    require(near(map.heightAt(7.5f, 2.25f), -1.5f), "interpolacao entre secao presente e fundo padrao");
    top::TerrainMesh mesh;
    top::buildTerrainMesh(map, 0, 0, 8, mesh);
    const auto* terrain = batch(mesh, 1, 5);
    require(terrain && terrain->verts.size() == 8 * 8 * 6, "malha de terreno presente preservada");
    const auto* water = batch(mesh, 7);
    require(water, "mar ativado na costa");
    bool dry = false, wet = false, emptyBoundary = false, threshold = false;
    for (const auto& v : water->verts) {
        if (near(v.x, 0.f) && near(v.z, 0.f)) dry = near(v.a, 0.f);
        if (near(v.x, 4.f) && near(v.z, 4.f)) wet = near(v.a, 207.f / 255.f);
        if (near(v.x, 8.f) && near(v.z, 4.f)) emptyBoundary = near(v.a, 207.f / 255.f);
        if (near(v.x, 4.f) && near(v.z, 0.f)) threshold = near(v.a, 207.f / 255.f);
    }
    require(dry && wet && threshold, "alpha da costa conserva limiar historico de 50 cm");
    require(emptyBoundary, "amostra de secao vazia nao abre buraco no mar da costa");
    map.close();
}

void testLimitsAndPartialSections() {
    Fixture fixture;
    fixture.save(header(10, 9, {0, 0, 0, 0}));
    top::MapFile map;
    require(map.open(fixture.path), "abrir mapa com secao parcial");
    checkDefault(map.at(9, 8));
    for (const auto& pos : std::vector<std::pair<float, float>>{{-.01f, 0.f}, {0.f, -.01f}, {10.f, 8.f}, {9.f, 9.f}, {1e30f, 0.f}, {std::numeric_limits<float>::infinity(), 0.f}, {std::numeric_limits<float>::quiet_NaN(), 0.f}}) {
        require(!map.sailableAt(pos.first, pos.second), "fora do mapa/non-finite nao navegavel");
        require(map.blockedAt(pos.first, pos.second), "fora do mapa/non-finite bloqueado");
        require(near(map.heightAt(pos.first, pos.second), 0.f), "altura invalida nao e fundo de mar");
        require(map.regionAt(pos.first, pos.second) == 0, "regiao invalida segura");
    }
    require(!map.at(-1, 0).valid && !map.at(10, 8).valid && !map.at(9, 9).valid, "limites de at permanecem invalidos");
    top::TerrainMesh mesh;
    top::buildTerrainMesh(map, 8, 8, 4, mesh);
    const auto* floor = batch(mesh, 1, 22);
    require(floor && floor->verts.size() == 2 * 6, "secao parcial so emite tiles internos");
    const auto* water = batch(mesh, 7);
    require(water, "secao parcial ainda tem mar");
    bool sawOutside = false;
    for (const auto& v : water->verts) {
        if (v.x >= 10.f || v.z >= 9.f) {
            sawOutside = true;
            require(near(v.a, 207.f / 255.f),
                    "MPMap::GetTile devolve seabed padrao -2 m fora do mapa, sem furo no alpha");
        }
    }
    require(sawOutside, "fixture de secao parcial precisa cruzar a borda");
    map.close();
    require(!map.at(0, 0).valid && !map.sailableAt(0.f, 0.f), "mapa fechado nao vira oceano");
}

void testRange2SubgridHeight() {
    Fixture fixture;
    auto bytes = header(16, 8, {28, 0});
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            std::vector<std::uint8_t> tile(15, 0);
            if (x == 1 && y == 2) tile[11] = 20;       // +1.00 m
            if (x == 2 && y == 2) tile[11] = 64 | 10;  // -0.50 m
            if (x == 3 && y == 2) tile[11] = 128 | 30;// +1.50 m and blocked
            bytes.insert(bytes.end(), tile.begin(), tile.end());
        }
    }
    fixture.save(bytes);
    top::MapFile map;
    require(map.open(fixture.path), "abrir mapa sintetico RANGE2");
    require(near(map.range2GroundHeightAt(1, 2), 1.f), "btBlock low six bits decode 5 cm levels");
    require(near(map.range2GroundHeightAt(2, 2), -.5f), "btBlock sign bit decodes negative height");
    require(near(map.range2GroundHeightAt(3, 2), 1.5f), "blocked bit does not change encoded height");
    require(near(map.range2GroundHeightAt(8, 2), 0.f), "empty section has default subgrid height zero");
    require(near(map.range2GroundHeightAt(-1, 2), 0.f), "out of map subgrid sample is zero");

    unsetenv("TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT");
    require(near(top::historical_range2_ground::sample(map, 1, 2), 1.f),
            "normal RANGE2 sampler uses the authored subgrid");
    setenv("TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT", "1", 1);
    require(near(top::historical_range2_ground::sample(map, 1, 2), 1.f),
            "opt-in RANGE2 sampler selects original subgrid height");
    setenv("TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT", "0", 1);
    require(near(top::historical_range2_ground::sample(map, 1, 2), 1.f),
            "retired variable cannot replace the original subgrid");
    unsetenv("TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT");
    map.close();
}

void testShadeTileAndSubgridHeight() {
    Fixture fixture;
    auto bytes = header(16, 8, {28, 0});
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            std::vector<std::uint8_t> tile(15, 0);
            tile[7] = 10; // cHeight = +1 m
            if (x == 2 && y == 2) {
                tile[11] = 40;        // q0 = +2 m, unblocked
                tile[12] = 128 | 50;  // q1 = +2.5 m, blocked
                tile[13] = 64 | 20;   // q2 = -1 m
                tile[14] = 30;        // q3 = +1.5 m
            }
            if (x == 3 && y == 2) tile[7] = 5; // +0.5 m, edge's tile height
            bytes.insert(bytes.end(), tile.begin(), tile.end());
        }
    }
    fixture.save(bytes);
    top::MapFile map;
    require(map.open(fixture.path), "abrir mapa sintetico SHADE");
    require(near(map.shadeGroundHeightAt(2.f, 2.f, false, false), 2.f),
            "SHADE usa maior altura de quadrante desobstruido");
    require(near(map.shadeGroundHeightAt(2.5f, 2.f, false, false), 1.f),
            "quadrante bloqueado mantem altura base do tile");
    require(near(map.shadeGroundHeightAt(2.f, 2.5f, false, false), 1.f),
            "quadrante abaixo do tile mantem altura base");
    require(near(map.shadeGroundHeightAt(2.5f, 2.5f, false, false), 1.5f),
            "quadrante diagonal acima do tile e usado");
    require(near(map.shadeGroundHeightAt(3.f, 2.f, true, false), .5f),
            "borda direita amostra quadrante anterior mas conserva tile atual");
    require(near(map.shadeGroundHeightAt(2.f, 3.f, false, true), 1.f),
            "borda inferior desloca a amostra de subgrade");
    require(near(map.shadeGroundHeightAt(8.f, 2.f, false, false), 0.f),
            "secao vazia usa subgrade zero sobre tile submarino");
    require(near(map.shadeGroundHeightAt(-1.f, 2.f, false, false), 0.f),
            "coordenada externa mantem fallback zero do mapa DX9");
    map.close();
}

void testCorruptAndReadFailure() {
    Fixture fixture;
    top::MapFile map;
    top::TerrainMesh mesh;
    // Offset nao zero sem bloco completo nunca equivale a secao vazia.
    auto truncated = header(8, 8, {24});
    truncated.push_back(0);
    fixture.save(truncated);
    require(map.open(fixture.path), "cabecalho com bloco truncado e inspecionavel");
    require(!map.at(0, 0).valid && !map.sailableAt(.25f, .25f) && map.blockedAt(.25f, .25f), "bloco truncado fechado a navegacao");
    top::buildTerrainMesh(map, 0, 0, 8, mesh);
    require(mesh.batches.empty(), "bloco truncado nao desenha fundo ou mar");
    map.close();
    auto intoHeader = header(8, 8, {4});
    intoHeader.resize(24 + 8 * 8 * 15, 0);
    fixture.save(intoHeader);
    require(map.open(fixture.path) && !map.at(0, 0).valid && !map.sailableAt(.25f, .25f), "offset para cabecalho e invalido");
    map.close();
    fixture.save(header(16, 8, {0, 0xffffffffu}));
    require(map.open(fixture.path), "mapa misto com offset corrompido");
    require(map.sailableAt(.25f, .25f) && !map.sailableAt(8.25f, .25f), "vazio legitimo e erro permanecem distintos no mesmo mapa");
    top::buildTerrainMesh(map, 0, 0, 16, mesh);
    const auto* water = batch(mesh, 7);
    require(water, "secao vazia saudavel ainda desenha mar");
    bool corruptCorner = false;
    for (const auto& v : water->verts) {
        if (near(v.x, 8.f) && near(v.z, 0.f)) { corruptCorner = true; require(near(v.a, 0.f), "amostra corrompida transparente"); }
    }
    require(corruptCorner, "amostra de erro realmente verificada");
    map.close();
    // Bloco longe do buffer inicial de stdio: provoca fread curto depois de open.
    auto disappears = header(8, 8, {8192});
    disappears.resize(8192, 0);
    addCoast(disappears);
    fixture.save(disappears);
    require(map.open(fixture.path), "abrir mapa antes de falha de leitura");
    require(::truncate(fixture.path.c_str(), 8192) == 0, "simular truncamento apos open");
    require(!map.at(0, 0).valid && !map.at(0, 0).valid && !map.sailableAt(.25f, .25f), "fread curto e cache de erro nao liberam mar");
    map.close();
    fixture.save(header(16, 16, {0}));
    require(!map.open(fixture.path) && !map.ok() && map.width() == 0, "tabela truncada rejeitada e estado limpo");
    fixture.save(header(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), {}));
    require(!map.open(fixture.path), "dimensoes corrompidas nao causam overflow/alocacao gigante");
}

} // namespace

int main() {
    try {
        testEmptySea();
        testCoastAndNavigation();
        testLimitsAndPartialSections();
        testCorruptAndReadFailure();
        testRange2SubgridHeight();
        testShadeTileAndSubgridHeight();
        std::cout << "Sea terrain parity: empty sea, coast, bounds and corrupt/read-failure cases OK\n";
    } catch (const std::exception& e) {
        std::cerr << "Sea terrain parity failed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
