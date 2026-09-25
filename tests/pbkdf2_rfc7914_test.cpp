// ===========================================================================
// tests/pbkdf2_rfc7914_test.cpp
//
// 目的
//   为仓库建立第一个可提交、可一条命令运行的 C++ 单元测试目标，并用**公开标准
//   向量**机械判定 B1：`sha256.h:94-148` 的 `pbkdf2_sha256()` 中 HMAC 的 key
//   被错用成 `salt`，`password` 形参在函数体内零引用，导致 `verify_password()`
//   对任何 `pbkdf2$` 前缀的哈希、任何口令（含空串）都返回 true —— 完整认证绕过。
//
// 标准向量出处（权威来源：RFC 7914 §11）
//   RFC 7914, Section 11, "Test Vectors for PBKDF2 with HMAC-SHA-256"
//     https://www.rfc-editor.org/rfc/rfc7914.txt
//     https://datatracker.ietf.org/doc/html/rfc7914#section-11
//
//   PBKDF2-HMAC-SHA-256 (P="passwd",   S="salt", c=1,     dkLen=64) =
//     55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc
//     49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783
//   PBKDF2-HMAC-SHA-256 (P="Password", S="NaCl", c=80000, dkLen=64) =
//     4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56
//     a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d
//
//   RFC 7914 才是 PBKDF2-HMAC-**SHA256** 向量的权威出处。RFC 6070 规定的是
//   PBKDF2-HMAC-**SHA1**，其 PRF 与本实现的 HMAC-SHA256 不同，不可引用。
//
// ---------------------------------------------------------------------------
// 重要更正（T2 复核实测）：契约/任务描述中给出的同一向量**字面量被截断损坏**
// ---------------------------------------------------------------------------
// docs/audit/BATCH0_CONTRACT.md:123-126 与 T2 任务描述里的两条 hexadecimal 常量
// 分别只有 95 和 104 个字符（应为 128），且从第 81 / 97 个字符起与 RFC 正文分歧：
//   契约字面量 v1 = "…49ca9cccf179" + "7d9d33b6e52c2b1a5f0"   (95 字符)
//   RFC 正文   v1 = "…49ca9cccf179" + "b645991664b39d77ef31…" (128 字符)
//   契约字面量 v2 = "…51c9b317"     + "a6cd49f6"               (104 字符)
//   RFC 正文   v2 = "…51c9b317"     + "6a272bdebba1d078478f62b397f33c8d" (128 字符)
// 四个彼此独立的来源产出的**完全相同**的 128 字符值：
//   1) rfc-editor.org 纯文本正文 §11
//   2) datatracker HTML §11
//   3) .NET System.Security.Cryptography.Rfc2898DeriveBytes(HashAlgorithmName.SHA256)
//   4) CPython hashlib.pbkdf2_hmac('sha256', …)
// 故本文件采用 RFC 正文值。若照抄被截断的契约字面量，用例将**永远无法通过**，
// 而 T4 会被迫去迁就一个错误的期望值 —— 这正是「不得凭猜测写向量」要防的事。
//
// ---------------------------------------------------------------------------
// 与 sha256.h 冻结接口的关系（BATCH0_CONTRACT.md §3.1:95-106）
// ---------------------------------------------------------------------------
// 生产函数 `pbkdf2_sha256(password, salt, iterations)` 的 dkLen 固定为 32（单块），
// 而 RFC 7914 §11 两组向量的 dkLen 都是 64（两块）。PBKDF2 的各块彼此独立：
//     T_i = U_1(i) XOR … XOR U_c(i)，DK = T_1 || T_2 || … 截断到 dkLen
// 因此 dkLen=32 的输出恒等于 dkLen=64 输出的**前 32 字节**。据此：
//   * rfc7914_v1_t1 / rfc7914_v2_t1 —— 用**生产函数**对 T_1（前 64 hex）做
//     **逐字节相等**断言（不是比长度、不是比前缀后的部分匹配）。这直接钉死
//     HMAC key=password、ipad/opad 结构、以及 INT32BE(1) 的块序号与字节序。
//   * rfc7914_dklen64_block_counter —— 用本文件自带的、**仅基于产品 `sha256()`
//     原语**的参考实现对**完整 128 hex** 做断言，覆盖第 2 块与 INT32BE(i) 的
//     块计数递增。它证明「产品 SHA-256 原语正确 + RFC 8018 块结构理解正确」，
//     但**不**构成对 `pbkdf2_sha256()` 多块能力的覆盖——该函数没有多块接口
//     （契约 §3.1:95 明确 dkLen=32）。此限制已在 T2 报告中如实标注。
//
// 本文件**只** #include "../sha256.h"（全文 #include 行数 = 1）：
//   * 不引入任何第三方或额外头文件 —— HTTP 库、JSON 库、SQLite 均不需要，
//     测试可在无网络栈、无 DB 的环境下编译；
//   * 用到的 std::string / std::printf 等由 sha256.h 的传递包含提供，
//     刻意不额外 include，以便「只依赖 sha256.h」这一条可被机械核对：
//       Select-String -Path tests/pbkdf2_rfc7914_test.cpp -Pattern '^\s*#\s*include'
// ===========================================================================

