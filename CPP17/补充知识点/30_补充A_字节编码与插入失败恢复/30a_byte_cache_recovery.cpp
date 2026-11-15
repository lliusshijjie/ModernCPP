// 补充 30A：显式字节编码、try_emplace 的求值边界与节点冲突恢复。
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using Bytes = std::array<std::byte, 2>;

constexpr Bytes encode(std::uint16_t value) {
    return {std::byte{static_cast<unsigned char>(value >> 8)},
            std::byte{static_cast<unsigned char>(value & 0xFFU)}};
}

constexpr std::uint16_t decode(const Bytes& bytes) {
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(bytes[0]) << 8)
                                    | std::to_integer<unsigned>(bytes[1]));
}

struct Frame {
    inline static int constructions = 0;
    Bytes bytes;
    explicit Frame(std::uint16_t value) : bytes(encode(value)) { ++constructions; }
    Frame(const Frame&) = delete;
    Frame(Frame&&) = delete;
};

int factory_calls = 0;
std::uint16_t make_code() {
    ++factory_calls;
    return 0xBEEF;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << std::boolalpha;
        std::cout << "===== 1. PRINCIPLES =====\n";
        constexpr auto wire = encode(std::uint16_t{0x1234}); // 高字节在前。
        static_assert(decode(wire) == 0x1234);
        static_assert(!std::is_convertible_v<std::byte, int>);
        require(wire[0] == std::byte{0x12} && wire[1] == std::byte{0x34},
                "wire representation failed");
        std::cout << "wire: " << std::to_integer<unsigned>(wire[0]) << ' '
                  << std::to_integer<unsigned>(wire[1])
                  << " value=" << decode(wire) << '\n'; // 18 52 value=4660

        std::cout << "===== 2. SYNTAX =====\n";
        std::map<std::string, Frame> cache;
        auto [first, created] = cache.try_emplace("alpha", std::uint16_t{0x1234});
        require(created, "initial insert failed");
        Frame* original = &first->second; // 在 extract 之前取得指针。
        const auto duplicate = cache.try_emplace("alpha", make_code());
        require(!duplicate.second && factory_calls == 1 && Frame::constructions == 1,
                "argument evaluation and construction were confused");
        std::cout << "factory/constructions: " << factory_calls << ' '
                  << Frame::constructions << '\n'; // 1 1：实参求值，Frame 未再构造。
        if (cache.find("alpha") == cache.end()) {
            cache.try_emplace("alpha", make_code()); // 显式分支才能避免调用工厂。
        }
        require(factory_calls == 1, "factory was not deferred");
        unsigned next_id = 1;
        std::map<std::string, unsigned> ids;
        ids.try_emplace("known", next_id++);
        ids.try_emplace("known", next_id++);
        require(next_id == 3 && ids.at("known") == 1, "argument side effect disappeared");
        std::cout << "next id: " << next_id << '\n'; // 3，重复键也执行 ++。

        std::cout << "===== 3. APPLICATION =====\n";
        cache.try_emplace("taken", std::uint16_t{0xCAFE});
        auto node = cache.extract("alpha");
        require(!node.empty(), "source node missing");
        node.key() = "taken";
        auto collision = cache.insert(std::move(node));
        require(!collision.inserted && !collision.node.empty(), "node not recovered");
        require(decode(collision.position->second.bytes) == 0xCAFE,
                "existing entry was overwritten");
        std::cout << "collision kept node: " << !collision.node.empty() << '\n'; // true
        collision.node.key() = "restored";
        auto restored = cache.insert(std::move(collision.node));
        require(restored.inserted && restored.node.empty(), "reinsertion failed");
        // 节点已重新进入容器，现在才再次使用 extract 前保存的指针。
        require(&restored.position->second == original && decode(original->bytes) == 0x1234,
                "element identity changed");
        static_assert(!std::is_move_constructible_v<Frame>);
        require(Frame::constructions == 2 && cache.size() == 2, "element was reconstructed");
        std::cout << "same frame: " << (&restored.position->second == original)
                  << " constructions=" << Frame::constructions << '\n'; // true constructions=2
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
