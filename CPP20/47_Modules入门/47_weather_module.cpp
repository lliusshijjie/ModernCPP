// 第 47 课示例（模块接口单元）：Modules 模块入门（C++20）
//
// 本课是双文件示例（模块无法单文件演示，参照补充课 26A 的多文件先例）：
//   47_weather_module.cpp —— 模块接口单元（本文件）：export module weather
//   47_modules_main.cpp    —— 导入方（import weather）
//
// 编译运行（两步，注意 -fmodules 旗子，见 47_Modules入门.md）：
//   g++ -std=c++20 -fmodules -c 47_weather_module.cpp -o 47_weather_module.o
//   g++ -std=c++20 -fmodules 47_modules_main.cpp 47_weather_module.o -o run
//   ./run
// 先预测每行输出，再运行对照！

module;                      // 全局模块片段：传统 #include 只能放这里
#include <string>

export module weather;       // 模块名：weather（接口单元的身份证）

#define SECRET_MACRO 42      // 模块内宏——不会泄漏给导入方

int hidden_helper(int c) {   // 未 export：模块私有（导入方看不见）
    return c * 2;
}

export namespace weather {   // export 一次，命名空间内全部实体导出
struct Reading {             // 导出的聚合：可用指定初始化器（第 41 课）
    std::string city;
    int celsius;
};

std::string describe(const Reading& r) {     // 导出的函数
    return r.city + ": " + std::to_string(r.celsius) + "C";
}

int adjusted(const Reading& r) {             // 公开接口内部调用私有助手
    return hidden_helper(r.celsius);        // ——实现细节被封装在模块里
}
}  // namespace weather
