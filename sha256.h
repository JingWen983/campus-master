#ifndef SHA256_H
#define SHA256_H

#include <string>
#include <cstring>
#include <cstdio>
#include <random>
#include <sstream>
#include <iomanip>

// SHA256 常量
static const unsigned int SHA256_K[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// SHA256 右旋转
inline unsigned int rotr(unsigned int x, unsigned int n) { return (x >> n) | (x << (32 - n)); }

// SHA256 压缩函数
inline void sha256_compress(unsigned int state[8], const unsigned char block[64]) {
    unsigned int w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (block[i*4] << 24) | (block[i*4+1] << 16) | (block[i*4+2] << 8) | block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        unsigned int s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        unsigned int s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    unsigned int a = state[0], b = state[1], c = state[2], d = state[3];
    unsigned int e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
        unsigned int s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        unsigned int ch = (e & f) ^ (~e & g);
        unsigned int temp1 = h + s1 + ch + SHA256_K[i] + w[i];
        unsigned int s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

// SHA256 摘要的共享核心：完成填充与压缩，输出 8 个状态字（即原始摘要）。
// 供 sha256()（hex 文本）与 sha256_bytes()（原始 32 字节）共用，确保两者语义严格一致。
// 注意：本函数按 input.size() 处理，可安全承载含 0x00 的原始字节消息。
inline void sha256_state(const std::string& input, unsigned int state[8]) {
    state[0] = 0x6a09e667; state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
    state[4] = 0x510e527f; state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;

    size_t len = input.length();
    size_t blocks = (len + 9 + 63) / 64;
    unsigned char* msg = new unsigned char[blocks * 64];
    memset(msg, 0, blocks * 64);
    memcpy(msg, input.c_str(), len);
    msg[len] = 0x80;

    // SHA256 要求消息长度为 64 位大端整数
    unsigned long long bit_len = len * 8;
    for (int i = 0; i < 8; i++) {
        msg[blocks * 64 - 8 + i] = (bit_len >> (56 - i * 8)) & 0xFF;
    }

    for (size_t i = 0; i < blocks; i++) {
        sha256_compress(state, msg + i * 64);
    }

    delete[] msg;
}

// SHA256 原始摘要：32 字节，用 std::string 承载原始字节（可能含 0x00）。
// 安全修复 V17（B1 缺陷 C）：HMAC/PBKDF2 必须使用本函数，
// **绝不允许**再把 sha256() 返回的 64 字符 hex 文本当作原始摘要字节使用
// （旧实现 `memcpy(u, h1.data(), 32)` 正是此错：摘要被截断成 16 字节 + 双重 hex 编码）。
inline std::string sha256_bytes(const std::string& input) {
    unsigned int state[8];
    sha256_state(input, state);
    unsigned char out[32];
    for (int i = 0; i < 8; i++) {
        out[i * 4 + 0] = (unsigned char)((state[i] >> 24) & 0xFF);
        out[i * 4 + 1] = (unsigned char)((state[i] >> 16) & 0xFF);
        out[i * 4 + 2] = (unsigned char)((state[i] >> 8) & 0xFF);
        out[i * 4 + 3] = (unsigned char)(state[i] & 0xFF);
    }
    return std::string((const char*)out, 32);
}

// SHA256 计算（64 字符小写 hex）
// 保持既有行为完全不变：旧版裸 SHA256 存储串的兼容分支依赖它。
inline std::string sha256(const std::string& input) {
    unsigned int state[8];
    sha256_state(input, state);
    char hex[65];
    for (int i = 0; i < 8; i++) {
        sprintf(hex + i*8, "%08x", state[i]);
    }
    hex[64] = 0;
    return std::string(hex);
}

// ===========================================================================
// 安全修复 V17（B1 修复）：PBKDF2-HMAC-SHA256 按 RFC 8018 §5.2 重写
//
// 旧实现在同一函数里叠加了 **4 处** 缺陷，任一处都足以让 RFC 7914 §11 向量失败：
//   A) HMAC 的 key 取自 salt，`password` 形参在函数体内**零引用**；
//   B) 内层写成 SHA256(k_ipad || salt || INT32BE(1))，外层同理 —— 根本不是 HMAC 结构；
//   C) `memcpy(u, h1.data(), 32)` 把 sha256() 的 **hex 文本**当原始摘要字节取：
//      摘要被截断成 16 字节，且最后又 hex 编码一次（**双重 hex 编码**）；
//   D) `salt.size() > 64` 的压缩分支同错，且 dkLen 被硬编码为 32 字节（单块）。
//
// 正确语义（RFC 8018 §5.2 / RFC 6234 HMAC）：
//   U_1 = HMAC-SHA256(key = password, msg = salt || INT32BE(1))
//   U_i = HMAC-SHA256(key = password, msg = U_{i-1})        i = 2..c
//   T_i = U_1 XOR U_2 XOR ... XOR U_c                        （32 字节）
//   DK  = T_1 || T_2 || ... 并截断到 dkLen
// ===========================================================================

// HMAC-SHA256，返回**原始 32 字节**（不是 hex）。
// key 超过 64 字节时先 SHA256 压缩；ipad/opad 的异或作用在 64 字节 key 块上。
inline std::string hmac_sha256(const std::string& key, const std::string& msg) {
    const size_t BLOCK = 64;
    std::string k = key;
    if (k.size() > BLOCK) k = sha256_bytes(k);
    k.resize(BLOCK, '\0');                      // 右侧补 0 至 64 字节

    std::string ipad(BLOCK, '\0'), opad(BLOCK, '\0');
    for (size_t i = 0; i < BLOCK; i++) {
        ipad[i] = (char)((unsigned char)k[i] ^ 0x36);
        opad[i] = (char)((unsigned char)k[i] ^ 0x5c);
    }
    std::string inner = sha256_bytes(ipad + msg);
    return sha256_bytes(opad + inner);
}

// PBKDF2-HMAC-SHA256：输出 2*dkLen 个小写十六进制字符。
//   * iterations <= 0 或 dkLen <= 0 时**显式拒绝**，返回空串（不得返回可用哈希）；
//   * dkLen 可配，4 参入口正是 RFC 7914 §11 的 dkLen=64（两块）向量所需，
//     它同时钉死 INT32BE(i) 的块计数递增 —— 只实现单块必挂；
//   * 3 参调用行为不变：dkLen 默认 32 → 64 个 hex 字符，存储格式不受影响。
inline std::string pbkdf2_sha256(const std::string& password, const std::string& salt,
                                 int iterations = 100000, int dkLen = 32) {
    if (iterations <= 0 || dkLen <= 0) return std::string();

    std::string out;
    int blocks = (dkLen + 31) / 32;
    for (int i = 1; i <= blocks; i++) {
        // INT32BE(i)：块序号按 4 字节大端拼接
        std::string idx;
        idx.push_back((char)((i >> 24) & 0xFF));
        idx.push_back((char)((i >> 16) & 0xFF));
        idx.push_back((char)((i >> 8) & 0xFF));
        idx.push_back((char)(i & 0xFF));

        std::string u = hmac_sha256(password, salt + idx);   // U_1
        std::string t = u;                                   // T = U_1
        for (int j = 1; j < iterations; j++) {               // iterations=1 时一次都不执行
            u = hmac_sha256(password, u);                    // U_j
            for (size_t b = 0; b < t.size(); b++) {
                t[b] = (char)((unsigned char)t[b] ^ (unsigned char)u[b]);
            }
        }
        out += t;
    }
    out.resize((size_t)dkLen);                               // 第二块只取所需字节

    // 只在最后做一次 hex 编码（旧实现的双重 hex 即由此消除）
    static const char* digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(out.size() * 2);
    for (size_t i = 0; i < out.size(); i++) {
        unsigned char b = (unsigned char)out[i];
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 0x0F]);
    }
    return hex;
}