#include "../sha256.h"

// ---------------------------------------------------------------------------
// 期望值来源纪律（队长 2026 更正要求，务必遵守）
//   * 本文件**所有期望值只来自 RFC 7914 §11 原文**（rfc-editor.org / datatracker
//     正文，并经 .NET Rfc2898DeriveBytes 与 CPython hashlib 两个独立实现复算一致）。
//     没有任何期望值取自本仓库实现的输出 —— 那是循环论证，等于回归网自己给自己发证。
//   * 下方 ref_* 参考实现只用于**产出被测值**并交叉验证「产品 sha256() 原语 + RFC 8018
//     块结构」是否自洽，**绝不**用于产出期望值。
//   * dkLen=64 必须正好 **128 个 hex 字符**（= 64 字节）。长度本身就是最硬的证伪判据：
//     任何被截断/污染的向量（例如只有 95 / 104 个 hex 字符的版本）在此**编译期**即被拒。
// ---------------------------------------------------------------------------

// ===========================================================================
// 极简断言框架（无第三方依赖）
// ===========================================================================

// RFC 7914 §11 向量 1：P="passwd", S="salt", c=1, dkLen=64
// 用 constexpr 数组而非指针，以便对长度与前缀关系做编译期断言（见下方 static_assert）。
static constexpr char RFC7914_V1_DK64[] =
    "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
    "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783";
// 其前 32 字节（T_1）—— 冻结的 3 参生产函数 dkLen=32 应逐字节等于此值
static constexpr char RFC7914_V1_T1[] =
    "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc";

// RFC 7914 §11 向量 2：P="Password", S="NaCl", c=80000, dkLen=64
static constexpr char RFC7914_V2_DK64[] =
    "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
    "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d";
static constexpr char RFC7914_V2_T1[] =
    "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56";

// 编译期硬门禁：dkLen=64 ⇒ 128 个 hex 字符；T_1 必须是 DK64 的前 64 个 hex 字符。
constexpr bool hex_is_prefix(const char* big, const char* small) {
    for (int i = 0; small[i] != '\0'; i++)
        if (big[i] != small[i]) return false;
    return true;
}
static_assert(sizeof(RFC7914_V1_DK64) - 1 == 128,
              "RFC 7914 §11 向量 1 必须是 128 个 hex 字符（dkLen=64）；被截断的向量在此即被拒");
static_assert(sizeof(RFC7914_V2_DK64) - 1 == 128,
              "RFC 7914 §11 向量 2 必须是 128 个 hex 字符（dkLen=64）；被截断的向量在此即被拒");
static_assert(sizeof(RFC7914_V1_T1) - 1 == 64, "向量 1 的 T_1 必须是 64 个 hex 字符");
static_assert(sizeof(RFC7914_V2_T1) - 1 == 64, "向量 2 的 T_1 必须是 64 个 hex 字符");
static_assert(hex_is_prefix(RFC7914_V1_DK64, RFC7914_V1_T1),
              "向量 1 的 T_1 必须是 DK64 的前 64 个 hex 字符");
static_assert(hex_is_prefix(RFC7914_V2_DK64, RFC7914_V2_T1),
              "向量 2 的 T_1 必须是 DK64 的前 64 个 hex 字符");

