// 05-AK.2: teste headless do cronometro monotono de inicializacao e CSV.
#include "top/StartupProfiler.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

int main() {
    constexpr const char* kReport = "ui05ak2_startup_test.csv";
    {
        top::startup::Profiler disabled;
        disabled.mark("nao deve ser gravado");
        assert(disabled.stages().empty());
    }
    {
        top::startup::Profiler p;
        p.enable(kReport);
        assert(p.enabled());
        p.mark("inicio");
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        p.mark("etapa \"com aspas\"");
        assert(p.stages().size() == 2);
        assert(p.stages()[1].deltaMs >= 0.0);
        assert(p.stages()[1].totalMs >= p.stages()[0].totalMs);
        assert(p.stages()[1].deltaMs >= 1.0);
        p.finish();
        p.finish();
        p.mark("nao deve aparecer apos finish");
        assert(p.stages().size() == 2);
    }
    std::ifstream file(kReport);
    assert(file.good());
    std::string line;
    assert(static_cast<bool>(std::getline(file, line)));
    assert(line == "etapa,delta_ms,total_ms");
    assert(static_cast<bool>(std::getline(file, line)));
    assert(line.find("\"inicio\",") == 0);
    assert(static_cast<bool>(std::getline(file, line)));
    assert(line.find("\"etapa \"\"com aspas\"\"\",") == 0);
    assert(!static_cast<bool>(std::getline(file, line)));
    file.close();
    std::remove(kReport);
    return 0;
}
