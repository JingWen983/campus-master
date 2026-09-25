# 批次 0 修复契约（BATCH0_CONTRACT）

- 任务：team `audit-batch0-remediation` 任务 **T1**（requirements-architect）
- 产物性质：**契约与判据文档**。本文件不修改任何源码、配置或构建产物。
- 权威依据（冲突时以序号靠前者为准）：
  1. `docs/audit/VERIFICATION.md`（独立复核报告，1037 行）
  2. `docs/audit/CODE_QUALITY_REVIEW.md`（尤其是 §6 批次 0 路线图与 §7.1 勘误表）
  3. `docs/audit/CODE_QUALITY_SUMMARY.md`（导航与 §三 批次 0 十条）
- 行号口径：本文件所有 `文件:行号` **均由我（requirements-architect）在本仓库当前工作区用 read/grep 打开核对**；与审计文档不一致处已在 §8 逐条记录，并以**本文件实测行号**为准。
- 基线快照（T1 执行日 **2026-09-19** 实测；审计评估日 2026-09-12 的结论与之一致）：
  - `git status --porcelain` → 仅 `?? .agent-teams/` 与 `?? docs/audit/`（源码零改动，与审计 §7.4 只读性自证一致）
  - 工作区**不存在任何 `.db/.db-wal/.db-shm/.sqlite/.sqlite3` 文件**（递归扫描为空）
  - `tests/` 目录**不存在**（`Test-Path tests` → False）→ 仓库当前仍是 0 个测试文件
  - `git ls-files | Select-String '^\.trae/'` → **25** 命中；`.trae/` 磁盘上实际有 26 个文件
- 术语：`主进程编译` = `gcc -c sqlite3.c -o sqlite3.o -O2` + 9 个 `g++ -c <src>.cpp -o <src>.o -std=c++17 -O2 -I.` + 链接 `g++ -o server.exe main.o models.o logger.o routes_static.o routes_public.o routes_admin.o routes_teacher.o routes_student.o routes_parent.o sqlite3.o -lws2_32 -lwsock32 -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -lwinpthread`（与 `.github/workflows/ci.yml:69-88` 逐字同源）。

---

## 1. 本轮唯一的顺序强约束（安全类硬绑定）—— 不得颠倒

审计结论（`CODE_QUALITY_SUMMARY.md` §三「安全类硬绑定」+ `CODE_QUALITY_REVIEW.md:546-557`）：

> **必须先把 B1 修好，再去改默认口令。** 「改密码」在 B1 存在时根本救不了场——B1 下改密写入的仍是与口令无关的坏哈希，任意口令照样能登录。

契约化后的强制顺序（每一步都是下一步的**前置门禁**）：

| 门 | 内容 | 任务 | 若要越过该门，必须先满足 |
|---|---|---|---|
| G1 | 先补标准向量测试（RFC 7914 §11），使其在未修复的 `sha256.h` 上**必然失败** | T2 | — |
| G2 | 独立复核「B1 缺陷成立 + 该测试确实能拦住它」 | T3 | G1 的失败输出被独立复现 |
| G3 | 按 RFC 8018 重写 PBKDF2，测试转全绿，后端仍可编译 | T4 | G2 verdict=pass |
| G4 | 首启随机口令 + 强制首登改密 + 去除可预测默认凭据 | T7 | G3 完成（否则「改密」无效） |
| G5 | F13（登录页预填按钮 + Release Notes 明文口令）随 B2 一并处置 | T7/T8 | G4 同一批次内，**不得推迟到后续批次** |

**F13 的严重度是 low，但 low ≠ 可延后** —— 它必须与 B1/B2 同批完成（`CODE_QUALITY_REVIEW.md:557`）。

### 1.1 必须与上表同读的交叉结论

> **不得用 F1 的存在降低 B1 的紧急度；修复顺序 B1 先于 F1。**

理由（`CODE_QUALITY_SUMMARY.md:121-127`、`CODE_QUALITY_REVIEW.md:129-133`、`VERIFICATION.md` §3.11）：开发版交付包因前端走 mock 而**恰好不触发** B1 的升级链，这是**两个缺陷相互掩盖**，不是任何一层被修好。正式发布包前端是真实的，一旦有人用 `admin/admin123` 正常登录一次，`routes_public.cpp:47-51` 立即把该账号哈希改写为坏 pbkdf2，此后**任意口令可登录 admin 且永久有效**。

---

## 2. 条目总表（批次 0 全 10 条）

| # | 条目 | 严重度 | 修复文件 | 实测位置（本文件口径） | 任务号 | 独立验证任务 |
|---|---|---|---|---|---|---|
| 1 | **B1** PBKDF2 完整认证绕过 | blocker | `sha256.h`（+ `routes_public.cpp` 升级链、`models.cpp` 兜底表） | `sha256.h:94-148`、`:169-188`；升级链 `routes_public.cpp:45-51`；兜底表 `models.cpp:17-22` | **T2→T3→T4** | T3（修复前）、T6（修复后） |
| 2 | **F1** CI 交付包前后端错配 | blocker | `.github/workflows/ci.yml`、`frontend/vite.config.ts` | `ci.yml:41-46`、`:54-59`、`:114-124`、`:135-140`；`vite.config.ts:12-15` | **T8** | T10 |
| 3 | **B13** 备份判据写反 | medium | `routes_admin.cpp` | `routes_admin.cpp:323-350`（判据在 `:341-347`） | **T9** | T10 |
| 4 | **N1** HTTPS 回退后 Cookie 仍带 Secure | medium | `main.cpp`（+ 核对 `config.h`/`auth.h`） | `main.cpp:311-315`；`config.h:79-85`；`auth.h:88,95,174` | **T9** | T10 |
| 5 | **F24** 无效请求头 `Content-Length` | medium | `frontend/src/lib/api.ts` | `api.ts:73-76`（语句在 `:74-75`） | **T8** | T10 |
| 6 | **F16** 发布不可复现 | medium | `.github/workflows/release.yml` | `release.yml:49-51` | **T8** | T10 |
| 7 | **B2** 种子默认口令 + 静默内存兜底 | high | `main.cpp`、`models.cpp`、`models.h`、`routes_public.cpp`、`config.h` | `main.cpp:250-254`、`:60-75`、`:77-283`；`models.cpp:17-22`；`sqlite_wrapper.h:19-32,190-192` | **T7** | T10 |
| 8 | **F4** 评价列恒「未评价」 | high | `frontend/src/pages/teacher/TeacherApp.vue` | `TeacherApp.vue:59-65`、`:168-171`、`:313-321`、`:634-637`、`:664-683`、`:1038-1081`、`:1087-1091` | **T5** | T10 |
| 9 | **B6** erase 后索引错位 | high | `models.cpp`、`models.h`、`routes_admin.cpp`、`routes_teacher.cpp` | `models.cpp:164-221`；erase `routes_admin.cpp:826-827`、`routes_teacher.cpp:259-260` | **T9** | T10 |
| 10 | **N4** `.trae/` 误入库 | low | Git 索引（25 个已跟踪文件） | `git ls-files \| Select-String '^\.trae/'` → 25；`.gitignore:78-79` | **T9** | T10 |

**全批次编排**：T2 → T3 → T4 → {T5, T7} → {T8, T9} → {T6, T10} → T11 → T12。

---

## 3. 逐条契约

> 每条固定六段：**位置（实测） / 根因 / 修复动作 / 验收判据（可机械判定） / 边界与反例 / 分工**。
> 「验收判据」全部写成能判 true/false 的命题，不含「尽量」「建议」这类不可判定措辞。

### 3.1 B1 — PBKDF2 完整认证绕过（blocker）

**位置（实测）**

- `sha256.h:94-148` `pbkdf2_sha256(password, salt, iterations=100000)`
- `sha256.h:169-188` `verify_password`
- `routes_public.cpp:45-51` 旧哈希登录成功后的**就地升级链**
- `models.cpp:17-22` 内存兜底用户表（同样是裸 SHA256 种子哈希）

**根因（两条彼此独立的缺陷，缺一不可）**

缺陷 A：**HMAC 的 key 用成了 `salt`**，`password` 形参在函数体内**零引用**。
- `sha256.h:101-108`：`k_ipad`/`k_opad` 由 `salt`（或 `sha256(salt)`）填充；
- `sha256.h:114-118`：内层消息体 `msg = k_ipad || salt || INT32BE(1)`；
- `sha256.h:121-125`：外层消息体 `msg2 = k_opad || u`；
- `sha256.h:129-142`：迭代段只用 `u` 与 `k_opad`，仍无 `password`。
- 机械证据（三方独立复现，`CODE_QUALITY_SUMMARY.md:80-83`、`VERIFICATION.md:138-145`）：函数体区间内 `password` 出现次数 = **0**。
- 后果：派生值 `F(password, salt, iters) ≡ F(salt, iters)`；`verify_password`（`sha256.h:180`）用存储串自带的 salt/iters 重算同一个 F，故 `calc == dk` **恒真** → 对任何 `pbkdf2$` 前缀哈希，**任意口令（含空串）都通过**。

缺陷 B：**HMAC 内层被错误实现为单个 SHA256**，而非两次 SHA256 的 ipad/opad 结构。
- 正确 HMAC：`inner = SHA256((K ⊕ 0x36) || msg)`，`HMAC = SHA256((K ⊕ 0x5c) || inner)`。
- 现实现把「拼接后的串」直接喂给 `sha256()`（`sha256.h:118`、`:124`），且迭代段（`:133`、`:139`）同样如此——**即使把 key 改成 password，只改缺陷 A 也仍然不是标准 PBKDF2**。
- 附加事实：`salt.size() > 64` 分支（`:101-104`）写入 32 字节后剩余 32 字节保持 0（`k_ipad` 初始化为全 0），但**从未对结果做 `memcpy` 回写 `salt`**，属死分支，修复时应一并删除（见「边界与反例」）。

**正确语义（修复的规范定义，必须逐字实现）**

设 `INT32BE(i)` 为 4 字节大端整数，`dkLen = 32`（单块，`c` 为迭代次数）：

```
U_1 = HMAC-SHA256(key = password, msg = salt || INT32BE(1))
U_i = HMAC-SHA256(key = password, msg = U_{i-1})          for i = 2..c
T_1 = U_1 XOR U_2 XOR ... XOR U_c                          （32 字节）
DK   = T_1                                                  （dkLen = 32）
```

- HMAC key = **password**；若 `password.size() > 64` 则先 `K = SHA256(password)`，否则 `K = password`，再右侧补 0 至 64 字节。
- HMAC 内层 = `SHA256((K ⊕ 0x36) || msg)`；外层 = `SHA256((K ⊕ 0x5c) || inner)`。
- 输出为 `T_1` 的 32 字节小写十六进制（64 字符）。

**修复动作**

