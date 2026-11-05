# 补充 18A · any 的类型处理器注册（C++17）

> 阅读顺序为第 18 课之后，不占每日主线编号。
> 所需标准为 C++17。配套代码见 [18a_any_handler_registry.cpp](18a_any_handler_registry.cpp)。

## 一、底层原理

`std::any` 能持有不同的值，并通过 `type()` 暴露当前类型信息。调用业务方法需要先恢复具体类型。若扩展类型由不同模块提供，可以让它们各自注册处理器，统一入口按运行时类型找到对应的函数。

## 二、具体语法

示例使用 `std::map<std::type_index, Handler>` 保存处理器。函数指针的统一签名是 `std::string (*)(const std::any&)`。注册时的类型参数 `T` 同时决定映射的键和处理器内的取值类型。

```cpp
handlers_.emplace(std::type_index(typeid(T)), [](const std::any& value) {
    return std::any_cast<const T&>(value).describe();
});
```

无捕获 lambda 可转换为函数指针。注册 `Caption` 时，生成的处理器使用 `any_cast<const Caption&>`。注册 `Reading` 时则使用 `any_cast<const Reading&>`。两者都能通过同一个 `Handler` 类型存放。

## 三、实际应用与最佳实践

展示模块接收标题卡片和读数，注册后就可以通过 `registry.render(value)` 得到描述。读取时先用 `value.type()` 查表，命中后再调用处理器。结果用 `optional<string>` 表达有无描述，调用方用 `value_or("unregistered")` 显示提示。

添加读数处理器前，读数输出 `unregistered`。添加后，同一个 `any` 输出 `reading: 7`。

## 常见陷阱

- 注册模板按保存的值类型工作，示例用静态断言限制为衰减后的可复制类型。传给 `any_cast` 的引用形式只决定访问方式。
- 处理器借用 `any` 内部对象来生成字符串。返回的字符串拥有自己的内容，可以在原对象替换后继续使用。
- 本例将空 `any` 和未注册类型都视为无描述。如果调用方需要区分这两种原因，可以在查表之前检查 `has_value()`。


## 完整示例

在仓库根目录编译运行，先预测输出，再对照源码注释。

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/18_补充A_any的类型处理器注册/18a_any_handler_registry.cpp" -o run.exe
.\run.exe
```

## 练习

1. 新增 `Photo` 类型及其 `describe()`，观察注册前后的输出。
2. 在调用处分别显示空容器和未注册类型，保持注册表的接口简洁。

## 参考

- [C++17 草案 N4659 · any_cast 取值规则](https://timsong-cpp.github.io/cppwp/n4659/any.nonmembers)
- [C++17 草案 N4659 · type_index 作为容器键](https://timsong-cpp.github.io/cppwp/n4659/type.index)
- [C++17 草案 N4659 · lambda 闭包及函数指针转换](https://timsong-cpp.github.io/cppwp/n4659/expr.prim.lambda.closure)

## 学习衔接

从 [第 18 课](../../18_std_any/18_std_any.md) 进入本节，其他补充内容见 [C++17 补充知识点目录](../README.md)。