// 安全修复 V4：CSPRNG —— 使用 std::random_device 生成随机字节
inline std::string generate_random_hex(int bytes) {
    std::random_device rd;
    std::ostringstream oss;
    for (int i = 0; i < bytes; i++) {
        unsigned char b = (unsigned char)(rd() & 0xFF);
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

// 简单的密码哈希函数（新版带盐）
inline std::string hash_password(const std::string& password) {
    std::string salt = generate_random_hex(16);
    std::string dk = pbkdf2_sha256(password, salt);
    return "pbkdf2$100000$" + salt + "$" + dk;
}

// 验证密码：自动识别新版 pbkdf2 与旧版 sha256。
// 安全修复 V17（B1）：pbkdf2$ 分支 **fail-closed** —— 任何畸形/不可信输入一律 false，
// 不抛异常、也绝不恒真。严格性要点：
//   * 空哈希恒 false；
//   * 迭代次数**严格解析**（仅 '0'..'9' 且长度 1..9），拒绝 atoi 的非数字→0 与溢出 UB；
//   * iters <= 0 或超过上限（10000000，防 DoS）一律 false；
//   * 字段必须齐备；salt 非空；dk 必须恰好 64 位小写 hex（即 32 字节）；
//   * 与计算值做常量时间比较。
// 兼容分支：非 pbkdf2$ 前缀的裸 SHA256 存储串仍按 sha256(password) == hash 判定。
inline bool verify_password(const std::string& password, const std::string& hash) {
    if (hash.empty()) return false;

    if (hash.compare(0, 7, "pbkdf2$") == 0) {
        // pbkdf2$<iters>$<salt_hex>$<dk_hex>
        size_t p1 = hash.find('$', 7);
        if (p1 == std::string::npos) return false;
        size_t p2 = hash.find('$', p1 + 1);
        if (p2 == std::string::npos) return false;

        std::string iters_s = hash.substr(7, p1 - 7);
        if (iters_s.empty() || iters_s.size() > 9) return false;
        long iters = 0;
        for (size_t i = 0; i < iters_s.size(); i++) {
            if (iters_s[i] < '0' || iters_s[i] > '9') return false;
            iters = iters * 10 + (iters_s[i] - '0');
        }
        if (iters <= 0 || iters > 10000000L) return false;

        std::string salt = hash.substr(p1 + 1, p2 - p1 - 1);
        std::string dk = hash.substr(p2 + 1);
        if (salt.empty() || dk.size() != 64) return false;
        for (size_t i = 0; i < dk.size(); i++) {
            char c = dk[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        }

        std::string calc = pbkdf2_sha256(password, salt, (int)iters);
        if (calc.size() != dk.size()) return false;
        // 常量时间比较
        unsigned char diff = 0;
        for (size_t i = 0; i < calc.size(); i++) diff |= (unsigned char)(calc[i] ^ dk[i]);
        return diff == 0;
    }
    // 旧版兼容：无盐 SHA256（登录成功后由调用方升级为 pbkdf2）
    return sha256(password) == hash;
}

// 获取当前时间字符串
inline std::string get_current_time() {
    time_t now = time(nullptr);
    tm* tm_info = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm_info);
    return std::string(buffer);
}

// 安全修复 V4：生成随机密码（8-12位，包含大小写字母和数字），使用 CSPRNG
inline std::string generate_random_password() {
    const std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    const std::string digits = "0123456789";
    const std::string all_chars = uppercase + lowercase + digits;

    std::random_device rd;
    std::uniform_int_distribution<int> len_dist(0, 4);
    int length = 8 + len_dist(rd);

    std::string password;
    password += uppercase[rd() % uppercase.length()];
    password += lowercase[rd() % lowercase.length()];
    password += digits[rd() % digits.length()];

    for (int i = 3; i < length; i++) {
        password += all_chars[rd() % all_chars.length()];
    }

    for (int i = 0; i < length; i++) {
        int j = rd() % length;
        char temp = password[i];
        password[i] = password[j];
        password[j] = temp;
    }

    return password;
}

#endif
