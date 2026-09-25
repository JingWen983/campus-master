# T6 独立验证证据：B1 修复后不可绕过、产品接口可产出两组 128 hex、后端仍可编译

| 项 | 值 |
| --- | --- |
| 任务 | t6 —— 独立验证 B1 修复后不可绕过且后端仍可编译 |
| 承担人 | verifier（独立验证，非实现者） |
| attempt_id | `74965bb2-ce02-464d-a64d-73b869c92f09` |
| verdict | **pass**（B1 已修复且不可绕过；产品接口产出两组完整 128 hex 逐字节正确；后端全量编译链接 exit 0） |
| 记录时间 | 2026-09-19 11:4x (+08:00) |
| 环境 | g++/gcc `14.2.0 (MinGW-W64 x86_64-msvcrt-posix-seh)`、node `v24.16.0`、pwsh `7.6.5` |

## 0. 判据所绑定的修订（指纹锁定）

| 文件 | 行数 | SHA256 前缀 | mtime |
| --- | --- | --- | --- |
| `sha256.h`（被测实现） | 297 | `3E5C59DDC9AEB5EE` | 11:33:56 |
| `tests/pbkdf2_rfc7914_test.cpp` | 535 | `95026632E043C2D5` | 11:32:23 |

`sha256.h` 完整 SHA256 = `3E5C59DDC9AEB5EE22488A8058AA7BA89C479BDD054179048A4F26D3A00DFA22`（与队长锁定值一致）。
**所有判据均在本修订上执行，执行前后指纹复核一致（未变动）。** 本文件所述结论仅对该修订成立。

验证方式：**verifier 自写独立 harness**（`%TEMP%\verifier_t6_harness\t6_harness.cpp`），**不复用** `tests/` 下的仓库测试文件，**不引用**队长的探针 `cap_b1_probe`。期望值来源为 verifier 自己的 Node `crypto`（`pbkdf2Sync` / `createHmac`）复算 + RFC 正文，非任何他人转述。

---

## 1. 独立 harness 总结果（主判据）

```
checks=57 fail=0
RESULT: PASS
HARNESS EXIT CODE = 0
```

覆盖（全部 `[ok]`）：

| 组 | 检查内容 | 结果 |
| --- | --- | --- |
| RFC 7914 §11 v1 | `pbkdf2_sha256("passwd","salt",1,64)` 长度恰 128、全小写 hex、**逐字节相等** | 3/3 ok |
| RFC 7914 §11 v2 | `pbkdf2_sha256("Password","NaCl",80000,64)` 长度恰 128、全小写 hex、**逐字节相等** | 3/3 ok |
| 3 参兼容 | 3 参默认 → 64 hex；**恰等于** 4 参 `dkLen=64` 输出的前 64 hex；显式 `dkLen=32` == 3 参默认 | 3/3 ok |
| 多块边界 | `dkLen=33`(66 hex)、`dkLen=100`(200 hex, 4 块) 与 Node `pbkdf2Sync` 逐字节相等 | 2/2 ok |
| HMAC 原语 | RFC 4231 TC1/TC2/TC6/TC7（含 key>64B 压缩分支）与 Node `createHmac` 逐字节相等 | 4/4 ok |
| 口令参与派生 | 不同口令不同值；空口令 ≠ `"alpha"`；空口令仍得 64 hex；64B vs 65B 口令不同 | 4/4 ok |
| 参数显式拒绝 | `iterations=0`/`-1`、`dkLen=0`/`-5`、组合 → 空串 | 5/5 ok |
| 存储格式 | 前缀 `pbkdf2$100000$`、恰 3 个 `$`、iters=100000、salt 32 hex、dk 64 hex、两次 salt 不同 | 6/6 ok |
| `verify_password` | 正确=1、错误=0、空口令=0 | 3/3 ok |
| 对抗性畸形输入 | 18 类全部 fail-closed（见 §5） | 18/18 ok |
| 「空串==空串」专项 | `iters=0` + 空 dk，传空口令/任意口令**均不得恒真** | 2/2 ok |
| 合法非默认 iters | `pbkdf2$1$…$<合法 dk>`：正确口令 true、错误口令 false | 2/2 ok |
| legacy 分支 | 正确口令 true、错误口令 false | 2/2 ok |

