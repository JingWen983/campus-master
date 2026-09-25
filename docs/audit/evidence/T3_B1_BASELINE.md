# T3 独立验证证据：B1（PBKDF2 完整认证绕过）缺陷成立且基线必然失败

| 项 | 值 |
| --- | --- |
| 任务 | t3 —— 独立复核 B1 缺陷与『修复前基线必然失败』 |
| 承担人 | verifier（独立验证，非实现者） |
| attempt_id | `b1c03ba7-9713-4cfe-9706-46788089e8ed` |
| verdict | **pass**（缺陷被运行时复现；T2 测试脚本可编译可运行，失败退出码 1） |
| 记录时间 | 2026-09-19 11:30 (+08:00) |
| 被测对象 | `sha256.h`（229 行，**未修改**）、`tests/pbkdf2_rfc7914_test.cpp`（438 行）、`tests/run_tests.ps1`（111 行） |
| 环境 | g++ `14.2.0 (MinGW-W64 x86_64-msvcrt-posix-seh)`、node `v24.16.0`、pwsh `7.6.5`、工作目录 `D:\邵敬文\comptation` |

> **本文档性质**：verifier 的**独立证据归档**。所有命令由 verifier 亲自重跑，引用的输出均为 verifier 自己终端的原文；**不转述** T2/T4 或队长的结论。凡与外部标准对照的期望值，除引用 RFC 正文外，均附 verifier 自己的机械复算。

---

## 0. 结论摘要

1. **B1 缺陷成立**（运行时复现，非静态推导）：`verify_password()` 对错误口令、空串、单字节 `0x01` 全部返回 `true`；同一 salt 下 `pbkdf2_sha256` 对 `"a"`/`"b"`/`""` 输出完全相同。
2. **T2 的测试脚本确实能拦住 B1 的现状**：未修复 `sha256.h` 上退出码 **1**（5/10 用例、7/32 断言失败）。
3. **但 T2 的回归网对「是否支持多块 dkLen」判别力为 0**：变异体实验证明，一个「只修缺陷 A/B/C、结构正确、**无 dkLen 参数**、恒返回单块」的实现让原样测试文件 **10/10 用例、32/32 断言全绿、退出码 0**。
4. 因此 **T4 必须让产品接口支持显式 dkLen，T6 必须走产品接口对 RFC 7914 §11 两组向量做完整 128 hex 逐字节断言**；只以「`run_tests.ps1` 退出 0」作为 B1 关闭依据是**无效**的。

---

## 1. 基线必然失败（验收判据一）

```powershell
cd D:\邵敬文\comptation
pwsh -NoProfile -File tests/run_tests.ps1
```

verifier 终端原始输出（节选，逐字）：

