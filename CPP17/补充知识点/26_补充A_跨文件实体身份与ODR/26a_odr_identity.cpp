// 补充 26A：同一头文件、两个翻译单元，验证实体地址与共享状态。
#include "26a_shared.hpp"

#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        using namespace lesson26a;
        std::cout << std::boolalpha;
        const Addresses peer = peer_addresses();

        std::cout << "===== 1. PRINCIPLES =====\n";
        const bool shared_identity = peer.shared == &shared_calls;
        const bool local_identity = peer.local == &local_calls;
        std::cout << "inline same address: " << shared_identity << '\n'; // true
        std::cout << "static same address: " << local_identity << '\n';  // false
        require(shared_identity && !local_identity, "variable identity failed");

        std::cout << "===== 2. SYNTAX =====\n";
        const bool protocol_identity = peer.protocol == &protocol;
        const bool tag_identity = peer.tag == &local_tag;
        const bool member_identity = peer.attempts == &Limits::attempts;
        std::cout << "inline constexpr same: " << protocol_identity << '\n'; // true
        std::cout << "plain constexpr same: " << tag_identity << '\n';       // false
        std::cout << "class constexpr same: " << member_identity << '\n';    // true
        require(protocol_identity && !tag_identity && member_identity,
                "constant identity failed");

        std::cout << "===== 3. APPLICATION =====\n";
        ++shared_calls;
        ++local_calls;
        ++registry().visits;
        peer_visit();
        const bool registry_identity = peer.registry == &registry();
        std::cout << "shared/local/peer-local: " << shared_calls << ' '
                  << local_calls << ' ' << *peer.local << '\n'; // 2 1 1
        std::cout << "registry same: " << registry_identity
                  << " visits=" << registry().visits << '\n';   // true visits=2
        require(shared_calls == 2 && local_calls == 1 && *peer.local == 1,
                "state sharing failed");
        require(registry_identity && registry().visits == 2, "local static identity failed");
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
