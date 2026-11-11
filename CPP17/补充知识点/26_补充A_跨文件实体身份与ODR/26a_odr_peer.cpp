// 第二翻译单元：必须单独编译或与主程序一起交给编译器，不能被 #include。
#include "26a_shared.hpp"

namespace lesson26a {
Addresses peer_addresses() {
    // 在本 TU 的普通函数内取得内部链接对象的地址，不把此函数写成 inline。
    return {&shared_calls, &local_calls, &protocol, &local_tag,
            &Limits::attempts, &registry()};
}

void peer_visit() {
    ++shared_calls;
    ++local_calls;
    ++registry().visits;
}
} // namespace lesson26a