```
>>> C:\mingw64\bin\g++.exe tests/pbkdf2_rfc7914_test.cpp -o tests/pbkdf2_test.exe -std=c++17 -O2 -I.    (cwd=D:\邵敬文\comptation)
<<< g++ exit code = 0

>>> D:\邵敬文\comptation\tests\pbkdf2_test.exe
=== tests/pbkdf2_rfc7914_test.cpp ===
--- [RUN ] sha256_fips180_4_primitive
--- [ OK ] sha256_fips180_4_primitive  (2 checks)
--- [RUN ] rfc7914_v1_t1_passwd_salt_c1
      [FAIL] rfc7914_v1_t1_passwd_salt_c1: pbkdf2_sha256("passwd","salt",1) == RFC 7914 §11 v1 的前 32 字节
             expected: 55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc
             actual  : 3030386166383038356366363965356134653666366164363136393332616366
--- [FAIL] rfc7914_v1_t1_passwd_salt_c1  (1 of 2 checks failed)
--- [RUN ] rfc7914_v2_t1_Password_NaCl_c80000
      [FAIL] rfc7914_v2_t1_Password_NaCl_c80000: pbkdf2_sha256("Password","NaCl",80000) == RFC 7914 §11 v2 的前 32 字节
             expected: 4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56
             actual  : 01040d5b5102015756010a040f5354040e5c5d595b0f04045006505c5f0b585f
--- [FAIL] rfc7914_v2_t1_Password_NaCl_c80000  (1 of 2 checks failed)
--- [RUN ] rfc7914_dklen64_block_counter
--- [ OK ] rfc7914_dklen64_block_counter  (2 checks)
--- [RUN ] b1_distinct_password_distinct_dk
      [FAIL] b1_distinct_password_distinct_dk: 同一 salt 下 pbkdf2_sha256("alpha-pw") != pbkdf2_sha256("beta-pw")
             expected: <两个不同的值>
             actual  : 两者相同: 52065a510c540d010f07065f09075b0f5e51060b08550e580b0055010b580c0c
      [FAIL] b1_distinct_password_distinct_dk: 同一 salt 下 pbkdf2_sha256("") != pbkdf2_sha256("alpha-pw")
             expected: <两个不同的值>
             actual  : 两者相同: 52065a510c540d010f07065f09075b0f5e51060b08550e580b0055010b580c0c
--- [FAIL] b1_distinct_password_distinct_dk  (2 of 3 checks failed)
--- [RUN ] b1_password_len_64_vs_65
      [FAIL] b1_password_len_64_vs_65: 口令 64 字节与 65 字节必须派生不同 dk（65 字节走 K=SHA256(key) 分支）
             expected: <两个不同的值>
             actual  : 两者相同: 0b5851510f06085458590e5509020d0a08585452580f515a5d545354515c5451
--- [FAIL] b1_password_len_64_vs_65  (1 of 2 checks failed)
--- [RUN ] b1_verify_password_wrong_password_false
      [FAIL] b1_verify_password_wrong_password_false: verify_password(错误口令, 同一存储串) == false
             expected: false
             actual  : true
      [FAIL] b1_verify_password_wrong_password_false: verify_password(空口令, 同一存储串) == false
             expected: false
             actual  : true
--- [FAIL] b1_verify_password_wrong_password_false  (2 of 3 checks failed)
--- [RUN ] verify_password_malformed_fail_closed
--- [ OK ] verify_password_malformed_fail_closed  (8 checks)
--- [RUN ] hash_password_storage_format
--- [ OK ] hash_password_storage_format  (6 checks)
--- [RUN ] verify_password_legacy_sha256
--- [ OK ] verify_password_legacy_sha256  (2 checks)

============================================================
cases : 10 run, 5 ok, 5 failed
checks: 32 run, 25 ok, 7 failed
RESULT: FAIL
<<< test exit code = 1
```

**退出码**：测试进程 `1`；包装脚本 `tests/run_tests.ps1` 亦为 `1`（verifier 在调用外层用 `$LASTEXITCODE` 核实）。
**判定**：脚本本身可编译（g++ exit 0）、可运行（非崩溃/非环境错误），失败是**真实断言失败**，满足契约 §3.1 V1-9「修复前基线必然失败」。

---

## 2. B1 两条机制的逐条判定（验收判据二）

### 机制① 派生值只由 (salt, iters) 决定，与 password 无关 —— 成立

verifier 自写最小程序（源码 `%TEMP%\...\verifier_t3_minprog\min.cpp`，g++ exit 0）：

```cpp
std::string a = pbkdf2_sha256("a","s",1);
std::string b = pbkdf2_sha256("b","s",1);
std::string e = pbkdf2_sha256("","s",1);
```

原始输出：

```
pbkdf2_sha256("a","s",1) = 3934303962646231613365343266626336386430313334626439663031396439
pbkdf2_sha256("b","s",1) = 3934303962646231613365343266626336386430313334626439663031396439
pbkdf2_sha256("","s",1)  = 3934303962646231613365343266626336386430313334626439663031396439
a==b ? 1   a==empty ? 1
```

同一 salt 下**三个不同（含空）口令输出完全相同** → 派生值不含口令信息。

