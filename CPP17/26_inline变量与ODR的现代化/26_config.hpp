// 第 26 课迷你头文件：header-only 库的全局状态模板（C++17 inline 变量）
// 本文件可被任意多个 .cpp include —— 链接后所有实体唯一。
// C++14 时代这里每个变量都要么 Meyers 函数包一层、要么另配一个 .cpp。

#pragma once

#include <string>

inline int api_calls = 0;                     // 可变全局：跨 TU 合并为一个实体

inline const std::string app_version = "v1.2.3";  // 动态初始化的 inline 常量

inline constexpr int max_retry = 3;           // 常量：唯一实体 + 唯一地址

struct Config {
    // C++17：static constexpr 成员隐式 inline —— 被 odr-use（取地址/绑引用）
    // 不再需要类外定义（C++14 会链接报错 undefined reference）
    static constexpr double ratio = 0.5;
};
