#ifndef SHA256_H
#define SHA256_H

#include <string>
#include <cstring>
#include <cstdio>

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

// 简单的密码哈希函数
inline std::string hash_password(const std::string& password) {
    return sha256(password);
}

// 验证密码
inline bool verify_password(const std::string& password, const std::string& hash) {
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

// 生成随机密码（8-12位，包含大小写字母和数字）
inline std::string generate_random_password() {
    const std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    const std::string digits = "0123456789";
    const std::string all_chars = uppercase + lowercase + digits;

    srand(time(nullptr));
    int length = 8 + rand() % 5; // 8-12位

    std::string password;
    password += uppercase[rand() % uppercase.length()];
    password += lowercase[rand() % lowercase.length()];
    password += digits[rand() % digits.length()];

    for (int i = 3; i < length; i++) {
        password += all_chars[rand() % all_chars.length()];
    }

    for (int i = 0; i < length; i++) {
        int j = rand() % length;
        char temp = password[i];
        password[i] = password[j];
        password[j] = temp;
    }

    return password;
}

#endif