1. 重写 `sha256.h:94-148`：按上文语义实现；`iterations <= 0` 时**不得**进入迭代、不得返回可用哈希（返回值策略见第 3 条验收判据与 §4 决策①）。
2. `verify_password`（`sha256.h:169-188`）**fail-closed**：`pbkdf2$` 分支的以下输入一律返回 `false`，不得抛异常、不得恒真：
   - 缺少第 2 或第 3 个 `$`（`hash.find('$',7)` / 再一个 `find` 任一为 `npos`）；
   - 迭代次数解析结果为 `<= 0`（注意 `std::atoi` 对非数字返回 0，对溢出是 UB → 改为严格解析：仅接受全部字符为 `'0'..'9'` 且长度 ≤ 9，否则 false）；
   - 迭代次数超过上限（建议上限 `10000000`，防 DoS）；
   - `salt` 为空、`dk` 为空、`dk.size() != 64`、`dk` 含非 `[0-9a-f]` 字符。
3. `routes_public.cpp:45-51` 的升级链：**仅在 `password_match == true` 时**才可改写哈希（现顺序已是先判定再改写，保留）；按 §4 决策① 处置存量 `pbkdf2$` 记录。
4. `models.cpp:17-22` 与 `main.cpp:250-254` 的裸 SHA256 种子哈希由 **T7** 一并移除（见 3.7）。

**验收判据（可机械判定）**

- V1-1：RFC 7914 §11 两组向量**逐字节相等**（不是前缀相等、不是长度相等），且**断言必须经过产品接口**（即 `pbkdf2_sha256(pw, salt, c, 64)` 这类产品导出函数），**不得以「`tests/run_tests.ps1` 退出码为 0」作为关闭 B1 的依据**。
  - ⚠️ **门槛收紧理由（verifier 变异体实验，2026-09-19）**：T2 初版的脚本对 RFC 向量只能断言 T_1（前 64 hex，因产品函数 dkLen 固定 32），完整 128 hex 走的是测试文件自带的参考实现。verifier 用「只修缺陷 A/B/C、结构正确、**无 dkLen 参数、恒返回单块**」的变异体 `sha256.h` 配原样测试文件实测 → **10/10 cases、32/32 checks 全绿、退出码 0**。旁证：`rfc7914_dklen64_block_counter` 这一用例在**未修复的坏实现**上也是 `[ OK ]` —— 该断言对产品函数零信息量。因此**产品接口必须能直接产出并被断言完整的 128 hex**，否则回归网形同不存在。
  - 向量原文（**队长于 2026-09-19 更正**；取自 `https://www.rfc-editor.org/rfc/rfc7914.txt` §11 正文，并经 Node `crypto.pbkdf2Sync` 独立复算一致）：
    - `PBKDF2-HMAC-SHA256(P="passwd", S="salt", c=1, dkLen=64)` =
      `55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783`
    - `PBKDF2-HMAC-SHA256(P="Password", S="NaCl", c=80000, dkLen=64)` =
      `4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d`
  - ⚠️ **更正记录（务必与 V1 一起阅读）**：本节初版给出的两组十六进制串是**错误的**——第一组只有 96 个字符、第二组 104 个字符，而 `dkLen=64` 必须恰好 **128 个字符**；错值的前 32 字节与 RFC 值相同、尾部是伪造/截断内容。**任何期望值只要长度不等于 128 个 hex 字符即可直接判为伪造**，无需密码学知识。该错值已由队长在 §8 之外单独更正，原文见本节上方。
  - **纪律**：期望值只能来自 RFC 7914 §11 正文，**禁止**用被测实现的输出充当期望值（循环论证，等于门禁自己给自己发合格证）。
  - **注意（对 T2 的措辞更正）**：这两组不是 RFC 6070 的向量——RFC 6070 规定的是 **PBKDF2-HMAC-SHA1**。本仓库实现的是 HMAC-**SHA256**，故权威出处必须是 **RFC 7914 §11**（`https://www.rfc-editor.org/rfc/rfc7914.txt`）。测试注释中必须写「RFC 7914 §11」而非「RFC 6070」。
- V1-2：`dkLen=64` 意味着**两块 32 字节** T 值拼接。只实现单块的实现会把第 2 块的 64 个 hex 字符写错，因此该向量同时钉死「块计数（`INT32BE(i)` 中的 i 随块递增）」这一点。
- **V1-2b（队长补充 · B1 缺陷 C/D，独立于缺陷 A）**：verifier 独立复现时发现 B1 不止「HMAC key 用成 salt」一处，共**四缺陷叠加**，T4 必须一次修全：
  - 缺陷 A：HMAC key 取自 salt，`password` 形参零引用。
  - 缺陷 B：内层被写成 `SHA256(k_ipad || salt || INT32BE(1))`，外层同理 —— 结构上根本不是 HMAC。
  - 缺陷 C（新）：`sha256()` 返回 **hex 字符串**，而现实现用 `std::memcpy(u, h1.data(), 32)` 取该 hex 串的**前 32 个字符**，即把 hex 文本当字节用 → **摘要被截断为 16 字节**；末尾又对 `t[32]` 再做一次 hex 编码 → **双重 hex 编码**（实测返回值形如 `3030386166…`，外层 hex 解码后为 `008af8085cf69e5a4e6f6ad616932acf`）。
  - 缺陷 D（新）：`salt.size() > 64` 分支同错；`dkLen` 被硬编码为 32 字节（单块）。
  - **因此 T4 的硬性要求**：① 先补**面向原始字节**的 `sha256_bytes()` 原语，保留 `sha256()` 的 hex 行为以兼容裸 SHA256 旧哈希分支；② 在字节原语之上实现**真正的 HMAC-SHA256**（key>64 先压缩；ipad/opad 异或作用在 64 字节 key 块上）；③ `dkLen` 可配并通过 `dkLen=64`（`ceil(dkLen/32)` 块）；④ 只在最终做一次 hex 编码。
  - **可判定判据**：`pbkdf2_sha256("passwd","salt",1)` 的 64 字节输出恰好 **128 个 hex 字符**且逐字节等于 RFC 值；若只修 A+B 而漏 C/D，第一组可能碰巧对上、第二组（c=80000 多块）必挂。
  - **基线证据**：verifier 已在运行时复现（自建 harness，g++ 编译运行）：修复前 `pbkdf2_sha256("passwd","salt",1)` 与 RFC 第一组的 dkLen=64 全比较、dkLen=32 前缀比较**双双 MISMATCH**，且 `verify_password` 对错误口令、空串、`0x01` **全部返回 1**。
  - **⚠️ 队长于 T2 交付后发现的门槛漏洞（务必与 V1-2b 同读）**：T2 的测试文件**如实自认**了它的覆盖边界 —— 生产函数 `pbkdf2_sha256(password, salt, iterations)` 的 dkLen **固定为 32（单块）**，所以它对 RFC 向量只能断言 **T_1（前 64 hex）**；完整的 128 hex 断言是用「本文件自带、仅基于产品 `sha256()` 原语的参考实现」做的，**不经过产品的 `pbkdf2_sha256()`**（见 `tests/pbkdf2_rfc7914_test.cpp:43-56`）。这是诚实的自我设限，但意味着**只靠 T2 的脚本，一个仍不支持多块的 T4 也能全绿**。
  - **因此补充硬要求**：**T4 必须让产品接口支持显式 dkLen**（例如 `pbkdf2_sha256(password, salt, iterations, dkLen)`，默认 32 以保持既有调用兼容），并且 **T4/T6 的验收必须走产品接口**对 RFC 两组向量做**完整 128 hex 逐字节**断言 —— 不接受用测试文件内的参考实现替代产品实现。否则 V1-9 的「测试能拦住回归」在本条上不成立。
- V1-3：同一 salt 下 `pbkdf2_sha256("a","s",1) != pbkdf2_sha256("b","s",1)`（直接钉死缺陷 A）。
- V1-4：`verify_password(正确口令, hash_password(该口令)) == true` 且 `verify_password(错误口令, 同一存储串) == false`。
- V1-5：`verify_password` 对下列 6 类畸形输入全部返回 `false` 且不抛：`""`、`"pbkdf2$"`、`"pbkdf2$0$00112233445566778899aabbccddeeff$deadbeef"`、`"pbkdf2$abc$...$..."`、`"pbkdf2$100000$00112233445566778899aabbccddeeff"`（字段缺失）、`"pbkdf2$100000$00112233445566778899aabbccddeeff$zzzz"`（非十六进制）。
- V1-6：`hash_password(p)` 输出严格匹配 `pbkdf2$100000$<32hex>$<64hex>`；连续两次调用 `salt` 段不同、`dk` 段不同。
- V1-7：非 `pbkdf2$` 前缀的裸 SHA256 存储串仍按 `sha256(password) == hash` 判定：正确口令 `true`、错误口令 `false`。
- V1-8：主进程编译（§术语）退出码 0，`server.exe` 生成。
- V1-9（反例门禁）：**修复前的基线必须失败** —— 在 T4 动手前，T2 的 V1-1/V1-3 必须真实失败，且失败输出被 T3 独立复现（这是 G1/G2 的存在意义）。**【已由 T3 独立复现，verdict=pass】** 证据见 `docs/audit/evidence/T3_B1_BASELINE.md`。
- **V1-10（队长补充 · T2 终结后的补强，必须由 T6 独立复核）**：T2 交付后又发生了一次**终结后修正**（backend-engineer 主动补强，`sha256.h` 未动），把「128 hex 必须来自产品接口」从人工判据升级为**编译期硬门禁**。这一轮改动**未经过独立验证**（T3 复现的是修正前的版本），因此以下三项必须由 T6 独立复核：
  1. **基线口径已变更**：`pwsh -NoProfile -File tests/run_tests.ps1` → `cases : 11 run, 5 ok, 6 failed` / `checks : 33 run, 25 ok, 8 failed` / 脚本 exit 1（g++ exit 0）。**不要拿 T3 报告里的 5 failed 与之对比而误判为不一致**（队长已实测复核该口径）。
  2. **编译期门禁可被证伪且真的拦住坏向量**：四条向量常量已改为 `constexpr char[]`，并对「dkLen=64 ⇒ 必须 128 个 hex 字符」「T_1 必须是 DK64 的前 64 字符」做 `static_assert`。**队长已独立校准**：把 V2_DK64 的第二个 64 字符字面量删掉（长度变 64）后编译 → `error: static assertion failed: RFC 7914 §11 向量 2 必须是 128 个 hex 字符（dkLen=64）；被截断的向量在此即被拒`，g++ exit 1。T6 需独立复现该校准（变异体可建在 `tests/` 内以便 `../sha256.h` 相对包含生效，用后删除）。
  3. **新增用例 `rfc7914_dklen64_from_production` 是有效门而非死门**：它在编译期探测 `sha256.h` 是否提供 `pbkdf2_sha256(const std::string&, const std::string&, int iterations, int dkLen)`；探测不到即**明确失败**（不得静默跳过）。修复前它必须红（这本身就是「产品接口没有多块能力」的直接证据）；修复后必须绿。T6 需确认它**不是永远为红**——即 T4 提供 4 参入口后该用例能通过。

