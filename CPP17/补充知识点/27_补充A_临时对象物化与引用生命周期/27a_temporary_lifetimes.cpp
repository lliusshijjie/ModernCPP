// 补充 27A：强制消除不等于没有临时对象；引用是否延长寿命取决于绑定方式。
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

struct Ticket {
    inline static int alive = 0;
    int id;
    explicit Ticket(int number) : id(number) { ++alive; }
    ~Ticket() { --alive; }
    Ticket(const Ticket&) = delete;
    Ticket(Ticket&&) = delete;
};

Ticket make_ticket(int id) { return Ticket{id}; }

const char* category(const Ticket&) { return "lvalue"; }
const char* category(Ticket&&) { return "rvalue"; }
int read_id(const Ticket& value) { return value.id; }

class CachedTicket {
    Ticket value_;
public:
    explicit CachedTicket(int id) : value_(make_ticket(id)) {}
    const Ticket& view() const & { return value_; }
    const Ticket& view() const && = delete; // 拒绝从临时拥有者借用内部对象。
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << "===== 1. PRINCIPLES =====\n";
        {
            const Ticket& kept = make_ticket(7); // 临时对象活到此引用作用域结束。
            require(kept.id == 7 && Ticket::alive == 1, "reference lifetime failed");
            std::cout << "bound temporary alive: " << Ticket::alive << '\n'; // 1
        }
        require(Ticket::alive == 0, "temporary was not destroyed");
        std::cout << "after scope: " << Ticket::alive << '\n'; // 0

        std::cout << "===== 2. SYNTAX =====\n";
        {
            Ticket&& named = make_ticket(8); // 局部右值引用也可以延长临时对象寿命。
            static_assert(std::is_same_v<decltype(named), Ticket&&>);
            static_assert(std::is_same_v<decltype((named)), Ticket&>);
            std::cout << "named expression: " << category(named) << '\n'; // lvalue
            std::cout << "move expression: " << category(std::move(named)) << '\n'; // rvalue
            require(Ticket::alive == 1, "std::move should not construct a Ticket");
        }
        const int read = read_id(make_ticket(9)); // 实参临时量只活到完整表达式结束。
        std::cout << "parameter read: " << read << " alive=" << Ticket::alive << '\n'; // 9 alive=0
        require(read == 9 && Ticket::alive == 0, "argument lifetime failed");
        {
            const int& member = make_ticket(10).id; // 直接绑定子对象，延长整个 Ticket 的寿命。
            require(member == 10 && Ticket::alive == 1, "subobject lifetime failed");
            std::cout << "member owner alive: " << Ticket::alive << '\n'; // 1
        }

        std::cout << "===== 3. APPLICATION =====\n";
        {
            auto owner = CachedTicket{42}; // Ticket 不可移动，仍可在成员位置直接构造。
            const Ticket& borrowed = owner.view();
            require(borrowed.id == 42 && Ticket::alive == 1, "owner borrowing failed");
            std::cout << "borrowed id: " << borrowed.id << '\n'; // 42
        }
#ifdef LESSON27A_TEMP_VIEW
        const auto& dangling = CachedTicket{99}.view(); // 诊断实验：预期命中已删除重载。
        (void)dangling;
#endif
        require(Ticket::alive == 0, "owner cleanup failed");
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