### 1.1 RFC 4231 交叉验证的价值
RFC 4231 是 HMAC-SHA256 的权威测试向量，**与 PBKDF2 向量独立**：它单独钉死 `hmac_sha256()` 的实现（key 压缩、`resize(64,'\0')`、ipad/opad 异或、内外层顺序）。TC6/TC7 的 key 为 131 字节（> 64），专门覆盖 `K = SHA256(key)` 压缩分支。该原语此前**无任何测试**，本项是本次唯一对它的独立覆盖。

---

## 2. 仓库脚本绿灯（**辅助证据，不作为关闭依据**）

```powershell
cd D:\邵敬文\comptation
pwsh -NoProfile -File tests/run_tests.ps1
```

```
g++ exit code = 0
cases : 11 run, 11 ok, 0 failed
checks: 36 run, 36 ok, 0 failed
RESULT: PASS
<<< test exit code = 0
```

**为什么不以此关闭 B1**：T3 已用变异体实验证伪过该门槛的充分性（一个「结构正确但不支持多块」的实现可让 T2 原版断言 10/10、32/32 全绿）。修复后虽然新增了编译期门禁与 `rfc7914_dklen64_from_production`，但**脚本绿灯仍需 §1 的独立 harness 与 §3/§4 的对抗性对照来补强**，故本文件将其标为辅助证据。

---

## 3. 编译期向量门禁的独立校准（队长要求我复现第二次）

方法：变异体**不写入仓库 `tests/`**（T3/T6 契约均禁止改动该目录），改为在仓库外建**保留 `../` 相对层级**的隔离副本（`<T>\tests\<测试文件>` + `<T>\sha256.h`），`#include "../sha256.h"` 照常解析；编译参数与 `run_tests.ps1` 逐字一致。

| 变异体 | 改动 | 编译结果 | 关键诊断 |
| --- | --- | --- | --- |
| `A3_V2_clean` | `RFC7914_V2_DK64` 截断为**恰好 64 hex**（语法合法） | **exit 1**，未生成二进制 | **仅一条**：`static assertion failed: RFC 7914 §11 向量 2 必须是 128 个 hex 字符（dkLen=64）；被截断的向量在此即被拒`（第 105 行）。此时 **T1 前缀断言仍通过** → 长度门与前缀门互相独立 |
| `B_V1_truncated` | `RFC7914_V1_DK64` 截断为 64 hex | **exit 1** | **两条**同时触发：长度门 + `向量 1 的 T_1 必须是 DK64 的前 64 个 hex 字符` |
| `C_unmutated_control` | 未变异 | **exit 0** | 对照通过，证明失败来自变异而非环境 |

> 方法论自我纠错留档：我最初的 A/A2 版变异体把行尾 `;` 一并删掉，编译器报的是语法错误而非 `static_assert`，**不能作为门禁校准证据**；该两版已删除，结论以 `A3`（干净截断）为准。

**结论**：坏向量（被截断）**连编译都过不去**，「长度=128」已成编译期硬约束。

---

## 4. `rfc7914_dklen64_from_production` 是「活门」而非「死门」——正/负向对照

| 对照 | 改动（在隔离副本内，仓库零改动） | 结果 |
| --- | --- | --- |
| **正向** `R_verbatim_current` | 原样当前 `sha256.h` + 原样测试文件 | 编译 exit 0；`cases : 11 run, 11 ok, 0 failed` / `checks: 36 run, 36 ok, 0 failed` / **exit 0**；`rfc7914_dklen64_from_production` → `[ OK ] (4 checks)` → **T4 的 4 参入口确实让该用例变绿，不是死门** |
| **负向 N1** | 把 `out.resize((size_t)dkLen)` 改为 `out.resize(32)`（**即"只支持单块"的 T4**） | 编译 exit 0；`cases : 11 run, 10 ok, 1 failed` / `checks: 36 run, 32 ok, 4 failed` / **exit 1**；4 条失败全在 `rfc7914_dklen64_from_production`（两条长度断言 + 两组向量逐字节），`actual  : 55ac046e…c20dacbc`（仅 64 hex） |
| **负向 N2** | 把 `INT32BE(i)` 的 `i` 改为恒 `1`（**块序号不递增**） | 编译 exit 0；`cases : 11 run, 10 ok, 1 failed` / `checks: 36 run, 34 ok, 2 failed` / **exit 1**；两组向量均失败，`actual` 恰为**两块相同**（`55ac…dacbc55ac…dacbc`） |