**边界与反例（必须写进实现者的自检清单）**

- `iterations = 1` 时循环体必须**一次都不执行**（`T = U_1`）；现存 `for (int i = 1; i < iterations; i++)`（`sha256.h:129`）在 `iterations = 0` 时仍会执行一次迭代——修复后必须显式拒绝 `<= 0`。
- `password` 为空串必须能正常派生（不得因空 key 早退）。
- `password` 长度恰为 64 与恰为 65 字节：64 → 直接作 key；65 → 先 `SHA256` 压缩。两者都必须覆盖，因为现实现的 `> 64` 分支是完全错误的。
- `salt` 含 `$` 字符时 `verify_password` 的字段切分按**前两个** `$` 切分；`salt` 本身不得含 `$`（`generate_random_hex` 保证为 hex，故实际安全），但解析函数不得因此崩溃。
- **不要**把 `hmac_sha256(...)` 实现成「先 `sha256(key)` 再拼消息」；ipad/opad 的异或必须作用在 **64 字节 key 块**上。
- **不要**为了让用例通过而放宽比较（例如改成 `calc.compare(0, 32, dk) == 0` 或只比长度）——那是把门禁改成摆设。
- **禁止**把「`tests/run_tests.ps1` 不存在时静默跳过」当通过。

**分工**：T2（建测试+基线，不动 `sha256.h`）→ T3（独立复核缺陷与基线失败）→ T4（重写实现，纯 `sha256.h`）→ T6（独立验证不可绕过）。

---

### 3.2 F1 — CI 交付包前后端错配（blocker）

**位置（实测）**

- `ci.yml:41-46` `frontend` job 以 `VITE_USE_MOCK: 'true'` + `VITE_BASE: '/campus-master/'` 执行 `npm run build:no-typecheck`
- `ci.yml:54-59` 该 `dist/` 上传为 artifact 名 **`frontend-dist`**
- `ci.yml:105-108` `package` job `needs: [frontend, backend]`，仅 `push` 到 `main` 时执行
- `ci.yml:114-118` 下载 **`frontend-dist`** 到 `release/frontend/dist`
- `ci.yml:120-124` 下载 `server-exe` 到 `release`
- `ci.yml:135-140` 打成 `campus-energy-station-dev.zip`
- `frontend/src/lib/api.ts:58-60`：`isMockEnabled()` 为真时**所有** `/api/*` 交给 `mockRequest()`

**根因**

开发版交付 zip 里的前端是 **mock 构建产物**，请求永不发往后端；包内 `server.exe` 永远收不到请求。**次要但独立的第二处根因**：该 artifact 还带 `VITE_BASE=/campus-master/`（GitHub Pages 子路径），即使它不含 mock，被后端在站点根 serve 时静态资源路径也是错的。

**必须一并记录的对照事实（不得误判为「全都坏了」）**

- `release.yml:53-55` 的 `npm run build` **未设** `VITE_USE_MOCK` → 正式发布包不含 mock（tree-shaking 生效，属优点）。
- `pages.yml:41-47` 设为 `true` → Pages 纯前端演示站，**有意为之，不是缺陷**。
- `pages.yml` 的 artifact 走 `upload-pages-artifact@v3`（`pages.yml:59-62`），**不消费** `ci.yml` 的 `frontend-dist`。经 `grep` 核对，`frontend-dist` 的**唯一消费者**就是 `ci.yml:114-118` 的 `package` job。

**修复动作（唯一结论）**

采用**方案 A（推荐，判据最硬）**：`package` job 不再下载 `frontend-dist`，改为**在 package job 内以非 mock 方式重建前端**。

1. 删除 `ci.yml:114-118`「Download frontend dist」整步。
2. 在 package job 内、`Assemble release folder` 之前新增：`actions/setup-node@v4`（node 20，`cache-dependency-path: frontend/package-lock.json`）→ `working-directory: frontend` 执行 `npm ci` → 执行 `npm run build`（**必须用 `build`，不要用 `build:no-typecheck`，让类型门禁留在打包路径上**）。
3. **不得**给该步设置 `VITE_USE_MOCK`；**不得**设置 `VITE_BASE`（未设时 `vite.config.ts:11` 的 `env.VITE_BASE || '/'` 给出根路径，正是后端同源部署所需）。
4. 组装路径改为 `cp -r frontend/dist release/frontend/dist`。
5. 保留 `ci.yml:54-59` 的 mock artifact 上传**仅当**它仍被需要；本决策下它已无消费者 → 保留会给后来者制造「这个 dist 就是交付包前端」的误解，**推荐一并删除该步**（若删除，必须同时确认 `pages.yml` 不引用它——已核对为不引用）。
6. `frontend/vite.config.ts:12-15`：把默认值写成**显式字面量断言**：
   `'import.meta.env.VITE_USE_MOCK': JSON.stringify(env.VITE_USE_MOCK === 'true' ? 'true' : 'false')`
   （等价写法：`env.VITE_USE_MOCK || 'false'` 改为上述三元式）。
7. `frontend/src/mock/index.ts:657-659` 的 `isMockEnabled()` 已实现「仅字符串 `'true'` 视为开启」，**保持不动**（这是必须保留的正确写法，不得改成 truthy 判断）。

**验收判据（可机械判定）**

- V2-1：`ci.yml` 的 `package` job **不再出现** `frontend-dist` 字样（`grep -c 'frontend-dist' ci.yml` → 0，除非第 5 条的删除被明确否决，此时需给出为何保留的书面理由）；也不出现 `name: frontend-dist` 的下载步。
- V2-2：`package` job 内存在 `npm ci` 与 `npm run build` 且**不存在** `VITE_USE_MOCK`（对其做 `grep -n 'VITE_USE_MOCK' ci.yml`，只允许命中 `frontend` job 的 `:44`；若第 5 条采纳删除该 job 的 mock 构建，则 `ci.yml` 内应完全无命中）。
- V2-3：`package` job 的 `needs` 仍包含 `backend`（`grep -n 'needs:' ci.yml` 命中 `needs: [frontend, backend]`；若 `frontend` job 因第 5 条被删，则 `needs` 必须同步改为 `[backend]`，**不得留下悬空依赖**——这是 T10/T11 的必查项）。
- V2-4：`vite.config.ts:14` 的表达式在语法层面是 `env.VITE_USE_MOCK === 'true' ? 'true' : 'false'`（或等价显式断言），且 `VITE_USE_MOCK=yes` 时构建产物内 mock 特征串 **MISS**。
- V2-5：本机非 mock 构建产物不含 mock 特征串：`cd frontend; npm run build:no-typecheck`（未设 `VITE_USE_MOCK`）退出码 0，且对 `frontend/dist` 递归 `grep` mock 特征串（`campus_mock_db_v1`、`mockRequest`）**零命中**。
- V2-6：`cd frontend; npm run typecheck` 退出码 0。
- V2-7：**必须显式标注「CI zip 装配链路未经端到端复现」**——本机无法运行 GitHub Actions。禁止把 V2-1/V2-2/V2-3 的静态核对写成端到端验证。

**边界与反例**

- artifact 名冲突：若新增「非 mock 前端」artifact，**不得**复用 `frontend-dist` 这个名字（同名会让 `download-artifact` 取到旧包或直接失败）。
- 缓存泄漏：`setup-node` 的 `cache: 'npm'` 只缓存 `~/.npm`，不缓存 `dist/`；但 `package` job 是全新 runner，`frontend/dist` 不存在，故 `emptyOutDir: true` 不会误删他人产物。
- `VITE_BASE` 泄漏：若有人把 `VITE_BASE: '/campus-master/'` 复制进 package job，交付包仍是坏的（静默 404）——V2-6 之外必须再断言 release zip 内 `index.html` 的资源引用以 `/` 开头。
- **反例**：只把 `ci.yml:117` 的 `name:` 从 `frontend-dist` 改成别的名字，而不改构建方式 → 交付包仍可能拿到 mock（若新名字恰好指向 mock artifact）→ **不算修复**。

**分工**：T8 → T10 独立复核 → T11 对抗性复核。

---

### 3.3 B13 — 备份接口判据写反（medium）

**位置（实测）**：`routes_admin.cpp:323-350`，判据在 `:341-347`。

```cpp
341:  json backup_result = db.query("VACUUM INTO '" + backup_path + "'");
342:  if (backup_result.is_null()) {          // VACUUM INTO 无行返回 → query 返回 [] 而非 null → 恒 false
343:      response = {{"code", 200}, {"msg", "备份成功"}, {"data", {{"path", backup_path}}}};
344:      Logger::info("数据库备份成功: " + backup_path);   // ← 不可达
345:  } else {
346:      response = {{"code", 500}, {"msg", "备份失败"}};   // ← 恒走这里
347:  }
```

**根因**：`db.query()`（`sqlite_wrapper.h:53-62`）在 `prepare` 失败时也返回 `json::array()`（`sqlite_wrapper.h:54` 初始化 + `:59-61` 早退），对**无结果行**的语句（`VACUUM INTO`）返回 `[]`；`is_null()` 对数组恒 false → **恒返回 500 假阴性**，而备份文件其实已生成。成功日志 `:344` 永不执行。路径注入经复核**不成立**（`backup_path` 全部由服务端常量 + `get_current_time()` 生成，无请求参数参与，`VERIFICATION.md` §2.7）。

**修复动作**

1. 判据改为**可观测的成功事实**的组合，且失败路径真实可达：
   - 用 `db.execute(sql)`（`sqlite_wrapper.h:41-51`，`sqlite3_exec` 返回 `SQLITE_OK` 即 true）执行 `VACUUM INTO`；**并且**
   - 用文件系统校验备份文件**真实存在且大小 > 0**（C++17 `std::filesystem::file_size` + `std::error_code` 重载，禁止抛异常版本）；
   - 两个条件同时满足才返回 `{"code":200}` 并 `Logger::info`；否则返回 `{"code":500,"msg":"备份失败: <原因>"}` 并 `Logger::error`（原因不得为空字符串）。
2. 失败路径必须可达：`VACUUM INTO` 在目标文件已存在、目标目录不存在或不可写时返回错误 → 必须命中 `else` 分支。
3. 次要加固（**必须做**，属于同一缺陷面）：`get_current_time()` 的精度是**分钟**（`sha256.h:191-197`，格式 `%Y-%m-%d %H:%M`，冒号与空格被替换为下划线），**同一分钟内第二次备份会撞名**；改为在文件名中追加 `generate_random_hex(4)`，使路径唯一。
4. 保留既有权限与 CSRF 门（`routes_admin.cpp:327-331`），不得削弱。

