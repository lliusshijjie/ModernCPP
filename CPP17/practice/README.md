# C++17 综合练习：回合制战斗结算器

适合学完第 11～13 课后阅读。完整程序在
[`battle_simulator.cpp`](battle_simulator.cpp) 中，不超过 200 行，包含中文注释和内置自检。

骑士和法师轮流行动：骑士使用物理攻击，法师施放魔法。程序结算命中、暴击、
法力消耗和 HP 变化，遇到角色被击败时结束战斗。使用固定判定值，便于先预测再运行。

## 三个知识点如何协作

| 课程 | 在练习中的作用 | 代码入口 |
|---|---|---|
| 11：结构化绑定 | 将攻击结果拆成伤害、暴击、击败、状态；拆解两名角色的最终快照 | `print_result`、`main` |
| 12：if / switch 初始化语句 | 将命中率限制在判断作用域内；计算当前行动阶段并立即分发 | `strike`、`duel` |
| 13：if constexpr | 物理攻击读取 `might` 和护甲；魔法攻击读取 `power`、抗性和法力消耗 | `strike<Attack>` |

`PhysicalAttack` 没有 `power`、`mana_cost`，`MagicAttack` 没有 `might`。
模板中的 `if constexpr` 让这些不同的成员访问各自在对应类型下实例化；
将其换成普通 `if` 会使两种类型都尝试编译不适用的成员访问。
命中与法力是否足够仍在运行时由普通 `if` 判断。

## 编译运行

从仓库根目录执行，以下为 PowerShell 命令：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic CPP17/practice/battle_simulator.cpp -o CPP17/practice/battle_simulator.exe
.\CPP17\practice\battle_simulator.exe
.\CPP17\practice\battle_simulator.exe --self-test
```

无参数运行 9 次行动的固定演示；`--self-test` 验证战斗规则，成功输出 `Self-tests: PASS`。
退出码：`0` 为成功，`1` 为自检或判定值错误，`2` 为启动参数错误。
输出保持 ASCII，中文说明只放在注释和本文中。

## 战斗规则

以下是本练习自定义的简化规则，角色和招式在代码中直接创建，使用非负小整数属性。

- 物理基础伤害：`max(0, strength + might - armor)`。
- 魔法基础伤害：`max(0, magic + power - resistance)`。
- 命中率：`max(0, min(100, accuracy - dodge))`；`hit` 判定值严格小于命中率才命中。
- 正基础伤害且 `critical` 判定值严格小于暴击率时暴击，伤害翻倍；零伤害不会暴击。
- 实际伤害不超过目标当前 HP，因此 HP 最低为零。
- 攻击者或目标已被击败时不执行攻击，也不消耗法力。
- 魔法法力不足时不执行攻击；法力足够时先扣除，未命中也不返还。
- `hit`、`critical` 是两个独立的 0～99 判定值；范围错误抛出异常。

演示中的主要事件：

| 行动 | 事件 | 结果 |
|---|---|---|
| 1 | 骑士攻击未命中 | 法师 HP 仍为 35 |
| 2 | 法师命中 | 骑士扣 14 HP；法师剩 5 法力 |
| 3 | 骑士暴击 | 法师扣 22 HP，剩 13 |
| 4 | 法师未命中 | 法力仍扣 3，剩 2 |
| 5 | 骑士未命中 | 法师 HP 仍为 13 |
| 6 | 法师法力不足 | 双方 HP 不变 |
| 7 | 骑士命中 | 法师剩 2 HP |
| 8 | 法师法力不足 | 双方 HP 不变 |
| 9 | 骑士暴击并击败法师 | 实际扣除 2 HP；战斗结束 |

最后输出 `Winner: Knight | HP: 26:0`。

## 阅读与练习

先读 `main → duel`，跟随行动轮换，再读 `strike → print_result` 理解计算和结果拆解。
最后阅读 `self_test`，核对物理与魔法差异、命中和暴击边界、零伤害、过量伤害和死亡限制。

1. 修改 `Roll` 序列，分别触发普通命中、未命中和暴击，先手算双方最终 HP。
2. 将 `print_result` 的 `const auto&` 换成 `auto`，分析 `Result::state` 是否发生复制。
3. 新增具有独立属性的 `PoisonAttack` 类型，扩展 `if constexpr` 分支，再添加对应战斗案例。

返回 [课程总目录](../../README.md)。
