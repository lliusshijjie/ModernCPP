// C++17 综合练习：迷你自动售货机，覆盖第 11～17 课。
// 所有逻辑都在本文件；固定指令演示，没有交互菜单和外部依赖。
// [11] 结构化绑定 -> show；[12] if 初始化语句 -> buy / stock。
// [13] if constexpr -> valid；[14] 折叠表达式 -> log。
// [15] CTAD / 推导指引 -> overloaded / 演示中的 vector。
// [16] optional -> find；[17] variant / visit -> Command / process。
// 编译：g++ -std=c++17 -Wall -Wextra -Wpedantic CPP17/practice/vending_machine.cpp -o vending_machine
// 运行：./vending_machine；自检：./vending_machine --self-test。
// 阅读顺序：demo -> process -> valid -> buy -> self_test。输出只使用 ASCII。

#include <cstddef>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

struct Product { std::string code; int price; int stock; }; // 金额单位：分。
struct InsertCoin { int cents; };
struct Buy { std::string code; };
struct Refund {};
using Command = std::variant<InsertCoin, Buy, Refund>;

template <class... T> struct overloaded : T... { using T::operator()...; };
template <class... T> overloaded(T...) -> overloaded<T...>; // [15] C++17 显式指引。

template <class... T>
void log(std::ostream& out, const T&... parts) { // [14] 一条日志可以包含不同类型。
    (out << ... << parts) << '\n';
}

template <class C>
bool valid(const C& command) {
    // [13] Refund 没有 cents 或 code，被丢弃分支不会实例化这些成员访问。
    if constexpr (std::is_same_v<C, InsertCoin>) return command.cents > 0 && command.cents <= 1'000;
    else if constexpr (std::is_same_v<C, Buy>) return !command.code.empty();
    else return std::is_same_v<C, Refund>;
}

class VendingMachine {
public:
    VendingMachine(std::vector<Product> catalog, std::ostream& output)
        : products_(std::move(catalog)), out_(output) {}

    bool process(const Command& command) {
        // [15/17] 三个 lambda 合成重载集；每个分支都返回 bool。
        const auto dispatch = overloaded{
            [this](const InsertCoin& c) { return insert(c.cents); },
            [this](const Buy& c) { return buy(c.code); },
            [this](const Refund&) { return refund(); }
        };
        return std::visit([&](const auto& c) { // 运行期选择指令，再校验、处理。
            if (!valid(c)) { log(out_, "rejected: invalid command"); return false; }
            return dispatch(c);
        }, command);
    }

    int balance() const { return balance_; }
    std::optional<int> stock(const std::string& code) const {
        if (auto index = find(code); index) return products_[*index].stock;
        return std::nullopt;
    }
    void show() const {
        for (const auto& [code, price, remaining] : products_) // [11] 引用式分解。
            log(out_, code, ": price=", price, ", stock=", remaining);
        log(out_, "balance=", balance_);
    }

private:
    std::optional<std::size_t> find(const std::string& code) const { // [16] 缺失是正常结果。
        for (std::size_t i = 0; i < products_.size(); ++i)
            if (products_[i].code == code) return i;
        return std::nullopt;
    }
    bool insert(int cents) {
        if (cents > 10'000 - balance_) { log(out_, "rejected: balance limit"); return false; }
        balance_ += cents;
        log(out_, "insert ", cents, ", balance=", balance_);
        return true;
    }
    bool buy(const std::string& code) {
        // [12/16] index 的作用域覆盖 if/else；包含数值 0 的 optional 也有值。
        if (auto index = find(code); index) {
            auto& product = products_[*index];
            if (product.stock == 0) { log(out_, "sold out: ", code); return false; }
            if (balance_ < product.price) { log(out_, "not enough credit: ", code); return false; }
            balance_ -= product.price;
            --product.stock;
            log(out_, "bought ", code, ", balance=", balance_);
            return true;
        } else {
            log(out_, "unknown product: ", code);
            return false;
        }
    }
    bool refund() {
        if (balance_ == 0) { log(out_, "nothing to refund"); return false; }
        const int change = std::exchange(balance_, 0); // 顺便复习 C++14。
        log(out_, "refund ", change);
        return true;
    }

    std::vector<Product> products_;
    std::ostream& out_;
    int balance_ = 0;
};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // Release 下仍检查。
}

void demo(std::ostream& out) {
    // [15] CTAD 推导 vector<Product>；目录中的商品编码唯一，价格为正、库存非负。
    VendingMachine machine(std::vector{Product{"Tea", 125, 2}, Product{"Water", 75, 1}}, out);
    // vector<Command>：每个元素可持有不同指令，各自携带所需字段。
    const std::vector script{
        Command{InsertCoin{100}}, Command{Buy{"Tea"}}, Command{InsertCoin{100}},
        Command{Buy{"Tea"}}, Command{Buy{"Water"}}, Command{InsertCoin{200}},
        Command{Buy{"Water"}}, Command{Buy{"Unknown"}}, Command{Refund{}},
        Command{InsertCoin{-10}}, Command{Refund{}}
    };
    int accepted = 0;
    for (const auto& command : script) if (machine.process(command)) ++accepted;
    machine.show();
    log(out, "accepted=", accepted, ", rejected=", script.size() - static_cast<std::size_t>(accepted));
    check(accepted == 6 && machine.balance() == 0, "unexpected command count or balance");
    check(machine.stock("Tea") == 1 && machine.stock("Water") == 0, "incorrect final stock");
    check(!machine.stock("Unknown"), "missing product unexpectedly exists");
    log(out, "Demo checks: PASS");
}

void self_test() {
    std::ostringstream sink;
    demo(sink);
    VendingMachine machine(std::vector{Product{"Tea", 125, 1}}, sink);
    check(!machine.process(InsertCoin{0}) && !machine.process(InsertCoin{1'001}), "invalid amount accepted");
    check(!machine.process(Buy{""}) && !machine.process(Buy{"Missing"}), "invalid product accepted");
    check(!machine.process(Buy{"Tea"}) && machine.stock("Tea") == 1, "failed buy changed stock");
    for (int i = 0; i < 10; ++i) check(machine.process(InsertCoin{1'000}), "valid deposit rejected");
    check(!machine.process(InsertCoin{1}) && machine.balance() == 10'000, "balance cap was exceeded");
    check(machine.process(Buy{"Tea"}) && machine.balance() == 9'875, "exact purchase arithmetic failed");
    check(!machine.process(Buy{"Tea"}) && machine.balance() == 9'875, "sold-out buy changed balance");
    check(machine.process(Refund{}) && machine.balance() == 0, "refund did not clear balance");
    check(!machine.process(Refund{}), "empty refund unexpectedly succeeded");
    VendingMachine empty(std::vector<Product>{}, sink);
    check(!empty.stock("Tea") && !empty.process(Buy{"Tea"}), "empty catalog lookup failed");
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) demo(std::cout);
        else if (argc == 2 && std::string(argv[1]) == "--self-test") {
            self_test();
            log(std::cout, "Self-tests: PASS");
        } else {
            std::cerr << "Usage: vending_machine [--self-test]\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