**验收判据（可机械判定）**

- V3-1：`routes_admin.cpp` 中不再出现 `backup_result.is_null()`；对备份路径的成败判定不再依赖 `query()` 的返回值。
- V3-2（成功路）：真实启动 `server.exe`（先备份 `config.json` 与记录当前工作区状态），管理员登录取得 `sid` cookie 与 `csrf_token`，`POST /api/admin/system/backup` 带 `X-CSRF-Token`，响应 `code == 200`，且响应 `data.path` 指向的文件**在磁盘上存在且 `Length -gt 0`**。
- V3-3（失败路）：把备份目录（或使目标文件名不可写的方式）置为不可写后再次调用，必须得到 `code != 200` 且日志出现 `Logger::error` 记录；**不得**仍返回成功。
- V3-4：同一分钟内连续调用两次，两次返回的 `data.path` **不相同**（文件唯一）。
- V3-5：清理测试产生的 `*.db`、`server.log` 增量、以及还原 `config.json`；报告须写明确切的还原方式与还原后对照内容。

**边界与反例**

- `VACUUM INTO` 的目标文件**已存在时 SQLite 直接报错**（不做覆盖）——这既是 V3-3 的可用失败注入点，也是同名碰撞的真实后果，故 `:3` 的唯一化不是美化而是必需。
- 不得用「文件存在」**单独**作为判据而忽略 `execute` 的返回码：若上一次运行的旧文件仍在，会得到假阳性。必须两个条件同时成立。
- 不得改成 `db.query(...).is_array() && !empty()` 之类的等价错判。
- 反例：只把 `is_null()` 改成 `!is_null()` → 判据仍然恒 false（数组恒非 null）→ **不算修复**。

**分工**：T9 → T10（真实 HTTP 证据）。

---

### 3.4 N1 — HTTPS 回退后 `cookie_secure` 未复位（medium）

**位置（实测）**

- `config.h:79-85`：`if (config.https_enabled) config.cookie_secure = true;`（**全项目唯一赋值点**）
- `main.cpp:311-315`：

```cpp
312:  if (g_config.https_enabled) {
313:      Logger::warning("HTTPS 已在配置中启用，但当前编译版本不支持 SSLServer。…回退到 HTTP 模式。");
314:      g_config.https_enabled = false;
315:  }
```

- `auth.h:84-96`（`set_session_cookie` / `clear_session_cookie` 两处）与 `auth.h:171-175`（`issue_csrf_token`）：`if (g_config.cookie_secure) cookie += "; Secure";`

**根因**：回退到明文 HTTP 时只复位了 `https_enabled`，`cookie_secure` 保持 true → 三种 Cookie 全带 `Secure` → 浏览器在 `http://` 下**拒绝保存 `sid`** → 「登录成功但立刻掉线」，且日志只有一句回退告警。

**修复动作**

1. 在 `main.cpp:314` 之后**紧邻**补一行：`g_config.cookie_secure = false;`，并在该行加注释说明「回退到明文 HTTP 时必须同时复位 Secure 属性，否则浏览器不保存 sid」。放在 `main.cpp:311-315` 块内、`main.cpp` 的监听启动（`:333`）之前——**必须早于任何 `Set-Cookie` 下发**。
2. 该编辑必须是**增量**的：`main.cpp` 同时会被 T7（B2）改动，T9 不得回退 T7 的改动；动手前先 read 该文件确认当前内容。

**验收判据（可机械判定）**

- V4-1：`main.cpp` 中 `g_config.https_enabled = false;` 之后存在 `g_config.cookie_secure = false;`（给出改动后 file:line）。
- V4-2：`grep -n 'cookie_secure' *.h *.cpp` 显示：赋值点恰好为 `config.h:84` 与 `main.cpp` 新增行（共 2 处赋值），读取点为 `auth.h:88/95/174`（共 3 处）→ 确认 `cookie_secure` 是唯一开关、无其他旁路。
- V4-3（可判定证据）：把 `config.json` 的 `https.enabled` 临时置为 `true` 启动 `server.exe`，确认 (a) 日志出现回退告警，(b) 登录响应的 `Set-Cookie` **不含** `Secure`（可使用 pwsh `Invoke-WebRequest -SessionVariable` 抓取响应头，或最小 httplib 客户端）。结束后**必须还原** `config.json` 并给出还原后的对照内容。
- V4-4：还原后再次核对 `git diff --stat config.json` 为空（或 `config.json` 内容逐字等于备份）。

**边界与反例**

- 若把复位写成 `g_config.cookie_secure = g_config.https_enabled;`（此时后者已为 false）虽结果正确，但语义脆弱；**契约要求字面量 `false`**。
- 反例：只在 `Logger::warning` 里改文案而不复位 → 不算修复。
- 反例：在 `config.h` 里改 `load_config` 的赋值顺序（例如先存 `https_requested` 再置 `cookie_secure=false`）而不改 `main.cpp` —— **不算**满足 V4-1，因为回退决策发生在 `main.cpp`，不能靠构造函数时序耦合。

**分工**：T9 → T10（还原证据）。

---

### 3.5 F24 — 无效请求头 `Content-Length`（medium）

**位置（实测）**：`frontend/src/lib/api.ts:73-76`

```ts
73:  } else if (method === 'DELETE') {
74:    // 无请求体的 DELETE 需 Content-Length:0，兼容部分反代
75:    headers['Content-Length'] = '0'
76:  }
```

**根因**：`Content-Length` 属 fetch **禁止请求头**（forbidden request header），浏览器**静默忽略、不抛异常、无警告**（MDN *Forbidden request header* 原文："cannot be set or modified programmatically in a request"，清单明确含 `Content-Length`；`VERIFICATION.md` §3.4）。故这是「无害但误导」的死代码。

**修复动作**

1. 删除 `api.ts:74-75` 两行；`else if (method === 'DELETE')` 分支因无内容 → 整段 `else if` 一并删除，使 `if (data !== undefined && data !== null) {...}` 成为该处的唯一分支。
2. **不需要**任何替代写法：后端不依赖该头（`VERIFICATION.md` §7.3 第 3 项的逻辑闭合：浏览器始终丢弃该头而 DELETE 功能正常；DELETE 的 CSRF 走 `X-CSRF-Token`，`auth.h:162-164` + `api.ts:79-82`）。
3. 若实现者仍想兼容反代，**必须**改为不依赖禁止请求头的等价写法（例如由反代自行按无体请求处理），并在报告中给出规范依据；**默认结论是删除，不加替代**。

**验收判据（可机械判定）**

- V5-1：`grep -n "Content-Length" frontend/src/lib/api.ts` → **零命中**。
- V5-2：`cd frontend; npm run typecheck` 退出码 0（删除分支后不得留下未使用的分支/变量）。
- V5-3：`api.ts` 中仍存在 `X-CSRF-Token` 注入逻辑（`grep -n 'X-CSRF-Token'` 命中），DELETE 的 CSRF 能力未被误删。

**边界与反例**

- Node/undici 环境**会保留**该头（`VERIFICATION.md` §3.4 补充限定）→ 删除会改变非浏览器侧行为；契约明确：**浏览器是本项目唯一目标运行时**，故该行为变更可接受，并须在报告中记录这一限定。
- 反例：把 `headers['Content-Length'] = '0'` 改成 `headers['content-length']`（小写）→ 仍是禁止请求头 → 不算修复。
- 反例：只加注释说「该头无效」但保留代码 → 不算修复（V5-1 必须零命中）。

**分工**：T8 → T10。

---

### 3.6 F16 — 发布不可复现（medium）

**位置（实测）**：`release.yml:49-51`

```yaml
49:      - name: Install frontend dependencies
50:        working-directory: frontend
51:        run: npm install
```

对照：`ci.yml:33-35`、`pages.yml:37-39` 都用 `npm ci`。

**根因**：`npm install` 可改写 `package-lock.json` 与依赖树（在解析范围内会升级 semver 兼容版本）→ 同一 tag 每次发布可能装出不同依赖 → 「发布不可复现」。同时与 CI 的安装方式不一致。

**修复动作**：把 `release.yml:51` 的 `run: npm install` 改为 `run: npm ci`；其余不动。

**验收判据（可机械判定）**

- V6-1：`grep -n 'npm install' .github/workflows/*.yml` → **零命中**；`grep -n 'npm ci' .github/workflows/*.yml` → 命中 `ci.yml`、`pages.yml`、`release.yml` 三处。
- V6-2：`release.yml` 的 `Install frontend dependencies` 步仍是 `working-directory: frontend`（不得因改动丢掉工作目录）。
- V6-3：改动前后 diff 仅涉及该一行（不得顺手改 `npm run build` 为 `build:no-typecheck`——`release.yml:53-55` 的 `npm run build` 内含类型门禁，属于应保留的正确做法）。

**边界与反例**

- `npm ci` 要求 `frontend/package-lock.json` 存在且与 `package.json` 一致。经核对仓库有 `frontend/package-lock.json`（`ci.yml:31`/`pages.yml:35` 均以其作为缓存键）→ 前提成立。
- 本机**不得**为了验证而运行 `npm ci`（会写 `node_modules` 并可能影响工作区）；V6-1 的 grep 是唯一本机判据，须标注「静态核对」。
- 反例：改成 `npm install --no-save` → 依旧不保证可复现 → 不算修复。

**分工**：T8 → T10（静态核对）。

---

### 3.7 B2 — 种子默认口令 + 静默内存兜底（high）

**位置（实测）**

- `main.cpp:250-254`：4 个种子账号 `INSERT OR IGNORE INTO users (...)`，`password_hash` 为**硬编码无盐 SHA256**（`admin123`/`teacher123`/`student123`/`parent123`，审计已用 .NET SHA256 独立复算四组全部 MATCH）
- `models.cpp:17-22`：**内存兜底用户表**，含**同样 4 个**裸 SHA256 哈希
- `main.cpp:60-75`：`db.open()` 失败时只 `Logger::warning("SQLite 数据库连接失败，使用内存存储")` 并继续
- `main.cpp:77-283`：schema 初始化与种子插入被 `if (db.isOpen())` 包住
- `sqlite_wrapper.h:19-32`：`open()` 失败返回 `false` 但 `db_` 仍被 `sqlite3_open` 赋值 → `isOpen()`（`:190-192`）仅判 `db_ != nullptr`
- `main.cpp:230-248`：12 权限码 / 4 角色映射

**根因（两条）**

1. 可预测默认凭据随产物分发：4 组口令是「用户名 + 123」，且**没有任何强制改密或首启轮换机制**；配合 F13（`Login.vue:181-208` 预填按钮 + `release.yml:164-167` 明文口令）等于「拿到产物的人不需要额外信息即可登录」。
2. DB 打开失败时**静默降级**：只打一条 warning 就继续服务，运维无从察觉；且此时 `db_.` 句柄可能是无效的，所有查询静默失败/返回空集，服务表面「在跑」实则全部业务不可用。

