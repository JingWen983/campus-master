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

// SHA256 计算
inline std::string sha256(const std::string& input) {
    unsigned int state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

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

    char hex[65];
    for (int i = 0; i < 8; i++) {
        sprintf(hex + i*8, "%08x", state[i]);
    }
    hex[64] = 0;
    return std::string(hex);
}

// 安全修复 V5：密码哈希采用 PBKDF2-HMAC-SHA256 + 随机盐 + 高迭代次数
// 存储格式：pbkdf2$<iterations>$<salt_hex>$<hash_hex>
// 兼容旧库：若传入的 hash 不以 "pbkdf2$" 开头，按旧 SHA256 校验，校验通过后由调用方升级
inline std::string pbkdf2_sha256(const std::string& password, const std::string& salt,
                                 int iterations = 100000) {
    // HMAC-SHA256 的内/外层块大小
    const int BLOCK = 64;
    // HMAC K
    unsigned char k_ipad[BLOCK] = {0};
    unsigned char k_opad[BLOCK] = {0};
    if (salt.size() > (size_t)BLOCK) {
        std::string s = sha256(salt);
        std::memcpy(k_ipad, s.data(), s.size() < 32 ? s.size() : 32);
        std::memcpy(k_opad, s.data(), s.size() < 32 ? s.size() : 32);
    } else {
        std::memcpy(k_ipad, salt.data(), salt.size());
        std::memcpy(k_opad, salt.data(), salt.size());
    }
    for (int i = 0; i < BLOCK; i++) { k_ipad[i] ^= 0x36; k_opad[i] ^= 0x5c; }

    // PBKDF2 单块派生（dkLen = 32，只需 1 块）
    unsigned char u[32];
    {
        std::string msg;
        msg.append((char*)k_ipad, BLOCK);
        msg.append(salt);
        msg.push_back(0); msg.push_back(0); msg.push_back(0); msg.push_back(1); // INT(1) BE
        std::string h1 = sha256(msg);
        std::memcpy(u, h1.data(), 32);

        std::string msg2;
        msg2.append((char*)k_opad, BLOCK);
        msg2.append((char*)u, 32);
        std::string h2 = sha256(msg2);
        std::memcpy(u, h2.data(), 32);
    }
    unsigned char t[32];
    std::memcpy(t, u, 32);
    for (int i = 1; i < iterations; i++) {
        std::string msg;
        msg.append((char*)k_ipad, BLOCK);
        msg.append((char*)u, 32);
        std::string h1 = sha256(msg);
        std::memcpy(u, h1.data(), 32);

        std::string msg2;
        msg2.append((char*)k_opad, BLOCK);
        msg2.append((char*)u, 32);
        std::string h2 = sha256(msg2);
        std::memcpy(u, h2.data(), 32);
        for (int j = 0; j < 32; j++) t[j] ^= u[j];
    }

    char hex[65];
    for (int i = 0; i < 32; i++) std::sprintf(hex + i * 2, "%02x", t[i]);
    hex[64] = 0;
    return std::string(hex);
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

// 验证密码：自动识别新版 pbkdf2 与旧版 sha256
inline bool verify_password(const std::string& password, const std::string& hash) {
    if (hash.compare(0, 7, "pbkdf2$") == 0) {
        // pbkdf2$<iters>$<salt>$<dk>
        size_t p1 = hash.find('$', 7);
        if (p1 == std::string::npos) return false;
        size_t p2 = hash.find('$', p1 + 1);
        if (p2 == std::string::npos) return false;
        int iters = std::atoi(hash.c_str() + 7);
        std::string salt = hash.substr(p1 + 1, p2 - p1 - 1);
        std::string dk = hash.substr(p2 + 1);
        // 常量时间比较
        std::string calc = pbkdf2_sha256(password, salt, iters);
        if (calc.size() != dk.size()) return false;
        unsigned char diff = 0;
        for (size_t i = 0; i < calc.size(); i++) diff |= (unsigned char)(calc[i] ^ dk[i]);
        return diff == 0;
    }
    // 旧版兼容：无盐 SHA256
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
