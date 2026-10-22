#include "game_src/unresolved/race_callback_registry.hpp"
#include <cassert>
#include <iostream>
using namespace roadwars;
int main() {
    using Domain = RaceCallbackKey::Domain;
    const RaceCallbackKey a{Domain::vehicle, 0, 1}, b{Domain::vehicle, 0, 2},
        c{Domain::vehicle, 0, 3}, d{Domain::vehicle, 0, 4};
    RaceCallbackRegistry list;
    std::string error;
    std::vector<std::size_t> trace;
    const auto setup = [&] {
        list.reset();
        list.insert(a);
        list.insert(b);
        list.insert(c);
        trace.clear();
    };
    setup();
    assert(list.dispatch(
               [&](auto key) {
                   trace.push_back(key.token);
                   return Result::ok;
               },
               error) == Result::ok);
    assert(trace == std::vector<std::size_t>({1, 2, 3}));
    setup();
    assert(list.dispatch(
               [&](auto key) {
                   trace.push_back(key.token);
                   if (key == a)
                       list.insert(d);
                   return Result::ok;
               },
               error) == Result::ok);
    assert(trace == std::vector<std::size_t>({1, 4, 4}) && list.size() == 4);
    setup();
    assert(list.dispatch(
               [&](auto key) {
                   trace.push_back(key.token);
                   if (key == a)
                       list.remove(a);
                   return Result::ok;
               },
               error) == Result::ok);
    assert(trace == std::vector<std::size_t>({1, 3, 3}) && list.size() == 2);
    setup();
    assert(list.dispatch(
               [&](auto key) {
                   trace.push_back(key.token);
                   if (key == a)
                       list.insert(b);
                   return Result::ok;
               },
               error) == Result::ok);
    assert(trace == std::vector<std::size_t>({1, 3, 3}) && list.size() == 3);
    setup();
    assert(list.dispatch(
               [&](auto key) {
                   trace.push_back(key.token);
                   if (key == a)
                       list.remove(d);
                   return Result::ok;
               },
               error) == Result::ok);
    assert(trace == std::vector<std::size_t>({1, 3, 3}));
    setup();
    assert(list.dispatch(
               [&](auto) {
                   list.remove(a);
                   list.remove(b);
                   list.remove(c);
                   return Result::ok;
               },
               error) == Result::unavailable);
    list.reset();
    assert(list.dispatch([](auto) { return Result::invalid_argument; }, error) == Result::ok);
    std::cout << "Original shared callback cursor contracts passed\n";
}