**修复动作（按 §4 决策②/§4-2 的唯一结论）**

1. **首启随机强口令**：数据库**首次创建**（判定：`users` 表在种子插入前为空，即 `SELECT COUNT(*) FROM users` → 0）时，为 4 个种子账号各生成**互不相同**的随机强口令（复用 `sha256.h:151-159` 的 CSPRNG 能力，或等强度的 `generate_random_password()`），**仅在标准输出/日志中以一次性明文提示形式输出一次**；不得写入任何文件（含 `server.log`、不得落库为明文）。
2. **seed SQL 去哈希**：`main.cpp:250-254` 的硬编码哈希移除，改为运行时 `hash_password(随机口令)` 结果**参数化**写入（`db.execute_bind`，占位符 `?`）。
3. **移除内存兜底表里的弱口令**：`models.cpp:17-22` 的 4 个裸 SHA256 种子**必须**改为 `password_hash = ""`（空串）或等价「不可登录」值 —— 空串经 `verify_password` 一路为 false（非 `pbkdf2$` → `sha256(password) == ""` 恒 false）。**不得**保留任何可计算的弱口令哈希。
4. **强制首登改密**：见 §4 决策② 的最终形态（`users.must_change_password INTEGER NOT NULL DEFAULT 0` + 双中间件门禁 + 改密接口 + 旧库 `ALTER TABLE` 兼容迁移）。
5. **静默降级改 fail-fast**：`main.cpp:60` 的 `db.open()` 失败（以及 `:69-71` 的重新打开失败）改为 `Logger::error(...)` + **`std::exit(1)`**（或 `return 1`），不得继续启动；同时把 `Logger::warning("…使用内存存储")` 这类会误导的文案删除。
6. **登录响应新增字段**：`routes_public.cpp:73-86` 的 `data.user` 增加 `must_change_password`（bool），供前端在登录后立即判定（T8 消费）。
7. `config.h` 只允许新增**常量**（例如 `PBKDF2_ITERS` / 最小口令长度）；不得改动既有字段语义（`cookie_secure` 那一段属 T9 范围，**T7 不得触碰**）。

**验收判据（可机械判定）**

- V7-1：`grep -n '240be518fabd2724\|cde383eee8ee7a44\|703b0a3d6ad75b64\|82e3edf5f5f3a46b' *.cpp *.h` → **零命中**（4 个弱口令 SHA256 在源码中彻底消失）。
- V7-2：全新首启（删除测试库后启动）时，标准输出出现 4 组一次性明文口令，**4 个口令两两不同**，且**再启动一次不再输出**（因为 `users` 表非空）。
- V7-3：`grep -rn` 确认随机口令**没有**被写入任何文件（`server.log`、`*.txt`、`*.json`）：首启后 `Select-String -Path server.log -Pattern '<明文口令>'` → 零命中（报告须贴出该命令与结果）。
- V7-4（迁移）：手工构造一个**旧库**（有 `users` 表但无 `must_change_password` 列）→ 启动 `server.exe` → 用 `sqlite3` CLI 或最小程序执行 `PRAGMA table_info(users)` 必须显示新列，且**旧库文件未被删除**（对比文件 inode/大小/mtime，或对比库内既有行数不变）。
- V7-5（拒绝路径）：用未改密的种子账号登录成功 → 立即调用任一业务接口（例如 `GET /api/admin/users`）→ 必须被拒绝（`403` 或明确业务码 + 可读 `msg`）；同一会话调用改密接口与登出 → 必须成功。
- V7-6（改密接口）：正确原口令 + 合规新口令 → `code == 200` 且 `must_change_password` 落库为 0；错误原口令 → 非 200；弱口令（长度不足 / 单一字符类）→ 非 200；新旧相同 → 非 200。四类输入的实测结果都要贴出。
- V7-7（fail-fast）：把 `config.json` 的 `database.path` 临时指向一个**非法路径**后启动 → 进程以非 0 退出，日志有 `Logger::error`，**不得**出现「使用内存存储」字样；结束后还原 `config.json` 并给出对照。
- V7-8：主进程编译退出码 0；`tests/run_tests.ps1` 全绿（RFC 向量与 `verify_password` 用例不受影响）。

**边界与反例**

- **硬约束（最易造成编译失败或静默错位）**：`models.h:18-27` 的 `struct User` 新字段**必须追加在结构体末位**（`string student_id;` 之后）并带默认初始化器 `= 0`。理由：仓库存在**聚合位置初始化** —— `models.cpp:17-22`（8 个值）与 `routes_teacher.cpp:133-142`（8 个值）—— 插在中间会让 `student_id` 被写成 `must_change_password`，轻则编译报错、重则静默错位。详见 `docs/audit/BATCH0_DESIGN_DECISIONS.md` ②-7。
- 「随机口令只输出一次」的判定口径：以**进程生命周期**为单位（同一进程内只输出一次）；重启后若 `users` 表非空则不再输出。若实现为「每次启动都重新生成并输出」→ **违反 V7-2**（那等于口令持续轮换，且日志成为口令泄露面）。
- **反例（必须避免）**：只把 `main.cpp:250-254` 改成参数化写入，却保留 `models.cpp:17-22` 的裸哈希 → `load_users_from_db()` 失败时兜底表仍可用弱口令登录 → **不算修复**（V7-1 会抓到这里）。
- **反例**：用 `std::exit(1)` 之外的方式「提示但不退出」（例如只打 error 后继续）→ 不算修复。
- **反例**：把 `must_change_password` 只做在前端（隐藏菜单）→ 不算修复；契约要求**服务端**在中间件层拒绝。
- 空口令/超长口令：`generate_random_password()`（`sha256.h:200-227`）长度 8~12、含大小写与数字；契约要求「强口令」判据为**长度 ≥ 12 且至少含大写/小写/数字三类**（`generate_random_password` 当前不满足长度 12，实现时须提高下限或调用其等价增强版）。
- 并发窗口：`must_change_password` 必须在**同一事务/同一条 SQL** 内随口令一起更新，避免「改密成功但标记未清」的窗口；本批次不引入锁，但 SQL 必须写成单条 `UPDATE users SET password_hash=?, must_change_password=0 WHERE id=?`。

**分工**：T7（后端）→ T8（前端交互与产物文案）→ T10（独立验证）。

---

### 3.8 F4 — 教师端评价列恒「未评价」（high）

**位置（实测，全部在 `frontend/src/pages/teacher/TeacherApp.vue`）**

| 行 | 内容 |
|---|---|
| `:28-34` | `interface Student { id: number \| string; studentId: string; ... }` |
| `:59-65` | `interface Evaluation { id: number; studentId: number; dimensionId: number; score: number; comment: string }` ← **类型失配的声明处** |
| `:161-167` | `evaluationDimensions`（1~5 维度的前端固有常量，正确） |
| `:168-171` | 硬编码假评价：`{ id:1, studentId:1, dimensionId:1, score:85 }`、`{ id:2, studentId:1, dimensionId:2, score:90 }` |
| `:280-289` | `loadStudents()` → `students.value = res.data` |
| `:313-321` | `loadEvaluations()` → `await api.get('/api/teacher/evaluation/dimensions')` **丢弃返回值**，注释自认「实际未使用返回值。此处保留 API 调用以维持网络行为」 |
| `:634-637` | `getStudentEvaluation(studentId: number \| string, dimensionId)` 用 `e.studentId === studentId` 严格相等 |
| `:639-662` | `submitEvaluation()` 成功后 `await loadEvaluations()`（`:651`），但上者从不回填 |
| `:664-666` | `editEvaluation()` 仅 `toast.info('编辑评价功能开发中...')` |
| `:668-683` | `deleteEvaluation()` 弹确认框后仍只 `toast.info('删除评价功能开发中...')` |
| `:1038-1081` | 5 个维度列，全部 `v-if="getStudentEvaluation(student.id, N)"` + `v-else` 渲染「未评价」 |
| `:1087-1091` | 模板内的编辑/删除按钮，绑定上面两个死函数 |

**后端事实（实测，作为前端契约）**

- `routes_teacher.cpp:76-99`：`GET /api/teacher/students` 返回 `{"id": user.id, "studentId": user.student_id.empty()? user.username : user.student_id, "name", "className", "points"}` → **`id` 是字符串**（`main.cpp:80` `id TEXT PRIMARY KEY`，种子如 `student-02-01-01`）。
- `routes_teacher.cpp:727-777`：`GET /api/teacher/evaluations`（**复数**）返回 `{"code":200,"data":[{"id","student_id","student_name","className","dimension_id","dimension_name","score","comment","evaluator_name","time"}]}` → **字段名是 `student_id`（snake_case），值是字符串**。
- 权限：该端点要求 `evaluation:manage`（`:731`）；按班级作用域过滤（`:746`）。
- `POST /api/teacher/evaluation`（单数，`routes_teacher.cpp:463` 起）与 `GET /api/teacher/evaluations`（复数，`:728`）**不存在路径遮蔽**（`VERIFICATION.md` §3.14 三重论证），修改时不要顺手重命名。

**根因**：`Evaluation.studentId` 声明为 `number`，而运行时两侧是字符串 `"student-02-01-01"`；`getStudentEvaluation` 用 `===` → `1 === "student-02-01-01"` 恒 false → **所有学生的 5 个分数列一律渲染「未评价」**；85/90 是**不可达死数据**；真实评价只写不读（`loadEvaluations()` 丢弃返回值）。

**修复动作**

1. 删除 `:168-171` 的硬编码数组 → `const evaluations = ref<Evaluation[]>([])`。
2. `Evaluation` 接口按后端真实形态对齐：`studentId: string`（若实现者选择保留后端原始字段名，则须映射）。
3. `loadEvaluations()` 改为请求 **`GET /api/teacher/evaluations`**（复数端点），并在 `res.code === 200` 时把 `res.data` **真正回填**到 `evaluations`（字段名 `student_id` → 前端 `studentId`）。删除 `:314-315` 的「保留 API 调用以维持网络行为」注释。
4. `getStudentEvaluation` 的匹配判据必须对字符串成立：`String(e.studentId) === String(studentId)`（或两侧均声明为 `string` 后直接用 `===`）。
5. 两个死操作按审计建议改为**禁用态**：`editEvaluation`/`deleteEvaluation` 对应的按钮（`:1087-1091`）加 `disabled` 并给 `title="暂未开放"`；或直接移除按钮与函数。**不得**保留「确认后无动作」的路径。
6. 完成后再触发一次数据加载（`loadEvaluations()` 应在 `onMounted` 的表单初始化链里被调用，确认它确实在教师端首屏被调用）。

**验收判据（可机械判定）**