static int g_checks = 0;        // 已执行断言总数
static int g_check_fails = 0;   // 失败断言总数
static int g_case_fails = 0;    // 当前用例内的失败断言数
static const char* g_case = ""; // 当前用例名

static void report_fail(const char* what, const std::string& expected,
                        const std::string& actual) {
    g_check_fails++;
    g_case_fails++;
    std::printf("      [FAIL] %s: %s\n", g_case, what);
    std::printf("             expected: %s\n", expected.c_str());
    std::printf("             actual  : %s\n", actual.c_str());
}

// 逐字节相等（std::string 的 == 是全串比较；不做长度或前缀放宽）
static void expect_str(const char* what, const std::string& expected,
                       const std::string& actual) {
    g_checks++;
    if (expected == actual) return;
    report_fail(what, expected, actual);
}

static void expect_bool(const char* what, bool expected, bool actual) {
    g_checks++;
    if (expected == actual) return;
    report_fail(what, expected ? "true" : "false", actual ? "true" : "false");
}

static void expect_ne_str(const char* what, const std::string& a,
                          const std::string& b) {
    g_checks++;
    if (a != b) return;
    report_fail(what, "<两个不同的值>", "两者相同: " + a);
}

static bool is_lower_hex(const std::string& s, size_t wantLen) {
    if (s.size() != wantLen) return false;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) return false;
    }
    return true;
}

static std::string raw_to_hex(const std::string& raw) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < raw.size(); i++) {
        unsigned char b = (unsigned char)raw[i];
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

static std::string hex_to_raw(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int hi = (hex[i] >= '0' && hex[i] <= '9') ? hex[i] - '0'
                 : (hex[i] >= 'a' && hex[i] <= 'f') ? hex[i] - 'a' + 10 : 0;
        int lo = (hex[i + 1] >= '0' && hex[i + 1] <= '9') ? hex[i + 1] - '0'
                 : (hex[i + 1] >= 'a' && hex[i + 1] <= 'f') ? hex[i + 1] - 'a' + 10 : 0;
        out.push_back((char)((hi << 4) | lo));
    }
    return out;
}

// ===========================================================================
// 参考实现（RFC 8018 §5.2）：**只**基于产品原语 sha256()。
// 仅用于覆盖 dkLen=64 的第二块与 INT32BE(i) 块计数，不作为产品实现的替代品。
// ===========================================================================

static std::string ref_hmac_sha256(const std::string& key, const std::string& msg) {
    const size_t BLOCK = 64;
    std::string k = key;
    if (k.size() > BLOCK) k = hex_to_raw(sha256(k)); // 长 key 先做 SHA256 压缩
    k.resize(BLOCK, '\0');                           // 右侧补 0 到 64 字节
    std::string ipad, opad;
    for (size_t i = 0; i < BLOCK; i++) {
        ipad.push_back((char)((unsigned char)k[i] ^ 0x36));
        opad.push_back((char)((unsigned char)k[i] ^ 0x5c));
    }
    std::string inner = hex_to_raw(sha256(ipad + msg));
    return sha256(opad + inner);
}

static std::string ref_pbkdf2_hmac_sha256(const std::string& pw,
                                          const std::string& salt, int c,
                                          int dkLen) {
    std::string out;
    int blocks = (dkLen + 31) / 32;
    for (int i = 1; i <= blocks; i++) {
        std::string idx;
        idx.push_back((char)((i >> 24) & 0xFF)); // INT32BE(i)：块序号大端
        idx.push_back((char)((i >> 16) & 0xFF));
        idx.push_back((char)((i >> 8) & 0xFF));
        idx.push_back((char)(i & 0xFF));
        std::string u = hex_to_raw(ref_hmac_sha256(pw, salt + idx));
        std::string t = u;
        for (int j = 1; j < c; j++) {
            u = hex_to_raw(ref_hmac_sha256(pw, u));
            for (size_t b = 0; b < t.size(); b++)
                t[b] = (char)((unsigned char)t[b] ^ (unsigned char)u[b]);
        }
        out += t;
    }
    out.resize((size_t)dkLen);
    return raw_to_hex(out);
}

