# C++17 综合练习

| 练习 | 对应主线 | 内容 |
|---|---|---|
| [vending_machine.cpp](vending_machine.cpp) | 第 11～17 课，7 个知识点 | 迷你自动售货机：投入金额、购买商品、退回余额 |
| [battle_simulator.cpp](battle_simulator.cpp) | 第 11～13 课，3 个知识点 | 骑士与法师的回合制战斗结算 |

两份练习都各自是一个独立 `.cpp`，直接编译即可。学完第 17 课后，推荐阅读售货机案例。

## 第 11～17 课：迷你自动售货机

完整程序在 [`vending_machine.cpp`](vending_machine.cpp) 中，不超过 200 行，
包含中文注释、固定演示和内置自检。仅使用标准库。

售货机有 Tea 和 Water 两种商品。用户的每一步操作是投入金额、购买商品、退回余额中的一种。
程序按顺序处理固定指令，展示余额不足、购买成功、缺货、商品不存在和非法投入等情况。

### 7 个知识点的对应关系

| 课程 | 在案例中的用途 | 阅读入口 |
|---|---|---|
| 11：结构化绑定 | 按引用拆解商品编码、价格、库存，输出目录 | `show` |
| 12：if / switch 初始化语句 | 把查找结果限制在判断及分支作用域中 | `buy`、`stock` 中的 if 初始化语句 |
| 13：if constexpr | 按指令类型校验金额、商品编码，Refund 无须这些字段 | `valid<C>` |
| 14：折叠表达式 | 将字符串、金额、库存等不同类型拼接为一条日志 | `log` |
| 15：CTAD | 推导 `vector<Product>`、`vector<Command>`；推导合成访问器的闭包类型 | `demo`、`overloaded` 的推导指引 |
| 16：std::optional | 返回找到的商品下标，找不到时返回 `nullopt` | `find` |
| 17：std::variant / std::visit | 保存三类不同指令，选择当前指令的具体处理逻辑 | `Command`、`process` |

### 跟随一条指令阅读

从 `demo` 看商品和指令如何创建，然后跟随 `Buy{"Tea"}`：

```text
Command -> std::visit -> valid<Buy> -> dispatch -> buy -> find -> 扣余额、减库存
```

`std::visit` 把当前活动指令传给泛型 lambda，`valid` 在编译期选择适用字段的校验逻辑。
校验通过后，`overloaded` 重载集调用对应业务方法。三个分支的返回值都是 bool，
表示指令是否实际执行成功。这里采用两层结构，把参数校验与业务处理分别放在短函数中。

`find` 的 `optional<size_t>` 表达“有下标或找不到”。下标为 0 时 optional 仍然有值，
因此 `if (index)` 能正确找到第一个商品；它检查的不是下标数字是否非零。

### 规则和预期结果

- 金额统一使用整数“分”，Tea 单价 125，Water 单价 75。
- 单次投入允许 1～1000，余额上限 10000。
- 购买时检查商品存在、库存充足、余额足够；全部满足后才扣款并减库存。
- 退款退回当前全部余额并清零；空余额退款返回失败。
- 商品目录由代码创建，编码唯一，价格为正、初始库存非负。
- 处理失败时保留余额和库存；后续指令继续执行。

演示中先投入 100，购买 Tea 因余额不足失败；再投入 100，依次购买 Tea 和 Water，
余额刚好归零。随后投入的 200 在缺货和未知商品处理后全部退回。最后输出：

```text
Tea: price=125, stock=1
Water: price=75, stock=0
balance=0
accepted=6, rejected=5
Demo checks: PASS
```

从仓库根目录执行，以下为 PowerShell 命令：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic CPP17/practice/vending_machine.cpp -o CPP17/practice/vending_machine.exe
.\CPP17\practice\vending_machine.exe
.\CPP17\practice\vending_machine.exe --self-test
```

自检覆盖精确扣款、缺货、缺失商品、余额不足、非法投入、余额上限、退款和空目录。
退出码：`0` 为成功，`1` 为检查失败，`2` 为启动参数错误。控制台输出保持 ASCII。

### 小练习

1. 修改商品价格或指令顺序，先预测最终库存和余额，再同步调整演示中的检查。
2. 加入 `Restock{code, count}` 指令，分别修改 Command、valid 和 dispatch，并编写补货方法。
3. 将 `show` 的 `const auto&` 改成 `auto`，观察输出相同的情况下是否复制了商品数据。

## 第 11～13 课：回合制战斗结算器

适合学完第 11～13 课后阅读。完整程序在
[`battle_simulator.cpp`](battle_simulator.cpp) 中，不超过 200 行，包含中文注释和内置自检。

骑士和法师轮流行动：骑士使用物理攻击，法师施放魔法。程序结算命中、暴击、
法力消耗和 HP 变化，遇到角色被击败时结束战斗。使用固定判定值，便于先预测再运行。

### 三个知识点如何协作

| 课程 | 在练习中的作用 | 代码入口 |
|---|---|---|
| 11：结构化绑定 | 将攻击结果拆成伤害、暴击、击败、状态；拆解两名角色的最终快照 | `print_result`、`main` |
| 12：if / switch 初始化语句 | 将命中率限制在判断作用域内；计算当前行动阶段并立即分发 | `strike`、`duel` |
| 13：if constexpr | 物理攻击读取 `might` 和护甲；魔法攻击读取 `power`、抗性和法力消耗 | `strike<Attack>` |

`PhysicalAttack` 没有 `power`、`mana_cost`，`MagicAttack` 没有 `might`。
模板中的 `if constexpr` 让这些不同的成员访问各自在对应类型下实例化；
将其换成普通 `if` 会使两种类型都尝试编译不适用的成员访问。
命中与法力是否足够仍在运行时由普通 `if` 判断。

### 编译运行

从仓库根目录执行，以下为 PowerShell 命令：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic CPP17/practice/battle_simulator.cpp -o CPP17/practice/battle_simulator.exe
.\CPP17\practice\battle_simulator.exe
.\CPP17\practice\battle_simulator.exe --self-test
```

无参数运行 9 次行动的固定演示；`--self-test` 验证战斗规则，成功输出 `Self-tests: PASS`。
退出码：`0` 为成功，`1` 为自检或判定值错误，`2` 为启动参数错误。
输出保持 ASCII，中文说明只放在注释和本文中。

### 战斗规则

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

### 阅读与练习

先读 `main → duel`，跟随行动轮换，再读 `strike → print_result` 理解计算和结果拆解。
最后阅读 `self_test`，核对物理与魔法差异、命中和暴击边界、零伤害、过量伤害和死亡限制。

1. 修改 `Roll` 序列，分别触发普通命中、未命中和暴击，先手算双方最终 HP。
2. 将 `print_result` 的 `const auto&` 换成 `auto`，分析 `Result::state` 是否发生复制。
3. 新增具有独立属性的 `PoisonAttack` 类型，扩展 `if constexpr` 分支，再添加对应战斗案例。

返回 [课程总目录](../../README.md)。