补充 harness（`%TEMP%\...\verifier_b1_baseline\b1_baseline.cpp`，g++ exit 0）：

```
[baseline] stored=pbkdf2$100000$e4d6ff708055eac634e35f8c790213a0$540f0a5b5b5958550205010c5f545d0d5f0a540c0e550f5a015c0f52515e0f0e
[baseline] verify(correct)=1
[baseline] verify(wrong)  =1
[baseline] verify(empty)  =1
[baseline] verify('\x01')   =1
```

### 机制② 内层被错误实现为 `SHA256(k_ipad || salt || INT32BE(1))` —— 成立（独立复刻，逐字节相等）

verifier **不看代码注释、直接按结构复刻**：以 Node 重算「key = salt（补零至 64 字节）；内层 `SHA256(k_ipad||salt||00000001)`；外层 `SHA256(k_opad||X)` 同错；`memcpy(u, h1.data(), 32)` 取 hex 文本前 32 字符；末尾对结果再 hex 编码」：

```
我的独立复刻 (salt='s', iters=1) = 3934303962646231613365343266626336386430313334626439663031396439
产品实测                          = 3934303962646231613365343266626336386430313334626439663031396439
逐字节相等 : true
```

复刻脚本与产品实测**逐字节相等** → 该结构判定成立（脚本为临时文件，已删除；重跑方式见 §5）。

### 叠加缺陷（不改变上述两条判定）

返回值形如 `3030386166…`，外层 hex 解码后为 `008af8085cf69e5a4e6f6ad616932acf`（32 字符，全 hex）——
即实现把 `sha256()` 的 **hex 文本**当字节使用（`memcpy(...,32)` 只取 16 字节摘要），末尾又 hex 编码一次，构成**双重 hex + 摘要截断到 16 字节**。这是独立于机制①②的第三处缺陷（契约 V1-2b 记为缺陷 C）。

---

## 3. 『password 形参零引用』机械证据（验收判据三）

**区间统计**：`sha256.h` 第 **94–148** 行为 `pbkdf2_sha256()` 函数体（第 94 行为声明、第 148 行为闭合花括号）。在该区间内：

| 标识符 | 出现次数 | 说明 |
| --- | --- | --- |
| `password` | **1** | 即第 94 行的形参声明本身，**函数体内零引用** |
| `salt` | 6 | 被用作 HMAC key 与消息的一部分 |

**行为证据**：§2 机制① 的最小程序（不同口令 → 相同输出）即为该机械事实的运行时印证。

---

## 4. 提权链可达性与工作区数据库扫描（验收判据四）

verifier 自行打开 `routes_public.cpp` 现网登录分支，逐行核对：

| file:line | 代码 | 作用 |
| --- | --- | --- |
| `routes_public.cpp:45` | `bool password_match = verify_password(password, user->password_hash);` | 校验（受 B1 影响恒真） |
| `routes_public.cpp:47` | `if (password_match && user->password_hash.compare(0, 7, "pbkdf2$") != 0) {` | 命中旧哈希 |
| `routes_public.cpp:48` | `std::string new_hash = hash_password(password);` | 用坏实现重算 |
| `routes_public.cpp:49` | `user->password_hash = new_hash;` | 就地替换 |
| `routes_public.cpp:50` | `save_user_to_db(*user);` | 落库 |

**行为确实存在**。叠加 B1 与 B2（种子口令原像由 verifier 实测命中：`admin123` / `teacher123` / `student123` / `parent123`，来源 `models.cpp:18-21` 与 `main.cpp:251-254`）：**一次正常登录即把可猜口令账号永久换成「任意口令（含空串）均可通过」的坏哈希**。

**`.db` 扫描命令与结果**：

```powershell
cd D:\邵敬文\comptation
Get-ChildItem -Recurse -File -Include *.db,*.db-shm,*.db-wal,*.sqlite,*.sqlite3 |
  Where-Object { $_.FullName -notmatch '\\vcpkg\\' -and $_.FullName -notmatch '\\node_modules\\' }
```

