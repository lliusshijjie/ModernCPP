// 第 47 课示例（导入方）：Modules 模块入门（C++20）
//
// import weather 之后：可直接查找导出的接口名字——
//   命名模块的宏不导入，hidden_helper 不能直接查找；导出类型的成员仍可使用
// （接口单元的编译说明见 47_weather_module.cpp 头部注释）

#include <iostream>          // 导入方自己的标准 IO 仍用传统包含
import weather;              // 导入模块：天气库的全部导出实体到手

int main() {
    weather::Reading r{.city = "osaka", .celsius = 12};   // 指定初始化器
    std::cout << weather::describe(r) << ' '
              << weather::adjusted(r) << '\n';           // osaka: 12C 24

    // ===== 隔离墙（两条编译错示例，注释保存）=====
    // hidden_helper(1);
    //   编译错：'hidden_helper' was not declared——未导出的名字不能在导入方直接查找

    // int m = SECRET_MACRO;
    //   编译错：宏不随 import 泄漏——每个翻译单元的宏世界独立

    // 本课输出：osaka: 12C 24
    return 0;
}