// ===========================================================================
// 多块入口探测（编译期）—— 把「第 2 块 + INT32BE(i) 块计数」钉在实现上
// ---------------------------------------------------------------------------
// RFC 7914 §11 两组向量的 dkLen 都是 64，即 DK = T_1 || T_2（两块）。而契约 §3.1
// 冻结的 3 参接口 pbkdf2_sha256(password, salt, iterations) 只产 dkLen=32，
// **无法表达 dkLen=64** —— 只实现单块的实现在 T_1 断言上可以完全通过，第 2 块与
// INT32BE(2) 写错也测不出来。为堵住这个洞，本文件在**编译期**探测 sha256.h 是否
// 提供了带 dkLen 的多块入口：
//     pbkdf2_sha256(const std::string& password, const std::string& salt,
//                   int iterations, int dkLen)   → 返回 2*dkLen 个字符的小写 hex
//   * 探测到 → 对**完整 128 hex 逐字节相等**做断言（不是前缀相等、不是长度相等）；
//   * 探测不到 → **明确失败**并打印所需签名，绝不静默跳过（契约 §3.1:145）。
// 这样「单块实现」在两组向量上必然挂掉，且缺口永远不会是静默的。
// ===========================================================================

namespace mb_probe {
struct yes_t { char pad[2]; };
struct no_t { char pad[1]; };
// 若存在 4 参重载，下面这个候选的返回类型推导成功；否则被 SFINAE 剔除。
template <typename P, typename S, typename C, typename D>
auto probe(int) -> decltype(pbkdf2_sha256(*(const P*)nullptr, *(const S*)nullptr,
                                          *(const C*)nullptr, *(const D*)nullptr),
                            yes_t());
template <typename P, typename S, typename C, typename D>
no_t probe(...);
} // namespace mb_probe

static const bool kHasMultiBlockDkLen =
    sizeof(mb_probe::probe<std::string, std::string, int, int>(0)) ==
    sizeof(mb_probe::yes_t);

// 依赖模板参数 Str，使 4 参调用只在**本函数模板被实例化**时才解析与检查；
// 未提供多块入口时 kHasMultiBlockDkLen 为 false，discarded 分支不被实例化。
template <typename Str>
static void dklen64_from_production() {
    if constexpr (kHasMultiBlockDkLen) {
        Str pw1 = "passwd", s1 = "salt";
        std::string dk1 = pbkdf2_sha256(pw1, s1, 1, 64);
        expect_bool("dkLen=64 的返回值必须**恰好 128 位小写 hex**（只实现单块的会返回 64 位）",
                    true, is_lower_hex(dk1, 128));
        expect_str("production: pbkdf2_sha256(\"passwd\",\"salt\",1,dkLen=64) "
                   "== RFC 7914 §11 v1 全 128 hex（两块）",
                   RFC7914_V1_DK64, dk1);
        Str pw2 = "Password", s2 = "NaCl";
        std::string dk2 = pbkdf2_sha256(pw2, s2, 80000, 64);
        expect_bool("dkLen=64 的返回值必须**恰好 128 位小写 hex**（第二组同样是两块）",
                    true, is_lower_hex(dk2, 128));
        expect_str("production: pbkdf2_sha256(\"Password\",\"NaCl\",80000,dkLen=64) "
                   "== RFC 7914 §11 v2 全 128 hex（两块）",
                   RFC7914_V2_DK64, dk2);
    } else {
        expect_bool("sha256.h 缺少 dkLen 多块入口 —— 需提供 pbkdf2_sha256(const "
                    "std::string& password, const std::string& salt, int iterations, "
                    "int dkLen)（返回 2*dkLen 个字符的小写 hex）。RFC 7914 §11 的 "
                    "dkLen=64 需要两块派生，无法用 32 字节接口表达；本项不得静默跳过",
                    true, false);
    }
}

// ===========================================================================
// 用例
// ===========================================================================

