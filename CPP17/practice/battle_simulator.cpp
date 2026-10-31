// C++17 综合练习：回合制战斗结算器（第 11～13 课，完整单文件不超过 200 行）。
// [11] 结构化绑定：拆解攻击结果和战斗结束后的两名角色快照。
// [12] if/switch 初始化语句：命中判定、回合阶段选择。
// [13] if constexpr：物理攻击读取 might，魔法攻击读取 power 并消耗 mana。
// 编译：g++ -std=c++17 -Wall -Wextra -Wpedantic CPP17/practice/battle_simulator.cpp -o battle_simulator
// 无参数观看固定战斗；--self-test 检查规则。阅读：main -> duel -> strike -> self_test。

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

struct Unit {
    std::string name;
    int hp;
    int strength;
    int magic;
    int armor;
    int resistance;
    int dodge;
    int mana;
};
struct PhysicalAttack { int might; int accuracy; int crit_chance; };
struct MagicAttack { int power; int accuracy; int crit_chance; int mana_cost; };
struct Roll { int hit; int critical; }; // 两个互相独立的 [0, 99] 判定值。
struct Result { int damage; bool critical; bool defeated; std::string state; };
template <class> constexpr bool unsupported_v = false;

template <class Attack>
Result strike(Unit& attacker, Unit& target, const Attack& skill, Roll roll) {
    if (roll.hit < 0 || roll.hit > 99 || roll.critical < 0 || roll.critical > 99)
        throw std::invalid_argument("rolls must be in [0, 99]");
    if (attacker.hp <= 0 || target.hp <= 0) return {0, false, false, "inactive"};
    int base = 0;
    // [13] 类型在编译期确定；被丢弃分支中的类型相关成员访问不会实例化。
    if constexpr (std::is_same_v<Attack, PhysicalAttack>) {
        base = attacker.strength + skill.might - target.armor;
    } else if constexpr (std::is_same_v<Attack, MagicAttack>) {
        if (attacker.mana < skill.mana_cost) return {0, false, false, "no-mana"};
        attacker.mana -= skill.mana_cost; // 施法就扣法力，未命中也不返还。
        base = attacker.magic + skill.power - target.resistance;
    } else {
        static_assert(unsupported_v<Attack>, "unsupported attack type");
    }
    // [12] chance 只属于这次命中判断；0% 必定未命中，100% 必定命中。
    if (const int chance = std::max(0, std::min(100, skill.accuracy - target.dodge));
        roll.hit >= chance) return {0, false, false, "miss"};
    base = std::max(0, base);
    const bool critical = base > 0 && roll.critical < skill.crit_chance;
    const int damage = std::min(target.hp, base * (critical ? 2 : 1));
    target.hp -= damage; // 记录实际扣除的 HP，过量伤害不计入 damage。
    return {damage, critical, target.hp == 0, damage == 0 ? "blocked" : "hit"};
}

void print_result(const Unit& attacker, const Unit& target, const Result& result, std::ostream& out) {
    // [11] 引用式绑定不复制 Result 中的字符串；绑定名仅在 result 生存期内有效。
    const auto& [damage, critical, defeated, state] = result;
    out << attacker.name << " -> " << target.name << ": " << state << ", damage=" << damage;
    if (critical) out << " CRITICAL";
    if (defeated) out << " DEFEATED";
    out << ", target-hp=" << target.hp << ", attacker-mana=" << attacker.mana << '\n';
}