→ **无任何输出（0 个文件）**。顶层 `*.log` 仅 `server.log`（5,154,608 字节，未逐行审计）。
**结论**：工作区无存量数据库 → **无法枚举存量受影响账号**，B1 存量影响面只能用「一律失效 + 强制重置」策略处理（T1 决策①）。

---

## 5. 变异体实验：T2 回归网对「多块 support」判别力为 0（本任务最关键证据）

### 5.1 设置

1. 取仓库 `sha256.h` 的 1–93 行 + **verifier 手写的"只修缺陷 A/B/C"实现** + 149–229 行 → `%TEMP%\verifier_t3_mutant\sha256.h`（211 行，8,664 B）。
   mutant 实现特征：HMAC key 取 `password`；使用**原始摘要字节**而非 hex 文本；ipad/opad 结构正确；**但签名仍是 `pbkdf2_sha256(password, salt, iterations)`，没有 `dkLen` 参数，恒返回单块 32 字节**。
2. 把仓库 `tests/pbkdf2_rfc7914_test.cpp` **原样**（21,318 B，未改一个字节）拷到 `%TEMP%\verifier_t3_mutant\tests\`。
3. 编译运行：

```powershell
& C:\mingw64\bin\g++.exe "$T\tests\pbkdf2_rfc7914_test.cpp" -o "$T\tests\mutant_test.exe" -std=c++17 -O2
& "$T\tests\mutant_test.exe"
```

### 5.2 结果（verifier 终端原文摘要）

```
mutant compile exit=0
--- [RUN ] sha256_fips180_4_primitive
--- [ OK ] sha256_fips180_4_primitive  (2 checks)
--- [RUN ] rfc7914_v1_t1_passwd_salt_c1
--- [ OK ] rfc7914_v1_t1_passwd_salt_c1  (2 checks)
--- [RUN ] rfc7914_v2_t1_Password_NaCl_c80000
--- [ OK ] rfc7914_v2_t1_Password_NaCl_c80000  (2 checks)
--- [RUN ] rfc7914_dklen64_block_counter
--- [ OK ] rfc7914_dklen64_block_counter  (2 checks)
--- [RUN ] b1_distinct_password_distinct_dk
--- [ OK ] b1_distinct_password_distinct_dk  (3 checks)
--- [RUN ] b1_password_len_64_vs_65
--- [ OK ] b1_password_len_64_vs_65  (2 checks)
--- [RUN ] b1_verify_password_wrong_password_false
--- [ OK ] b1_verify_password_wrong_password_false  (3 checks)
--- [RUN ] verify_password_malformed_fail_closed
--- [ OK ] verify_password_malformed_fail_closed  (8 checks)
--- [RUN ] hash_password_storage_format
--- [ OK ] hash_password_storage_format  (6 checks)
--- [RUN ] verify_password_legacy_sha256
--- [ OK ] verify_password_legacy_sha256  (2 checks)