→ **T3 发现的覆盖漏洞已闭合**：既拦得住「只出单块」，也拦得住「块计数不递增」，且对正确实现放行。

---

## 5. 对抗性 `verify_password` 边界（18 类，全部 fail-closed）

以 `verify_password("anything", <畸形哈希>)` 断言必须为 `false`：

1. 空哈希 `""`
2. 仅前缀 `"pbkdf2$"`
3. `iters=0` + 空 dk
4. `iters=0` + `dk=deadbeef`
5. `iters=-1`
6. `iters` 非数字 `abc`
7. `iters` 前导 0 且 10 位（`0000100000`）
8. `iters` 超上限（`10000001`）
9. `iters` 10 位（`1234567890`）
10. 缺 dk 字段（只有 2 个 `$`）
11. 空 salt
12. dk 非十六进制（`zzzz`）
13. dk 大写 hex
14. dk 只有 63 位
15. dk 有 65 位
16. dk 空但 iters 合法（`pbkdf2$100000$<salt>$`）
17. 完全不是哈希
18. 合法长度但错误 dk

**「空串==空串」隐患专项**（我 ④ 提出的风险点，独立复现确认不成立）：`iters=0` 会使 `pbkdf2_sha256` 返回空串，若比较逻辑写成「空==空 → true」即成为新的认证绕过。实测 `verify_password("", "pbkdf2$0$00112233445566778899aabbccddeeff$")` 与 `verify_password("x", 同一串)` **均为 false**，两道防线（`iters<=0` 早退 + `dk.size() != 64` 长度断言）各自独立拦下，隐患不存在。

---

## 6. V1-8：主进程编译（后端仍可编译）

命令与 `.github/workflows/ci.yml:70-88` / 契约 §术语**逐字同源**；产物输出到 `%TEMP%`（`verifier_t6_build`），**未污染仓库**：

```
gcc -c sqlite3.c -o <TEMP>\sqlite3.o -O2                                  exit=0
g++ -c main.cpp|models.cpp|logger.cpp|routes_static.cpp|routes_public.cpp|
       routes_admin.cpp|routes_teacher.cpp|routes_student.cpp|routes_parent.cpp
       -o <TEMP>\<src>.o -std=c++17 -O2 -I.                               exit=0 (全部 9 个)
g++ -o <TEMP>\server.exe <10 个 .o> -lws2_32 -lwsock32 -std=c++17 -O2 \
       -static -static-libgcc -static-libstdc++ -lwinpthread              exit=0
server.exe exists=True bytes=5723110
```

**10/10 编译步骤 exit 0、链接 exit 0、`server.exe` 生成（5,723,110 B）** → V1-8 满足。
附带结论：3 参默认参数的向后兼容在**真实调用点**上成立（`routes_public.cpp:45`、`routes_parent.cpp:73`、`hash_password` 均随 9 个源文件一起编译通过）。

---

## 7. 常量时间比较：一致性与严重度独立判定

### 事实
- `pbkdf2$` 分支（`sha256.h:249-252`）使用累加式比较：`for (...) diff |= (calc[i] ^ dk[i]); return diff == 0;` → **常量时间**。
- legacy 分支（`sha256.h:255`）`return sha256(password) == hash;` → 走 `std::string::operator==` → `char_traits::compare` → `memcmp`，**首个不同字节即返回**，非常量时间。
- **可达性（我自行核对）**：`routes_public.cpp:45` 与 `routes_parent.cpp:73` 都在**未认证**的登录分支调用 `verify_password`；攻击者提交的哈希若为裸 SHA256 存储串即走此分支。
- **是否为 T4 引入**：**不是**。`git show HEAD:sha256.h` 第 187 行同样是 `return sha256(password) == hash;` → 该比较是**修复前就存在**的，T4 在把新分支做成常量时间时**沿用了旧分支的原写法**。
- **项目既有风格**：`auth.h:166` 的 `csrf_check()` 自己就是用累加式常量时间比较（`diff |= (cookie_token[i] ^ it->second[i])`）→ 常量时间才是本仓库安全修复后的既定约定，legacy 行是**风格上的孤例**。