// --- 正向对照：SHA-256 原语本身是否正确 -------------------------------------
// 出处：FIPS 180-4 / RFC 6234 §8.1 的 SHA-256("abc") 与 SHA-256("") 示例值。
// 作用：把 B1 的失败**定位**到 PBKDF2/HMAC 层，排除「哈希原语坏了」这一混淆项。
// 预期：修复前后都通过。
static void case_sha256_primitive() {
    expect_str("sha256(\"\")  [FIPS 180-4]",
               "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
               sha256(""));
    expect_str("sha256(\"abc\") [RFC 6234 §8.1]",
               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               sha256("abc"));
}

// --- RFC 7914 §11 向量 1：生产函数 vs T_1 -----------------------------------
// 修复前必须失败（B1 基线证据）。
static void case_rfc7914_v1_t1() {
    std::string dk = pbkdf2_sha256("passwd", "salt", 1);
    expect_str("pbkdf2_sha256(\"passwd\",\"salt\",1) == RFC 7914 §11 v1 的前 32 字节",
               RFC7914_V1_T1, dk);
    expect_bool("pbkdf2_sha256 输出必须是 64 位小写 hex", true,
                is_lower_hex(dk, 64));
}

// --- RFC 7914 §11 向量 2：生产函数 vs T_1（c=80000） ------------------------
// 修复前必须失败（B1 基线证据）。
static void case_rfc7914_v2_t1() {
    std::string dk = pbkdf2_sha256("Password", "NaCl", 80000);
    expect_str("pbkdf2_sha256(\"Password\",\"NaCl\",80000) == RFC 7914 §11 v2 的前 32 字节",
               RFC7914_V2_T1, dk);
    expect_bool("pbkdf2_sha256 输出必须是 64 位小写 hex", true,
                is_lower_hex(dk, 64));
}

// --- RFC 7914 §11 完整 dkLen=64（两块）与 INT32BE(i) 块计数 -----------------
// 走本文件自带参考实现（仅用产品 sha256()）；修复前后都应通过。
static void case_rfc7914_dklen64_block_counter() {
    expect_str("ref: PBKDF2-HMAC-SHA256(passwd,salt,1,dkLen=64) 全 128 hex",
               RFC7914_V1_DK64,
               ref_pbkdf2_hmac_sha256("passwd", "salt", 1, 64));
    expect_str("ref: PBKDF2-HMAC-SHA256(Password,NaCl,80000,dkLen=64) 全 128 hex",
               RFC7914_V2_DK64,
               ref_pbkdf2_hmac_sha256("Password", "NaCl", 80000, 64));
}

// --- RFC 7914 §11 完整 128 hex：对**生产函数**的多块（dkLen=64）逐字节断言 ----
// 队长要求：两组向量必须对实现断言完整 128 个 hex 字符逐字节相等（不是前缀相等、
// 不是长度相等），从而把第 2 块与 INT32BE(i) 的块计数递增钉死在实现上。
// 在 sha256.h 提供 dkLen 多块入口之前，本用例**必须失败**（不得静默跳过）。
static void case_rfc7914_dklen64_from_production() {
    dklen64_from_production<std::string>();
}

// --- B1 行为用例①：同一 salt 下不同口令必须派生不同 dk ----------------------
// 这条直接钉死「password 形参零引用」。修复前必须失败。
static void case_b1_distinct_password_distinct_dk() {
    const std::string SALT = "b1-shared-salt";
    std::string dk_alpha = pbkdf2_sha256("alpha-pw", SALT, 1000);
    std::string dk_beta = pbkdf2_sha256("beta-pw", SALT, 1000);
    std::string dk_empty = pbkdf2_sha256("", SALT, 1000);
    expect_ne_str("同一 salt 下 pbkdf2_sha256(\"alpha-pw\") != pbkdf2_sha256(\"beta-pw\")",
                  dk_alpha, dk_beta);
    expect_ne_str("同一 salt 下 pbkdf2_sha256(\"\") != pbkdf2_sha256(\"alpha-pw\")",
                  dk_empty, dk_alpha);
    expect_bool("空口令派生结果仍须是 64 位小写 hex", true,
                is_lower_hex(dk_empty, 64));
}