- V8-1：`grep -n 'score: 85\|score: 90' TeacherApp.vue` → **零命中**；`grep -n 'studentId: 1'` → **零命中**。
- V8-2：`grep -n "api.get('/api/teacher/evaluation/dimensions')"` → **零命中**；`grep -n '/api/teacher/evaluations'` → 命中且出现在 `loadEvaluations()` 内。
- V8-3：`evaluations.value` 在 `loadEvaluations()` 内被赋值（给出改动后 file:line）。
- V8-4：`Evaluation` 接口的 `studentId` 类型为 `string`，并给出后端字段类型的 file:line 证据（`routes_teacher.cpp:761` 的 `eval["student_id"]` + `routes_teacher.cpp:89` 的 `"id": user.id` + `main.cpp:80` 的 `id TEXT PRIMARY KEY`）。
- V8-5：匹配逻辑可判定 —— 给出**等价比较的实测论证**：例如用 node 跑
  `node -e "console.log(String('student-02-01-01')===String('student-02-01-01'))"` → `true`，
  并说明修复前 `1 === "student-02-01-01"` 为 `false`；不得只写「看起来对了」。
- V8-6：`:1087-1091` 的两个按钮处于 `disabled` 或已移除（给出改动后 file:line 与 grep 输出）。
- V8-7：`cd frontend; npm run typecheck` 退出码 0，贴原始输出。
- V8-8（反例门禁）：**不得**用 `Number(student.id)` 转换两侧来「凑对」——`users.id` 是 TEXT 且形式为 `student-02-01-01`，`Number()` 会得 `NaN`；若实现者这样做，V8-5 必须失败并拒绝通过。

**边界与反例**

- `Evaluation.id`（评价自身主键）是 `INTEGER PRIMARY KEY AUTOINCREMENT`（`main.cpp:125`），**保持 `number` 正确**；只有 `studentId` 是 TEXT。不要把两者一起改成字符串。
- `dimensionId` 是前端固有 1~5 常量（`main.cpp:127` `dimension_id INTEGER`），**保持 `number`**。
- 「无评价」仍须显示「未评价」：`getStudentEvaluation` 返回 `null` 时走 `v-else`（`:1044` 等），该分支**必须保留**。
- `v-if="getStudentEvaluation(...)"` 在分数为 `0` 时也会走 `v-else`；本契约不改此行为（分数 0 属边界，留批次 2），但实现者若顺手改成 `!== null` 判据，属**可接受的小幅改进**，须在报告中说明并在 T10 中复核「0 分显示为 0」。
- 复核对照：`ParentApp.vue` 的 `currentChildId = ref<string | null>(null)`（`CODE_QUALITY_REVIEW.md:476`）是同一类活儿**做对了**的范本，可作为实现参照。
- 不得顺手拆分 1,519 行的巨型组件（属批次 3，`outOfScope`）。

**分工**：T5（前端）→ T10（独立复核类型对照与消除）。

---

### 3.9 B6 — `users.erase` 后索引错位（high）

**位置（**审计时实测快照 —— 注意：下列 `:496` / `:500` / `:501` 三行是批次 0 修复前的状态**；当前状态见 `:503-511` 的勘误块与 T24/T9 的落地位置。引用前请先读 §3.9 末尾的勘误。）**

- `models.cpp:164-169`：`user_id_map` / `user_username_map` 存的是 **`users` 向量的下标**（注释自称「安全修复 V12」）
- `models.cpp:171-186` `init_indexes()`：**无 `clear()`**，只做 `map[key] = i` 赋值 → ⚠️ **已被 T24 修复**：现为 `models.cpp:210-229`（`:211` 调 `clear_and_rebuild_user_indexes()`、`:212-214` 三个 `.clear()`、`:215-228` 依源重建），实测幂等（权限条目总数 23/23/23）
- `models.cpp:199-203` `remove_user_index()`：只 `erase` 两个 key，**不修正后续元素的下标** → 该描述**仍然成立**（函数体现为 `models.cpp:256-259`，未变；T32 后其活调用点为 **0**，见 `:510`）
- `models.cpp:205-212` `find_user_by_id()`：`return &users[it->second]`
- `models.cpp:214-221` `find_user_by_username()`：同上
- 删除路径 1：`routes_admin.cpp:825-828`（`delete_user_from_db` → `users.erase(user_it)` → `remove_user_index(...)`）→ ⚠️ **已被 T9 修复**：现为 `users.erase` → `delete_user_from_db` → **`rebuild_user_indexes();`（`routes_admin.cpp:889`）**
- 删除路径 2：`routes_teacher.cpp:257-263`（`users.erase(it)` → `remove_user_index(...)` → `delete_user_from_db`）→ ⚠️ **已被 T9 修复**：现为 `users.erase` → `delete_user_from_db` → **`rebuild_user_indexes();`（`routes_teacher.cpp:262`）**
- 其他索引维护点（本批次**不改**，仅记录完整性）：增号 `routes_admin.cpp:520-521`、`:632-633`、`:1501`（批量导入不即时更新，靠 `:1667` 全量重建兜底）、`routes_public.cpp:167-168`、`routes_teacher.cpp:143-144`、`:214`、`:952-953`；改名 `routes_admin.cpp:721-735`（~~先 `update_user_index` 再对旧名 `remove_user_index`，顺序正确~~）
  - ⚠️ **勘误（队长于 2026-09-19 追加，实测定论）**：上面那句「**顺序正确**」是**错误结论**，请勿采用。实测机制：`remove_user_index()`（`models.cpp:256-259`）的语义是**同时** `user_id_map.erase(user_id)` **与** `user_username_map.erase(username)`；因此该改名序列 `update_user_index(*user_it)` → `remove_user_index(user_id, old_username)` 会把**刚刚写入的 id 键亲手删掉** → `find_user_by_id(id)` 返回 `nullptr` → `user_must_change_password()`（经 `find_user_by_id`）判定失效 → **`must_change_password=true` 的用户在改名后可绕过强制改密门禁**（B2 判据的直接反例）；`check_permission_optimized` 同样受影响。该缺陷**只在重启（`init_indexes()`）后自愈**。
  - **可判定证据（两路独立）**：① 探针：改名后 `user_id_map.size=2` 而 `user_username_map.size=3`（**两映射长度不一致**）、`find_user_by_id(id)==nullptr`、`find_user_by_username(新名)` 仍正确、`rebuild_user_indexes()` 后自愈（8/8 PASS）；② 端到端：admin 改名后新用户名登录得 `code=200 must_change_password=True`，而受门禁端点 `GET /api/student/points/records` 返回 **HTTP 200（应为 403）**。
  - **性质**：属 **HEAD 版本即存在的原有代码**（`git show HEAD:routes_admin.cpp` 的 `:732/:734` 对应当前 `:783/:785`），**非本批引入、非任何本批任务的回退**；`git diff` 不含该路径。
  - **后果（对 B6 口径的影响）**：B6 的修复范围原应限定为「**两条 `users.erase` 删除路径已修；改名路径未修**」。**该限制已解除**：第三条同源路径（改名）亦已修（见下「后续归属」），B6 的三条同源路径现已全部闭合。本条勘误仍作为「审计给出了一个『已核对』的肯定结论，但该结论从未被任何可执行判据验证过」的实例保留。
  - **后续归属（T32 完成后的状态）**：已建独立修复任务 **T32**（`inScope` 仅 `routes_admin.cpp`），**T32 已完成** —— 修复位于 **`routes_admin.cpp:795`**（`update_user_index(*user_it)` + `remove_user_index(user_id, old_username)` 两行替换为一次 `rebuild_user_indexes();`，并一并移除随之无用的 `old_username`）。
    - **修复前实测危害（比"绕过"更严重）**：改名后新名登录得 `must_change_password=true`，而 `POST /api/auth/change-password` 返回 **HTTP 404** ⇒ 该用户**既进不去也改不了密，账号等于被改坏**；且只在重启后由 `init_indexes()` 自愈。
    - **修复后三段对照**：受门禁端点 **403**（含 `must_change_password` 体）／改密 **200**／改密后同会话 **200** 且 `/api/student/info` 返回该用户（`404 用户不存在` 症状消失）；并由**重编后的仓库产物**复测得同一结论。
    - **批次 1 项（注意区别，勿误删）**：① **`remove_user_index()` 现为真死代码**（全仓活调用点 = **0**，T32 后实测）→ 可删除，或保留但修正其「同时 erase id 键与用户名键」的陷阱语义（本缺陷根源）；② **`update_user_index()` 不可删** —— 当前仍有 **6 个活调用点**（`routes_admin.cpp:572`、`:684`、`routes_public.cpp:175`、`routes_teacher.cpp:144`、`:214`、`:954`），均为「新建账号后写索引」的正确用法，仅需考虑其不清理旧名残留的语义；③ `check_permission_optimized()`（`models.cpp:280`）**活调用点 = 0**，同为死代码。
    - ⚠️ **切勿写成"`remove_user_index()` 与 `update_user_index()` 一并清理"** —— 那会误删仍在使用的函数。

**根因**：`users.erase(it)` 使 `it` 之后的**所有元素下标减 1**，而两个 `map` 里存的还是旧下标 → `find_user_by_id()` 返回**另一个用户**（越权/串号），且 `init_indexes()` 不清 map 使错位被固化。

**修复动作（按 §4 决策④ 的唯一结论：本批只取「先 clear 再重建」）**

1. 在 `models.h`/`models.cpp` 新增一个语义明确的函数（签名建议）：

   ```cpp
   // 删除用户后调用：清空并重建用户索引，消除下标错位（批次 0 临时修复）
   void rebuild_user_indexes();
   ```

   实现为：`user_id_map.clear(); user_username_map.clear();` 然后按 `users` 当前顺序重新填充这两个 map。**不得**调用 `init_indexes()`（后者还会向 `role_permission_map` **重复 push_back**，见下条反例）。
2. 两个删除点改成：`users.erase(...)` →（保留对 DB 的删除）→ **`rebuild_user_indexes()`**；可以删除原先的 `remove_user_index(...)` 调用（重建已覆盖它）。
3. 顺手修正 `remove_user_index` 的**语义缺陷**（**可选但推荐**，须在报告中说明）：该函数签名接受 `username` 却在重建方案下不再需要；若保留，必须只在「不改变向量长度的路径」使用。
4. 三个新增用户的路径（`:520-521`、`:632-633`、`:143-144`、`:952-953`）**本批次不动**（`update_user_index` 在 push_back 之后用循环重查下标，行为正确；`routes_admin.cpp:1501` 的批量导入靠 `:1667` 全量重建兜底）。**不在本批添加锁定或改索引键类型**（属批次 1-2/1-3）。
5. 在 `rebuild_user_indexes()` 的注释与文档中显式标注：**这是临时修复，batch 1-3 会用稳定键/`deque`/`vector<unique_ptr>` 取代**。