### 我的独立判定：**low**（记录，不阻断 B1 关闭）
理由：
1. **收益极低**：时间侧信道至多泄露「legacy 存储串」本身，而它是**无盐单轮 SHA256**（B2 的 `admin123/teacher123/student123/parent123` 已被 verifier 实测原像）。攻击者无需侧信道即可离线字典/彩虹表恢复；
2. **不泄露口令**：泄露的是哈希，口令仍只能靠猜，而在线猜测本就可用登录成功与否判定；
3. **影响面在收敛**：T1 决策① 的「存量一律失效 + 强制重置」正是把 legacy 账号清零的路径；
4. **非本次回归**：HEAD 已如此，T4 未加重风险。
→ 结论：**low**，属「不一致 + 纵深防御」问题。若团队要修，建议走 **repair 流程**（避免绕过验证链）而不是让已交付的 T4 顺手改；修法是把 legacy 分支也换成同一累加式比较（约 3 行，行为等价，`sha256(password)` 仍为 64 hex）。

---

## 8. 交下游的观察（不属 T6 判据，但需有人认领）

| # | 观察 | 证据 | 建议归属 |
| --- | --- | --- | --- |
| 1 | `routes_public.cpp:46-51` 的「旧哈希登录成功后自动升级为 PBKDF2」链条**仍在**（T1 决策① 明确要求删除该链） | `routes_public.cpp:46-51` 现行代码；`git status` 显示该文件未被 T4 改动 | 疑属 T7（B2/存量重置）范围，请队长确认归属 |
| 2 | `verify_password` 未对**迭代数下限**设约束（接受 `pbkdf2$1$…`，我的 harness 正是用它做廉价计时实验） | `sha256.h:237` 只判 `iters <= 0` 与上限 1e7 | informational：哈希串在服务端存储，攻击者无法改写；若未来有导入路径需注意 |

---

## 9. 合规与未修改声明

- 未修改 `tests/` 下任何文件、未修改 `sha256.h` 及任何业务源码；`git status --short` 仅：` M .gitignore`、` M frontend/src/mock/index.ts`、` M frontend/src/pages/teacher/TeacherApp.vue`、` M sha256.h`（T4 的改动）、`?? .agent-teams/`、`?? docs/audit/`、`?? tests/`。**其中没有一处是 verifier 写入的源码**；verifier 本任务唯一写入的仓库文件是本文件。
- 所有变异体、harness、编译产物均在仓库外 `%TEMP%`，清单：
  - `%TEMP%\verifier_t6_harness\`：自写 harness（`t6_harness.cpp` / `.exe`）
  - `%TEMP%\verifier_t6_gate_calib\`：`A3_V2_clean`、`B_V1_truncated`、`C_unmutated_control`、`R_verbatim_current`、`N1_single_block_only`、`N2_block_index_always_1`
  - `%TEMP%\verifier_t6_build\`：V1-8 的 10 个 `.o` 与 `server.exe`（5,723,110 B）
  - 已删除：基于过期行切片而不可信的 4 个副本（`P_correct`、`N_wrong_multiblock`、`A_V2_truncated`、`A2_V2_clean_truncation`）与全部临时 `.js`
- 仓库根**未生成** `server.exe`/`.o`（本任务全部输出到 `%TEMP%`）；既有 `server.exe`（5,676,287 B）与 `sqlite3.o` 未被改动。
- 未执行 `git commit` / `git push`。

---

## 10. 非结论性附注

- 计时粗筛（20000 次/组，含 PBKDF2 固定开销、未随机化次序，**不作为常量时间结论**）：dk 首字节不符 `1433 ns/次`、末字节不符 `1425 ns/次`、全等命中 `1475 ns/次`。三者同量级，**未见首字节提前返回的量级差异**，与 §7 的代码级观察一致（常量时间分支）。该实验**不能**用于证明 legacy 分支的时序特性，§7 对 legacy 的判定基于**代码结构 + 可达性 + 影响面**，不基于计时。