============================================================
cases : 10 run, 10 ok, 0 failed
checks: 32 run, 32 ok, 0 failed
RESULT: PASS
=== mutant test exit code = 0 ===
```

### 5.3 判定

**一个没有 `dkLen` 参数、永远只能产出 32 字节（单块）的产品实现，可以让 T2 的测试套件 100% 全绿。**
即：**T2 现有断言对「是否支持多块 / dkLen=64」的判别力为 0。**

**锋利旁证**：用例 `rfc7914_dklen64_block_counter` 在**未修复的坏实现**上也是 `[ OK ]`（见 §1 原始输出），在 mutant 上同样 `[ OK ]` —— 该断言只验证了测试文件自带的参考实现，对产品函数 `pbkdf2_sha256()` 零信息量。

**artifact 路径（仓库外，未污染仓库）**：
- `%TEMP%\verifier_t3_mutant\sha256.h`（8,664 B，211 行）
- `%TEMP%\verifier_t3_mutant\tests\pbkdf2_rfc7914_test.cpp`（21,318 B，原样拷贝）
- `%TEMP%\verifier_t3_mutant\tests\mutant_test.exe`（144,745 B）
- 解析后的实际前缀：`C:\Users\Administrator\AppData\Local\Temp\dsh-Iom8tV\verifier_t3_mutant\`
- 另：`%TEMP%\...\verifier_t3_minprog\`（§2 最小程序）、`%TEMP%\...\verifier_b1_baseline\`（§2 harness）

---

## 6. 门槛结论（交 T4 / T6 的硬要求）

> **产品接口必须能吃下 128 hex。**

1. **T4 必须让产品接口支持显式 `dkLen`**（例如 `pbkdf2_sha256(password, salt, iterations, dkLen)`，默认 32 以保持既有 3 参调用——`hash_password()`、`routes_parent.cpp:73`、`routes_public.cpp:45` 等——不被破坏），并实现真正的多块（`ceil(dkLen/32)` 块，`INT32BE(i)` 随块递增）。
2. **T6 的验收必须走产品接口**（`pbkdf2_sha256(pw, salt, c, 64)`）对 RFC 7914 §11 两组向量断言**完整 128 hex 逐字节相等**；**不接受**用测试文件内的参考实现替代产品实现。
3. **不得**以「`tests/run_tests.ps1` 退出码 0」作为关闭 B1 的依据 —— §5 已用变异体实验证明该条件充要性不成立。
4. 建议同步收紧契约 V1-1 措辞：现文「`tests/run_tests.ps1` 中 RFC 7914 §11 两组向量逐字节相等…且脚本退出码为 0」可被**零覆盖形态**满足，应写明「128 hex 断言必须来自产品接口」。
5. T6 阶段 verifier 将**另写独立 harness**（不复用仓库测试文件）复算两组 128 hex、空串/错误口令拒绝、以及 bit 级比较，作为交叉证据。

---

## 7. 期望值本身的可信度（verifier 独立复核，防伪造/防循环论证）

**长度判据（最廉价证伪）**：从 `tests/pbkdf2_rfc7914_test.cpp` 机械提取全部 64 字符 hex 字面量（共 8 个），按出现顺序拼接：

| 拼接 | 长度 | 与权威值逐字节比较 |
| --- | --- | --- |
| `lit[0]+lit[1]`（RFC v1 dkLen=64） | **128** | `-ceq` RFC 7914 §11 第一组 = **True** |
| `lit[3]+lit[4]`（RFC v2 dkLen=64） | **128** | `-ceq` RFC 7914 §11 第二组 = **True** |
| `lit[2]`（v1 前 64 hex） | 64 | = RFC v1[0..63] = **True** |
| `lit[5]`（v2 前 64 hex） | 64 | = RFC v2[0..63] = **True** |
| `lit[6]` / `lit[7]` | 64 | = SHA-256(`""`) / SHA-256(`"abc"`) = **True** |

**权威出处**：verifier 自行 `web_fetch https://www.rfc-editor.org/rfc/rfc7914.txt`（HTTP 200），§11 正文两组向量与上述字面量逐字节一致；并自跑 Node `crypto.pbkdf2Sync` 复算两组（均 128 字符，一致）。

**非循环论证核对**：参考实现块 `tests/pbkdf2_rfc7914_test.cpp:160-196` 内 `pbkdf2_sha256` 出现 **0** 次（不调用产品函数）；产品调用仅 `:218`、`:228`，且只断言 T_1（前 64 hex）。文件仅 1 条 `#include`（`:66 #include "../sha256.h"`），出自有声明为 RFC 7914 §11（`:10-13`），`6070` 仅出现在 `:22-23` 的否定性说明中。

---

## 8. 合规性与未修改声明

- **未修改** `tests/` 下任何文件；**未修改** `sha256.h` 及任何业务源码。
- 本次运行后 `git status --short`：