**验收判据（可机械判定）**

- V9-1：`grep -n 'rebuild_user_indexes' *.cpp *.h` 命中定义（`models.h` 声明 + `models.cpp` 实现）+ 两处调用（`routes_admin.cpp`、`routes_teacher.cpp`）。
- V9-2：`models.cpp` 中 `rebuild_user_indexes()` 函数体内同时存在对 `user_id_map` 与 `user_username_map` 的 `clear()`。
- V9-3（可判定证据，必做）：构造**至少 3 个用户**的内存状态，删除**中间一个**后用 `find_user_by_id` 逐个查询**剩余用户 id**，证明不会取到错误对象。允许写临时 C++ 程序直接驱动 `models.cpp` 的内存容器（临时文件需清理或明确标注路径）；报告须贴出**原始输出**（例如 `find_user_by_id("u1")->id == "u1"`、`find_user_by_id("u3")->id == "u3"` 均为 true）。
- V9-4（反例门禁，必做）：在**修复前**的同一最小程序上运行，必须观察到错位现象（例如删除 `u2` 后 `find_user_by_id("u3")` 返回 `nullptr` 或返回 `u3` 之外的对象）；把该失败输出作为「缺陷真实存在」的基线证据贴出。
- V9-5：`grep -n 'init_indexes' *.cpp` 中，删除路径**不得**通过 `init_indexes()` 完成重建（否则会重复追加 `role_permission_map`）。
- V9-6：主进程编译退出码 0；`tests/run_tests.ps1`（若已存在）仍全绿。

**边界与反例**

- **反例（最易踩）**：在删除路径直接调 `init_indexes()`。`init_indexes()`（`models.cpp:183-185`）对 `role_permission_map` 用 `push_back`，第二次调用会把每个角色权限**重复追加**（`role_permission_map[2]` 从 7 项变 14 项）→ 权限判定结果会被污染。因此契约要求**独立**的 `rebuild_user_indexes()`。
- 删除**末尾**元素：下标无错位，但这不能作为「不用重建」的理由（判据必须覆盖中间元素）。
- 删除**唯一**元素：重建后两个 map 均为空，`find_user_by_*` 必须返回 `nullptr`。
- 同一 `username` 在 `users` 中出现两次（N2 类场景）时，`user_username_map` 只能保留一个下标；本批不修（批次 1-2 处理），但 T10/T11 需记录该残余风险。
- 不得引入锁（批次 1）；不得改 `find_user_by_id` 的返回类型（那是批次 1-2 的接口语义变更，超出本批 outOfScope）。

**分工**：T9 → T10（最小复现 + 基线对照）。

---

### 3.10 N4 — `.trae/` 25 个文件仍被跟踪（low）

**位置（实测）**

- `.gitignore:78-79`：`# Trae 工具目录（spec 文档可选择性保留，这里排除）` + `.trae/`
- `git ls-files | Select-String '^\.trae/' | Measure-Object` → **matched lines = 25**（与本文件 §1 的实测一致）
- 磁盘上 `.trae/` 实际有 **26** 个文件（与审计 25 的差异见 §8）

25 个已跟踪文件（实测清单，按路径排序）：
`.trae/documents/{admin_fix_plan,cookie-auth-refactor,localize-fonts-plan,remove-memory-storage-mode}.md`、
`.trae/specs/{add-parent-portal,batch-import-students,fix-admin-and-login,fix-teacher-click-parent-nav,refactor-uid-and-teacher-classes,unify-mobile-nav-and-admin-parent,write-project-docs}/{spec,tasks,checklist}.md`。

**根因**：`.gitignore` 规则是**后加**的，而 gitignore **不追溯已跟踪文件** → 25 个文件继续入库，`git status` 看起来干净。

**修复动作**

1. 执行 `git rm -r --cached .trae/`：**只动索引，不动工作区文件**（不加 `--force` 时对已跟踪且无本地改动的文件是安全的；若出现 `not staged` 报错需按 git 提示处理并在报告中如实记录）。
2. **不 commit、不 push**（本批次纪律）。
3. 操作前先记录 `git ls-files | Select-String '^\.trae/' | Measure-Object` 的基线计数（25）与 `.trae/` 在磁盘上的文件数（26），用于证明「索引移除 ≠ 删除文件」。

**验收判据（可机械判定）**

- V10-1：`git ls-files | Select-String '^\.trae/' | Measure-Object` → **matched lines = 0**（命令与输出均需贴出）。
- V10-2：`git status --porcelain` 中 25 个 `.trae/...` 路径呈现为**已暂存删除**（状态列首字符为 `D`）。
- V10-3：`.trae/` 在**磁盘上仍然存在且文件数不变（26）**（`Get-ChildItem -Recurse -Force -File .trae | Measure-Object`），证明只动索引。
- V10-4：`.gitignore:78-79` 的 `.trae/` 规则**保留**（不得删除规则）。
- V10-5：`git log --oneline -1` 与执行前一致（无新 commit）；`git diff --cached --stat` 只包含 `.trae/` 的删除。

**边界与反例**

- **反例**：用 `git rm -r --cached --force .trae/` 之外的手段（例如把 `.trae/` 移出仓库、或 `git update-index --assume-unchanged`）→ 不满足 V10-1/V10-2。
- `.trae/` 的 26 vs 25：说明存在 1 个未被跟踪的文件；**不得**为凑数把它也 `git add`。
- 该操作会改变索引但**不提交**，因此下一次真正提交时才会生效；这一点必须在报告中写明（它不构成本批次「未完成」，但属未闭环部分）。

**分工**：T9 → T10。

---

## 4. 待定设计决策

4 个决策的最终结论、理由与否决方案见 **`docs/audit/BATCH0_DESIGN_DECISIONS.md`**（同批交付）。本契约在 3.1 / 3.7 / 3.9 中已按该文件的结论固化，实现者**不需要**再做架构推断；若实现者认为某个决策不可行，**必须先回问 T1，不得自行改判**。

决策编号与影响条目：

| 决策 | 主题 | 影响的契约条目 | 唯一结论（摘要，详见决策文档） |
|---|---|---|---|
| ① | B1 存量 `pbkdf2$` 数据处置 | 3.1、3.7 | 视为失效 + 移除自动升级链；因工作区**无任何 .db 文件**，本机无需数据迁移，仅交付运维强制重置流程 |
| ② | B2 落库与接口形态 | 3.7、3.2(F13 前端)、T8 | `users.must_change_password INTEGER NOT NULL DEFAULT 0`（单列，非新表）+ 双中间件门禁 + 专用改密接口 + `ALTER TABLE` 兼容迁移 |
| ③ | F13 默认口令在产物中的处置 | 3.7、3.2(F13) | 登录页按钮收进 mock 门控（保留演示能力）；Release Notes 明文口令改写为「首次启动按控制台提示」 |
| ④ | B6 修复取向 | 3.9 | 本批取「erase 后 clear + 重建用户索引」；彻底版（稳定键）留批次 1-3 |

---

## 5. 文档内自检清单（逐条可打勾）

**A. 覆盖性**

- [x] 10 个条目全部覆盖：B1、F1、B13、N1、F24、F16、B2、F4、B6、N4（§2 总表 + §3 逐条）
- [x] 每条给出：精确 `文件:行号`、根因、修复动作、验收判据、边界与反例、任务号（T2..T12）
- [x] 4 个设计决策全部给出唯一结论（§4 + 决策文档）

**B. 可判定性**

- [x] 每条至少 1 条「grep 可判定」判据（V1-1～V10-4 中的 grep/计数类）
- [x] 每条至少 1 条「运行/实测可判定」判据（除 N4/F16 这类纯仓库/CI 项外，均已给出替代的本机可复现证据或显式标注静态核对）
- [x] 无「尽量」「建议」等不可判定措辞（对「推荐」「可选」项均已标注是否影响通过）
- [x] 每条包含至少 1 条**反例**（明确写出「不算修复」的情形）

**C. 行号真实性**

- [x] 全部行号由 read/grep 打开核对（差异见 §8）
- [x] 与审计文档不一致处已显式记录并采用实测值

**D. 只读性**

- [x] 本文件只新增 `docs/audit/BATCH0_CONTRACT.md`，未改动任何源码/配置
- [x] 未执行 `git commit` / `git push`
- [x] `git status --porcelain` 复核已执行 → 输出恰为 `?? .agent-teams/` 与 `?? docs/audit/`（无源码/配置改动，与审计只读性自证一致）

---

## 6. 事实核对记录（我实际打开的文件）

| 文件 | 核对内容 | 结论 |
|---|---|---|
| `sha256.h`（全文 229 行） | `pbkdf2_sha256:94-148` 的 key 来源与消息体；`verify_password:169-188`；`generate_random_hex:151-159`；`generate_random_password:200-227`；`get_current_time:191-197` | 与契约一致 |
| `main.cpp`（全文 336 行） | `:22` 全局 `SqliteDb db;`；`:26-42` 旧 schema 判定；`:60-75` 打开失败路径；`:77-283` schema+种子；`:250-254` 4 个裸 SHA256；`:303` `init_indexes()`；`:306` 线程池；`:311-315` HTTPS 回退；`:333` listen | 一致 |
| `models.cpp`（全文 244 行） | `:14` `points_records`；`:17-22` 内存兜底表；`:58-74` `load_users_from_db`（只加载 users）；`:77-85` `INSERT OR REPLACE`；`:112-160` `generate_user_id`；`:164-221` 索引与查找 | 一致 |
| `models.h`（全文 93 行） | `:73-91` 接口声明（`init_indexes`/`update_user_index`/`remove_user_index`/`find_user_by_*`） | 一致 |
| `sqlite_wrapper.h` | `:19-32` `open`；`:41-51` `execute`；`:53-62` `query` 早退返空数组；`:127-148` `execute_bind`（`:145-147` 0 行受影响仍 true）；`:190-192` `isOpen` | 一致 |
| `routes_public.cpp` | `:13-93` 登录（`:45-51` 升级链、`:73-86` 响应体）；`:167-168` 注册时建索引；`json::parse_error` 仅捕 `:88` | 一致 |
| `routes_teacher.cpp`（1171 行） | `:76-99` 学生列表（`id` 为字符串）；`:235-270` 删除学生（`:259-260`）；`:727-777` 复数评价端点（字段 `student_id`）；`:103` 起添加学生 | 一致 |
| `routes_admin.cpp` | `:320-350` 备份接口；`:700-735` 改名路径；`:805-828` 删除用户（`:826-827`）；`:1501`、`:1667` 批量导入 | 一致 |
| `auth.h`（全文 404 行） | `:84-96`、`:171-175` 三处 `Secure`；`:180-187` `require_csrf`；`:190-217` `create_session`；`:220-237` `verify_session`；`:240-275` 三处 `snprintf`；`:323-364` `check_permission_middleware`；`:369-402` 家长中间件 | 一致 |
| `config.h`（全文 94 行） | `:25` `https_enabled`；`:31` `cookie_secure`；`:79-85` HTTPS 读取与唯一赋值 | 一致 |
| `config.json` | `database.path = campus_system.db`；`https.enabled = false`；`cors.allowed_origins = []` | 一致 |
| `.github/workflows/ci.yml`（147 行）、`release.yml`（188 行）、`pages.yml`（77 行） | 见 3.2 / 3.6 逐行引用 | 一致 |
| `.gitignore`（84 行） | `:22-41` 忽略 `tests/`；`:78-79` `.trae/` | 一致 |
| `frontend/src/lib/api.ts`（135 行） | `:19-24` `ApiResponse<T=any>`；`:52`；`:58-60` mock 分派；`:73-76` `Content-Length`；`:79-82` CSRF；`:103-126` 状态码分支 | 一致 |
| `frontend/src/pages/Login.vue` | `:177-210` 演示账号按钮与 4 个 `fillTestAccount` | 一致 |
| `frontend/vite.config.ts`（40 行） | `:11` `base`；`:12-15` `VITE_USE_MOCK` define；`:29-38` dev proxy | 一致 |
| `frontend/src/mock/index.ts` | `:657-659` `isMockEnabled()` 严格等于 `'true'` | 一致 |
| `frontend/src/pages/teacher/TeacherApp.vue`（1519 行） | 见 3.8 逐行引用 | 一致 |
| `frontend/package.json`（29 行） | `:6-12` scripts（`build` 含 `vue-tsc --noEmit`，`build:no-typecheck` 不含；**无 test / 无 lint**） | 一致 |