// --- B1 边界：口令长度 64 与 65（>64 必须先 SHA256 压缩为 key） --------------
// 契约 §3.1「边界与反例」。修复前 password 被忽略 → 两者相同 → 必须失败。
static void case_b1_password_len_64_vs_65() {
    std::string pw64(64, 'A');
    std::string pw65 = pw64 + "X";
    std::string dk64 = pbkdf2_sha256(pw64, "len-salt", 500);
    std::string dk65 = pbkdf2_sha256(pw65, "len-salt", 500);
    expect_bool("64 字节口令的输出为 64 位小写 hex", true, is_lower_hex(dk64, 64));
    expect_ne_str("口令 64 字节与 65 字节必须派生不同 dk（65 字节走 K=SHA256(key) 分支）",
                  dk64, dk65);
}

// --- B1 行为用例②：verify_password 对错误口令必须 false ---------------------
// 修复前：错误口令/空口令都会恒返回 true（完整认证绕过）→ 必须失败。
static void case_b1_verify_password_correct_and_wrong() {
    const std::string pw = "Correct-Horse-9";
    std::string stored = hash_password(pw);
    expect_bool("verify_password(正确口令, hash_password(该口令)) == true", true,
                verify_password(pw, stored));
    expect_bool("verify_password(错误口令, 同一存储串) == false", false,
                verify_password("definitely-wrong", stored));
    expect_bool("verify_password(空口令, 同一存储串) == false", false,
                verify_password("", stored));
}

// --- fail-closed：畸形存储串一律 false（契约 §3.1 V1-5） --------------------
// 修复前后都应通过；T4 加固 verify_password 时不得回归。
static void case_verify_password_malformed_fail_closed() {
    static const char* labels[] = {
        "空串",
        "仅前缀 \"pbkdf2$\"",
        "迭代次数为 0",
        "迭代次数非数字 \"abc\"",
        "缺少 dk 字段（只有 2 个 $）",
        "dk 为非法十六进制 \"zzzz\"",
        "salt 段为空",
        "完全不是哈希格式的随意串"
    };
    static const char* hashes[] = {
        "",
        "pbkdf2$",
        "pbkdf2$0$00112233445566778899aabbccddeeff$deadbeef",
        "pbkdf2$abc$00112233445566778899aabbccddeeff$deadbeef",
        "pbkdf2$100000$00112233445566778899aabbccddeeff",
        "pbkdf2$100000$00112233445566778899aabbccddeeff$zzzz",
        "pbkdf2$100000$$00112233445566778899aabbccddeeff",
        "not-a-hash-at-all"
    };
    for (int i = 0; i < 8; i++) {
        expect_bool(labels[i], false, verify_password("anything", hashes[i]));
    }
}

// --- 存储格式契约（契约 §3.1 V1-6）：pbkdf2$100000$<32hex>$<64hex> ----------
// 修复前后都应通过；T4 重写实现时不得改变持久化格式。
static void case_hash_password_storage_format() {
    const std::string pw = "Storage-Format-Check-1";
    std::string h1 = hash_password(pw);
    std::string h2 = hash_password(pw);
    expect_str("hash_password 前缀为 \"pbkdf2$100000$\"", "pbkdf2$100000$",
               h1.substr(0, 14));
    size_t p1 = h1.find('$');
    size_t p2 = (p1 == std::string::npos) ? std::string::npos : h1.find('$', p1 + 1);
    size_t p3 = (p2 == std::string::npos) ? std::string::npos : h1.find('$', p2 + 1);
    bool four_fields = (p1 != std::string::npos && p2 != std::string::npos &&
                        p3 != std::string::npos &&
                        h1.find('$', p3 + 1) == std::string::npos);
    expect_bool("存储串必须恰有 3 个 '$' 分隔 4 个字段", true, four_fields);
    if (!four_fields) return; // 后续断言依赖字段切分，避免越界/误导
    expect_str("第 2 字段（迭代次数）", "100000", h1.substr(p1 + 1, p2 - p1 - 1));
    expect_bool("第 3 字段（salt）为 32 位小写 hex", true,
                is_lower_hex(h1.substr(p2 + 1, p3 - p2 - 1), 32));
    expect_bool("第 4 字段（dk）为 64 位小写 hex", true,
                is_lower_hex(h1.substr(p3 + 1), 64));
    expect_ne_str("两次 hash_password 的 salt 段必须不同",
                  h1.substr(p2 + 1, p3 - p2 - 1), h2.substr(p2 + 1, p3 - p2 - 1));
}