```
 M .gitignore
 M frontend/src/mock/index.ts
 M frontend/src/pages/teacher/TeacherApp.vue
?? .agent-teams/
?? docs/audit/
?? tests/
```

- 仓库内 `tests/pbkdf2_test.exe`（140,965 B）是 `tests/run_tests.ps1` 本次运行生成的编译产物，已被 `.gitignore:40 tests/*` 忽略（`git check-ignore -q` 退出码 0），verifier 按原样保留未删。
- 可跟踪性核实（`git check-ignore -q` 退出码）：`tests/pbkdf2_rfc7914_test.cpp` → **1（可跟踪）** 命中 `:41 !tests/*.cpp`；`tests/run_tests.ps1` → **1（可跟踪）** 命中 `:42`；对照组 `tests/anything_else.txt` → 0（被忽略）。
- 临时文件全部位于仓库**之外**（`%TEMP%`），已在 §5.3 逐一标注；本次分析用的两个临时 `.js` 脚本已删除。

---

## 9. 低严重度备注（不影响 verdict，不阻断下游）

| # | 内容 | 严重度 |
| --- | --- | --- |
| 1 | `tests/pbkdf2_rfc7914_test.cpp:29` 称契约初版字面量为 95 字符，而契约 §3.1:127 与 §8:679 记为 96 字符；原字面量已被覆盖，现无法复核（不影响向量正确性） | low |
| 2 | `tests/pbkdf2_rfc7914_test.cpp:34-38` 自述「四个独立来源（含 .NET `Rfc2898DeriveBytes`、CPython `hashlib`）」，未留可复现命令/记录；verifier 仅独立证实其中两源（RFC 正文 + Node `crypto`） | low |
| 3 | 团队质量门禁 `pathMatchesScope()` 不支持 glob，`inScope` 里的 `tests/**` 无法匹配任何文件 → 后续任务 `inScope` 应写 `tests/`（尾斜杠目录前缀） | informational（流程） |

---

## 10. 修订说明（本归档写出后发生的仓库变更，防止误读）

本文档 §1–§9 记录的是 **T3 时点（修复前）** 的被验证修订：

| 文件 | T3 时点修订 | 之后的状态 |
| --- | --- | --- |
| `sha256.h` | 229 行，含 B1 四缺陷叠加 | 已由 T4 重写为 **297 行**（新增 `sha256_bytes()`、真正的 `hmac_sha256()`、可配 `dkLen` 的 4 参入口、fail-closed `verify_password`） |
| `tests/pbkdf2_rfc7914_test.cpp` | 438 行，10 用例 / 32 断言 | 已追加**编译期门禁**（`constexpr` 向量 + `static_assert`）与用例 `rfc7914_dklen64_from_production` → **535 行**，11 用例 |

因此：

- **§1 的基线数字（`cases : 10 run, 5 ok, 5 failed` / `checks : 32 run, 25 ok, 7 failed`）是修复前口径**。加入新用例后的修复前口径为 `cases : 11 run, 5 ok, 6 failed` / `checks : 33 run, 25 ok, 8 failed`（verifier 于同日本机实测，脚本退出码 1）。**两者不可混比**。
- **§5 的变异体实验结论依然成立**：它判定的是「T2 交付时点的回归网对多块能力零判别力」，与后续是否加门禁无关（该结论正是新增编译期门禁与 4 参用例的动因）。
- 修复后的验证取证由 **T6** 在重新锁定修订指纹后独立完成，不在本文档内。
- 修复后（T4 落地）修订指纹（供 T6 对齐，verifier 于 2026-09-19 实测）：`sha256.h` 297 行 / 13,143 B / SHA256 前缀 `3E5C59DDC9AEB5EE` / mtime `11:33:56`；`tests/pbkdf2_rfc7914_test.cpp` 535 行 / 27,752 B / SHA256 前缀 `95026632E043C2D5` / mtime `11:32:23`。
