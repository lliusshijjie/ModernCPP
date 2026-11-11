// 补充 26A：此头文件由两个翻译单元分别包含。
#ifndef MODERNCPP_26A_SHARED_HPP
#define MODERNCPP_26A_SHARED_HPP

namespace lesson26a {
inline int shared_calls = 0;
static int local_calls = 0; // 内部链接：每个翻译单元各一份。
inline constexpr int protocol = 17;
constexpr int local_tag = 17;

struct Limits {
    static constexpr int attempts = 3; // 隐式 inline。
};

struct Registry {
    int visits = 0;
};

inline Registry& registry() {
    static Registry instance; // 外部链接 inline 函数的局部 static 由各 TU 共享。
    return instance;
}

struct Addresses {
    int* shared;
    int* local;
    const int* protocol;
    const int* tag;
    const int* attempts;
    Registry* registry;
};

Addresses peer_addresses();
void peer_visit();
} // namespace lesson26a
#endif