std::pair<Unit, Unit> duel(std::ostream& out) {
    // 属性顺序：name / hp / strength / magic / armor / resistance / dodge / mana。
    Unit knight{"Knight", 40, 9, 0, 5, 2, 5, 0};
    Unit mage{"Mage", 35, 2, 12, 3, 6, 10, 8};
    const PhysicalAttack slash{5, 90, 25};
    const MagicAttack fire{4, 85, 10, 3};
    // 固定判定值让输出可复现：未命中、命中、暴击、法力耗尽、最后一击。
    // 最后一个判定值用于检查提前结束：法师被击败后不应发生第 10 次行动。
    const Roll rolls[]{{95, 99}, {20, 99}, {30, 5}, {90, 99}, {90, 99},
                       {10, 0}, {10, 99}, {10, 99}, {10, 5}, {0, 0}};
    int turn = 0;
    for (const auto& roll : rolls) {
        if (knight.hp == 0 || mage.hp == 0) break;
        out << "Turn " << ++turn << ": ";
        // [12] phase 只在 switch 内可见：奇数回合骑士行动，偶数回合法师行动。
        switch (const int phase = turn % 2; phase) {
        case 1: {
            const auto result = strike(knight, mage, slash, roll);
            print_result(knight, mage, result, out);
            break;
        }
        case 0: {
            const auto result = strike(mage, knight, fire, roll);
            print_result(mage, knight, result, out);
            break;
        }
        }
    }
    return std::make_pair(knight, mage); // 返回对象快照，避免返回局部对象的引用。
}

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // Release 构建也检查。
}

void self_test() {
    const Unit hero{"Hero", 30, 8, 10, 2, 3, 0, 6};
    const Unit foe{"Foe", 20, 4, 4, 5, 7, 0, 0};
    const auto physical = PhysicalAttack{4, 100, 25};
    const auto magic = MagicAttack{4, 100, 25, 3};
    auto a = hero;
    auto b = foe;
    auto result = strike(a, b, physical, {99, 25});
    check(result.damage == 7 && !result.critical && b.hp == 13 && a.mana == 6,
          "physical damage or critical threshold is incorrect");
    a = hero; b = foe; b.dodge = 20;
    result = strike(a, b, physical, {80, 99});
    check(result.state == "miss" && b.hp == 20, "a roll at the hit threshold must miss");
    result = strike(a, b, physical, {79, 99});
    check(result.damage == 7 && b.hp == 13, "a roll below the hit threshold must hit");
    a = hero; b = foe;
    result = strike(a, b, magic, {0, 24});
    check(result.damage == 14 && result.critical && b.hp == 6 && a.mana == 3,
          "magic must use resistance, double on critical and spend mana");
    a = hero; b = foe;
    result = strike(a, b, MagicAttack{4, 0, 100, 3}, {0, 0});
    check(result.state == "miss" && result.damage == 0 && b.hp == 20 && a.mana == 3,
          "a missed spell must still spend mana");
    a.mana = 2;
    result = strike(a, b, magic, {0, 0});
    check(result.state == "no-mana" && a.mana == 2 && b.hp == 20,
          "insufficient mana must leave both units unchanged");
    a = hero; b = foe; b.armor = 100;
    result = strike(a, b, physical, {0, 0});
    check(result.state == "blocked" && result.damage == 0 && !result.critical && b.hp == 20,
          "armor must not produce negative damage or heal the target");
    b = foe; b.hp = 3;
    result = strike(a, b, physical, {0, 0});
    check(result.damage == 3 && result.defeated && b.hp == 0, "overkill must stop at zero HP");
    result = strike(a, b, magic, {0, 0});
    check(result.state == "inactive" && a.mana == 6, "a defeated target must not consume mana");
    a.hp = 0; b = foe;
    result = strike(a, b, physical, {0, 0});
    check(result.state == "inactive" && b.hp == 20, "a defeated attacker must not act");
    bool rejected = false;
    try { strike(a, b, physical, {100, 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "out-of-range rolls must be rejected");
    std::ostringstream transcript;
    // [11] 按值拆解返回的 pair；这里得到独立快照。
    const auto [knight, mage] = duel(transcript);
    check(knight.hp == 26 && mage.hp == 0 && mage.mana == 2, "fixed duel ended incorrectly");
    check(transcript.str().find("Turn 10:") == std::string::npos, "battle continued after defeat");
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            const auto [knight, mage] = duel(std::cout);
            const auto winner = mage.hp == 0 ? knight.name : knight.hp == 0 ? mage.name : "draw";
            std::cout << "Winner: " << winner << " | HP: " << knight.hp << ':' << mage.hp << '\n';
        } else if (argc == 2 && std::string(argv[1]) == "--self-test") {
            self_test();
            std::cout << "Self-tests: PASS\n";
        } else {
            std::cerr << "Usage: battle_simulator [--self-test]\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