Git/文件系统实测（本机 pwsh）：

```powershell
Get-ChildItem -Recurse -Force -File -Include *.db,*.db-wal,*.db-shm,*.sqlite,*.sqlite3   # → 空
(git ls-files | Select-String '^\.trae/').Count                                            # → 25
(Get-ChildItem -Recurse -Force -File .trae | Measure-Object).Count                          # → 26
Test-Path tests                                                                             # → False
git status --porcelain                                                                      # → ?? .agent-teams/  ?? docs/audit/
```

---

## 7. 下游任务的验收映射（谁用哪些判据）

| 任务 | 必须满足的判据（来自本文件） |
|---|---|
| T2 建测试 + 标准向量 | V1-1、V1-2、V1-3、V1-4、V1-9（基线必须失败）；不得修改 `sha256.h`；`tests/*.cpp` 与 `tests/run_tests.ps1` 可被跟踪（`.gitignore:31` 的 `tests/` 需调整，且须说明不违反「仓库仅保留源代码」的既有意图） |
| T3 复核 B1 存在 | V1-1/V1-3 的失败基线被独立复现；给出 `password` 零引用的机械证据；给出 `routes_public.cpp:45-51` 提权链的 file:line 与 `.db` 扫描结果 |
| T4 修 B1 | V1-1～V1-8 全部满足 |
| T5 修 F4 | V8-1～V8-8 |
| T6 复核 B1 修复 | V1-1（逐字节比较方式说明）、V1-4、V1-5、V1-7、V1-8 |
| T7 修 B2+F13(后端) | V7-1～V7-8 + §4 决策①/② |
| T8 修 F1/F16/F24/F13(前端)+F13 交互 | V2-1～V2-7、V5-1～V5-3、V6-1～V6-3、§4 决策③ |
| T9 修 B13/N1/B6/N4 | V3-1～V3-5、V4-1～V4-4、V9-1～V9-6、V10-1～V10-5 |
| T10 独立验证批次 0 其余 | 上述全部「运行/实测」类判据的**独立重跑** |
| T11 安全与正确性评审 | 以本文件的「反例」段作为对抗性输入；重点 §1 的顺序门禁与 §3.2/§3.7 的绕过路径 |
| T12 结项 | §1 的门禁与顺序已满足；V2-7 等「未端到端复现」事项已如实标注 |

---

## 8. 行号与事实差异记录（逐条）

**总则**：本文件所有行号以**我实测**为准；下表记录与审计文档的差异，以及我发现的新事实。差异不影响任何审计结论。

| # | 审计表述 | 实测结果 | 处理 |
|---|---|---|---|
| 1 | `CODE_QUALITY_SUMMARY.md:45` 称 B1 前置是「先补 **RFC 6070** 标准向量测试」 | RFC 6070 是 **PBKDF2-HMAC-SHA1**；本实现是 HMAC-**SHA256**，权威出处应为 **RFC 7914 §11** | **采用 RFC 7914 §11**；已在 3.1 显式标注（T2 的验收项原文已正确使用 RFC 7914） |
| 1b | 本契约 §3.1 初版给出的两组 RFC 7914 §11 向量 | **两组 hex 串均为伪造/截断值**：第一组仅 96 字符、第二组仅 104 字符，而 `dkLen=64` 必须恰好 128 字符（前 32 字节正确、尾部非 RFC 内容） | **已由队长于 2026-09-19 用 RFC 正文 + Node `crypto.pbkdf2Sync` 双重复算更正**；并在 §3.1 加入「长度必须为 128 字符」的廉价证伪判据。**教训**：声明「已逐字核对原文」不等于真的核对过——凡引用外部标准值，必须附上可机械复算的判据（此处即字符数），否则伪造值会静默流入回归网 |
| 2 | `api.ts:73-76` | 语句在 **`:74-75`**（`:73` 是 `else if`，`:76` 是 `}`）——与 `CODE_QUALITY_REVIEW.md:622` 的漂移记录一致 | 采用实测区间 |
| 3 | `sha256.h:179-184`（校验段） | 代码在 **`:180-184`**（`:179` 是注释）——与 `CODE_QUALITY_REVIEW.md:622` 一致 | 采用 `:180-184` |
| 4 | `models.cpp:16-22` 兜底表 | 数据行在 **`:17-22`**（`:16` 是注释）——与漂移记录一致 | 采用 `:17-22` |
| 5 | `main.cpp:312-315` HTTPS 回退 | 实测 **`:311-315`**（`:311` 是段落注释、`:312-315` 是语句） | 采用 `:312-315`（语句），区间表述用 `:311-315` |
| 6 | `routes_public.cpp:45-51` 提权链 | 实测 `:45` 判定、`:47-51` 改写（`:46` 是注释） | 采用 `:45-51` |
| 7 | `routes_admin.cpp:335-349` 备份判据 | 实测接口 `:323-350`，判据 `:341-347`，`:335-339` 是路径拼接 | 采用 `:341-347` |
| 8 | `.trae/` 25 个文件 | `git ls-files` 命中 **25**；但 `.trae/` 磁盘上实有 **26** 个文件 | 跟踪数采用 25；额外记录磁盘实有 26（多出的 1 个未被跟踪） |
| 9 | `routes_admin.cpp:826-827`、`routes_teacher.cpp:259-260` 的 `erase` | 实测一致 | 无差异 |
| 10 | `models.cpp:206-211` `find_user_by_id` | 实测 `:205` 注释 + `:206-212` 函数体 | 采用 `:205-212` |
| 11 | T9 任务书提到「`models.cpp` 的 `delete_user_from_db`/`update_user_points_in_db`」用于索引重建 | 这两个函数（`models.cpp:87-97`）**只管 DB，不管内存索引**；重建必须新增/扩展内存侧函数（`models.h` 可改，因其不在 `outOfScope` 内） | 已在 3.9 给出明确函数签名建议 |
| 12 | T7/T9 都涉及 `main.cpp` | `main.cpp` 同文件双人改动风险属实 | 契约拆分：**T7 不碰 `cookie_secure` 段；T9 只做 `:314` 之后的增量单行编辑**，动前必须 read |
| 13 | `vite.config.ts:14` 的 `VITE_USE_MOCK` 默认值 | 实测**已是**字符串字面量 `JSON.stringify(env.VITE_USE_MOCK || 'false')`，且 `isMockEnabled()`（`mock/index.ts:657-659`）**已是** `=== 'true'` | 该「配套」项已基本成立；契约改为**显式三元断言**（见 3.2 动作 6），并**禁止**把 `isMockEnabled` 改成 truthy 判断 |
| 14 | `pages.yml:45` 的 mock | 属**有意设计**（Pages 演示站），且其 artifact 走 `upload-pages-artifact`，**不消费** `ci.yml` 的 `frontend-dist` | 不立缺陷；F1 修复方案 A 因此可安全地不再上传 `frontend-dist` |

---

## 9. 已知未闭环/未端到端复现事项（必须与结论同读）

1. **CI zip 装配链路**（V2-1～V2-3）只做静态核对，本机无法运行 GitHub Actions → 标注「未端到端复现」。
2. **`npm ci` 与 `npm run build` 的 CI 行为**（F16、F1 的 package job）同理，本机只做 grep 与（前端侧）`build:no-typecheck` 的本地等价验证。
3. **B1 的「历史上是否已被触发」**：工作区无 `.db` 文件（已验证的否定事实），但 `server.log` 记录 603 次 `POST /api/auth/login` → 「历史数据状态」**UNVERIFIABLE**（沿用 `CODE_QUALITY_SUMMARY.md:261`）。
4. **存量 `pbkdf2$` 记录的真实存量数**：无数据库文件 → 本机无法统计；按 §4 决策① 一律视为失效处置。
5. **`.trae/` 的索引移除在本批次不提交**（纪律要求），因此对远端仓库的实际影响要等下一次提交才生效。
6. **B6 只是临时修复**：索引仍存向量下标，批次 1-3 会用稳定键替代；本批次不消除同源的 B5（悬垂指针）与 N2（并发重复 ID）。
7. **N1 的实测依赖** pwsh 抓取响应头或最小 httplib 客户端；若沙箱阻断，必须在报告中如实标注为「未复现」而**不得**降低判据。

---

## 10. 与审计文档的措辞纪律（防误读）

以下写法**禁止**出现在任何下游任务报告或结项文档中：

- ❌「F13 是前端内置后门 / 鉴权绕过」 → ✅「默认凭据随产物分发 + 无强制轮换机制；根因是 B2，单修 F13 收益≈0」
- ❌「F4 是 medium 提级为 high」 → ✅「F4 从头就是 high；被更正的是**机制**：分数列恒显示未评价，85/90 是不可达死数据」
- ❌「B13 是路径注入」 → ✅「路径注入经复核不成立；真实缺陷是判据写反（假阴性）」
- ❌「单参 `set_cors_headers` 是死代码」 → ✅「它是**占多数**（78:8）的调用形式，缺陷是永不回显 ACAO；同源部署下 low」
- ❌「B1 已修好，所以可以放心改默认口令」 → ✅「必须先修 B1 再改口令；顺序门禁见 §1」
- ❌「CI 已端到端验证」 → ✅「静态核对，未经端到端复现」
