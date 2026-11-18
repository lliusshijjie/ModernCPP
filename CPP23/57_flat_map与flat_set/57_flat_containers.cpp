// 第 57 课示例：std::flat_map / flat_set（C++23）
//
// 结构：① 原理验证 —— 排序语义；查找 O(log n)；迭代缓存友好
//       ② 语法演示 —— 与 map 的接口对齐；迭代器的现代/旧式差异
//       ③ 应用实战 —— 读多写少的配置表 + 批量构建
//
// 本机（MSYS2 g++ 15.1.0）已提供 <flat_map>（GCC 15 实现），
// 本课全部真实接口演示；链接 -lstdc++exp（系列惯例，print 需要）：
//   g++ -std=c++23 -Wall -Wextra 57_flat_containers.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <flat_map>
#include <flat_set>
#include <functional>   // std::less
#include <initializer_list>
#include <iterator>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：插入乱序，迭代恒有序（"扁平"= 底层排序数组，不是树）
    std::flat_map<std::string, int> fm;
    fm["banana"] = 2;
    fm["apple"] = 1;
    fm["cherry"] = 3;
    for (const auto& [k, v] : fm) std::print("{}={} ", k, v);
    std::println("| sorted");                          // apple=1 banana=2 cherry=3

    // 验证 2：常用查询与 map 相近——迁移仍须检查引用/失效/提取接口
    std::print("{} {} {}\n", fm.contains("apple"),
                fm.find("zzz") == fm.end(), fm.size()); // true true 3

    // 验证 3：flat_set 忽略重复插入（set 语义）
    std::flat_set<int> fs{5, 1, 3, 1, 5};
    std::print("{} ", fs.size());                       // 3
    for (int x : fs) std::print("{} ", x);
    std::println("|");                                  // 1 3 5 |

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 普通区间构造：允许未排序输入；这里从左值元素复制后排序去重
    std::vector<std::pair<int, std::string>> raw{{3, "c"}, {1, "a"}, {2, "b"}};
    std::flat_map<int, std::string> built(raw.begin(), raw.end());
    std::print("{} {}\n", built.at(2), built.size());   // b 3

    // 2.2 有序区间的力量：lower_bound / equal_range（范围内首个 >= / 相等段）
    std::flat_set<int> marks{10, 20, 30, 40};
    auto it = marks.lower_bound(25);                    // 首个 >= 25
    std::print("{} ", *it);                             // 30
    auto [lo, hi] = marks.equal_range(30);
    std::print("{} {}\n", *lo, *hi);                    // 30 40（hi 是开区间尾）

    // 2.3 迭代器能力：现代概念下 flat_map/flat_set 都是随机访问
    static_assert(std::random_access_iterator<
                  std::flat_map<int, int>::iterator>);
    static_assert(std::random_access_iterator<
                  std::flat_set<int>::iterator>);
    std::print("both RA ok\n");
    // flat_map 的代理引用不能满足旧式 ForwardIterator 的真引用要求，
    // libstdc++ 旧式标签是 input_iterator_tag；flat_set 本机标签是 RA。
    // RA 仅说明遍历能力；键只读，ranges::sort(fm) 仍不合法。
    static_assert(!std::sortable<std::flat_map<int, int>::iterator>);
    static_assert(std::is_same_v<std::flat_map<int, int>::reference,
                                std::pair<const int&, int&>>);

    // 2.4 指定比较器与比较对象类型（透明比较器，对照第 05A 补充课）
    std::flat_set<std::string, std::greater<>> desc{"b", "a", "c"};
    std::print("{} {} {}\n", *desc.begin(), *desc.rbegin(),
                desc.contains("a"));                    // c a true（降序）

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 读多写少的配置表：批量构建 + 纯查找——flat 的主场
    std::flat_map<int, const char*> http;
    for (int i = 100; i <= 500; i += 100)
        http[i] = i == 200 ? "ok" : "info";
    http[404] = "nf";                                   // 后补的键：中段插入
    int hits = 0;
    for (int code : {200, 301, 404, 500})
        if (http.contains(code)) ++hits;
    std::print("hits={}\n", hits);                      // hits=3（301 未收录）

    // 3.2 删除与插入的代价面：中段插入 O(n) 搬移——别在写热路径用
    //     （此时 6 键：100..500 五个 + 404；分步写避免实参求值顺序
    //      影响——正是第 28 课求值定序的教训）
    http.erase(100);
    std::size_t after_erase = http.size();              // 5
    http.insert_or_assign(505, "teapot");
    std::print("{} {}\n", after_erase, http.size());    // 5 6
    // extract 存在，但它取走整批底层容器，接口是右值限定的 extract() &&。
    // 与 map.extract(key) 返回单个节点句柄不同。
    auto storage = std::move(built).extract();
    std::print("extracted={} empty={}\n", storage.keys.size(), built.empty()); // 3 true
    built.replace(std::move(storage.keys), std::move(storage.values));
    std::print("restored={}\n", built.at(2));         // b
    // replace 的前提：keys 有序且唯一，keys/values 长度相等。
    std::flat_set<int> ready(std::sorted_unique, std::vector<int>{1, 3, 5});
    std::print("ready={}\n", ready.size());          // 3：调用方保证已排序去重
    // 本机 15.1 的 erase_if 是隐藏友元，用 ADL；谓词兼容 pair 与内部 zip tuple。
    auto dropped = erase_if(http, [](const auto& entry) { return std::get<0>(entry) == 505; });
    std::print("erase_if={} remaining={}\n", dropped, http.size()); // 1 5

    // 3.3 实战心法（对照 ①②③）：
    //   读多写少、批量构建、区间扫描 -> flat 系
    //   频繁插入删除（尤其中段） -> 传统 map/set
    //   泛型代码用 C++20 迭代器概念分发，避开旧标签差异
    return 0;
}
