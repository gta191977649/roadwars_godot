#include "game_src/unresolved/skid_marks.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace roadwars;
void require(bool value) {
    if (!value) {
        std::cerr << "Skid strip fixture failed\n";
        std::exit(1);
    }
}
int main() {
    SkidMarkEmitter marks;
    require(marks.configure(1, .25f) == Result::ok);
    TireParticleSource source;
    source.skid_enabled = true;
    source.skid_color_argb = 0xff192837;
    source.position = {0, .1f, 0};
    require(marks.advance(.01f, source) == Result::ok);
    source.position.z = 2;
    require(marks.advance(.05f, source) == Result::ok);
    const auto& live = marks.strips()[0];
    require(live.vertices.size() == 4 && live.committed_count == 2);
    require(std::abs(live.vertices[0].position.x - .25f) < .0001f);
    require(live.vertices[0].position.y == .1f && live.vertices[0].color_argb == 0xff192837);
    require(live.vertices[3].uv[1] == 1);
    source.position.z = 4;
    marks.advance(.08f, source);
    require(marks.strips()[0].vertices.size() == 6 && marks.strips()[0].vertices.back().uv[1] == 2);
    source.skid_enabled = false;
    const auto revision = live.revision;
    marks.advance(100, source);
    require(live.revision == revision && live.vertices.size() == 6);
    for (int session = 0; session < 5; ++session) {
        source.skid_enabled = true;
        marks.advance(.01f, source);
        source.position.z += 1;
        marks.advance(.02f, source);
        source.skid_enabled = false;
        marks.advance(.01f, source);
    }
    require(marks.strips()[0].vertices.size() == 4);
    source.skid_enabled = true;
    for (int sample = 0; sample < 100; ++sample) {
        source.position.z += 1;
        marks.advance(.13f, source);
    }
    for (const auto& strip : marks.strips())
        require(strip.vertices.size() <= 32);
    marks.reset();
    for (const auto& strip : marks.strips())
        require(strip.vertices.empty());
    require(marks.configure(0, .25f) == Result::invalid_argument);
    // D3DRMVectorRotate normalizes its result: strip width must not grow with speed
    // or depend on how much of the sample interval has elapsed at the live endpoint.
    for (const float displacement : {.01f, .1f, 1.0f, 10.0f}) {
        marks.configure(1, .25f);
        source.skid_enabled = true;
        source.position = {0, 0, 0};
        marks.advance(0, source);
        const auto& initial = marks.strips()[0].vertices;
        require(initial[0].position.x == -.25f && initial[1].position.x == .25f);
        source.position = {0, 0, displacement};
        marks.advance(.05f, source);
        const auto& vertices = marks.strips()[0].vertices;
        const auto a = vertices[2].position, b = vertices[3].position;
        require(std::abs(std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)) - .5f) <
                .00001f);
    }
    // Full 3D displacement is normalized before the caller discards the offset Y.
    marks.configure(1, .25f);
    source.position = {0, 0, 0};
    marks.advance(0, source);
    source.position = {0, 3, 4};
    marks.advance(.05f, source);
    const auto& slope = marks.strips()[0].vertices;
    require(std::abs(std::abs(slope[3].position.x - slope[2].position.x) - .4f) < .0001f);
    require(slope[2].position.y == 3 && slope[3].position.y == 3);
    // Fifteen committed points fit 32 strip vertices; the sixteenth wraps safely.
    marks.configure(1, .25f);
    source.position = {0, 0, 0};
    marks.advance(0, source);
    for (int point = 1; point < 15; ++point) {
        source.position.z = static_cast<float>(point * 10);
        marks.advance(.13f, source);
    }
    require(marks.strips()[0].vertices.size() == 32);
    require(marks.strips()[0].vertices.back().uv[1] == 15);
    for (std::size_t pair = 0; pair < 32; pair += 2) {
        const auto a = marks.strips()[0].vertices[pair].position;
        const auto b = marks.strips()[0].vertices[pair + 1].position;
        require(std::abs(std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)) - .5f) <
                .00001f);
    }
    source.position.z = 150;
    marks.advance(.13f, source);
    require(marks.strips()[0].vertices.size() == 4 && marks.strips()[0].vertices.back().uv[1] == 1);
}