// --- 旧版裸 SHA256 兼容分支（契约 §3.1 V1-7） ------------------------------
// 修复前后都应通过：登录成功后由调用方升级，兼容分支本身不得被改坏。
static void case_verify_password_legacy_sha256() {
    std::string legacy = sha256("Legacy-Pw-1");
    expect_bool("legacy 裸 SHA256：正确口令 == true", true,
                verify_password("Legacy-Pw-1", legacy));
    expect_bool("legacy 裸 SHA256：错误口令 == false", false,
                verify_password("nope", legacy));
}

// ===========================================================================
// 调度
// ===========================================================================

struct TestCase {
    const char* name;
    void (*fn)();
};

static const TestCase g_cases[] = {
    {"sha256_fips180_4_primitive", case_sha256_primitive},
    {"rfc7914_v1_t1_passwd_salt_c1", case_rfc7914_v1_t1},
    {"rfc7914_v2_t1_Password_NaCl_c80000", case_rfc7914_v2_t1},
    {"rfc7914_dklen64_block_counter", case_rfc7914_dklen64_block_counter},
    {"rfc7914_dklen64_from_production", case_rfc7914_dklen64_from_production},
    {"b1_distinct_password_distinct_dk", case_b1_distinct_password_distinct_dk},
    {"b1_password_len_64_vs_65", case_b1_password_len_64_vs_65},
    {"b1_verify_password_wrong_password_false",
     case_b1_verify_password_correct_and_wrong},
    {"verify_password_malformed_fail_closed",
     case_verify_password_malformed_fail_closed},
    {"hash_password_storage_format", case_hash_password_storage_format},
    {"verify_password_legacy_sha256", case_verify_password_legacy_sha256},
};

int main(int argc, char** argv) {
    std::string only;
    bool listOnly = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--only" && i + 1 < argc) {
            only = argv[++i];
        } else if (a.rfind("--only=", 0) == 0) {
            only = a.substr(7);
        } else if (a == "--list") {
            listOnly = true;
        } else {
            std::printf("usage: pbkdf2_test.exe [--list] [--only <name-substring>]\n");
            return 2;
        }
    }

    if (listOnly) {
        for (size_t i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++)
            std::printf("%s\n", g_cases[i].name);
        return 0;
    }

    std::printf("=== tests/pbkdf2_rfc7914_test.cpp ===\n");
    std::printf("目标：以 RFC 7914 §11 标准向量机械判定 B1（PBKDF2 认证绕过）\n");
    std::printf("filter: %s\n\n", only.empty() ? "(none)" : only.c_str());

    int cases_run = 0, cases_ok = 0;
    for (size_t i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
        std::string nm = g_cases[i].name;
        if (!only.empty() && nm.find(only) == std::string::npos) continue;
        cases_run++;
        g_case = g_cases[i].name;
        g_case_fails = 0;
        int before = g_checks;
        std::printf("--- [RUN ] %s\n", g_cases[i].name);
        g_cases[i].fn();
        int ran = g_checks - before;
        if (g_case_fails == 0) {
            cases_ok++;
            std::printf("--- [ OK ] %s  (%d checks)\n", g_cases[i].name, ran);
        } else {
            std::printf("--- [FAIL] %s  (%d of %d checks failed)\n",
                        g_cases[i].name, g_case_fails, ran);
        }
    }

    if (cases_run == 0) {
        // 明确禁止「筛选不到即静默通过」（契约 §3.1:145）
        std::printf("ERROR: --only \"%s\" 未匹配任何用例；不得静默跳过\n",
                    only.c_str());
        return 2;
    }

    std::printf("\n============================================================\n");
    std::printf("cases : %d run, %d ok, %d failed\n", cases_run, cases_ok,
                cases_run - cases_ok);
    std::printf("checks: %d run, %d ok, %d failed\n", g_checks,
                g_checks - g_check_fails, g_check_fails);
    if (g_check_fails == 0) {
        std::printf("RESULT: PASS\n");
        return 0;
    }
    std::printf("RESULT: FAIL\n");
    return 1;
}
