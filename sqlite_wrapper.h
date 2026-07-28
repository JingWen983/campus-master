#ifndef SQLITE_WRAPPER_H
#define SQLITE_WRAPPER_H

#include <sqlite3.h>

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include "json.hpp"

using json = nlohmann::json;

class SqliteDb {
public:
    SqliteDb() : db_(nullptr) {}
    ~SqliteDb() { close(); }

    bool open(const std::string& path) {
        int rc = sqlite3_open(path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        std::cout << "SQLite 数据库连接成功: " << path << std::endl;
        return true;
    }

    void close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool execute(const std::string& sql) {
        if (!db_) return false;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL错误: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    json query(const std::string& sql) {
        json result = json::array();
        if (!db_) return result;

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "查询失败: " << sqlite3_errmsg(db_) << std::endl;
            return result;
        }

        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json row;
            for (int i = 0; i < cols; i++) {
                const char* col_name = sqlite3_column_name(stmt, i);
                int col_type = sqlite3_column_type(stmt, i);

                switch (col_type) {
                    case SQLITE_INTEGER:
                        row[col_name] = sqlite3_column_int(stmt, i);
                        break;
                    case SQLITE_FLOAT:
                        row[col_name] = sqlite3_column_double(stmt, i);
                        break;
                    case SQLITE_TEXT:
                        row[col_name] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                        break;
                    case SQLITE_NULL:
                        row[col_name] = nullptr;
                        break;
                    default:
                        row[col_name] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                }
            }
            result.push_back(row);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    int insert(const std::string& sql) {
        if (!db_) return -1;
        if (execute(sql)) {
            return (int)sqlite3_last_insert_rowid(db_);
        }
        return -1;
    }

    int update(const std::string& sql) {
        if (!db_) return -1;
        if (execute(sql)) {
            return sqlite3_changes(db_);
        }
        return -1;
    }

    // ====== 安全修复 V2：参数化查询接口 ======
    // 绑定参数类型变体（int / double / string / null）
    struct Bind {
        enum Type { INT, DBL, STR, NUL } type = NUL;
        long long i = 0;
        double d = 0;
        std::string s;
        Bind() {}
        Bind(int v) : type(INT), i(v) {}
        Bind(long long v) : type(INT), i(v) {}
        Bind(double v) : type(DBL), d(v) {}
        Bind(const char* v) : type(STR), s(v ? v : "") {}
        Bind(const std::string& v) : type(STR), s(v) {}
        static Bind null() { return Bind(); }
    };

    // 参数化 execute：sql 中以 ? 占位，params 顺序绑定
    bool execute_bind(const std::string& sql, const std::vector<Bind>& params = {}) {
        if (!db_) return false;
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare 错误: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }
        for (size_t i = 0; i < params.size(); i++) {
            int idx = (int)i + 1;
            const auto& p = params[i];
            switch (p.type) {
                case Bind::INT: sqlite3_bind_int64(stmt, idx, p.i); break;
                case Bind::DBL: sqlite3_bind_double(stmt, idx, p.d); break;
                case Bind::STR: sqlite3_bind_text(stmt, idx, p.s.c_str(), (int)p.s.size(), SQLITE_TRANSIENT); break;
                case Bind::NUL: sqlite3_bind_null(stmt, idx); break;
            }
        }
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    // 参数化 query：返回 json 数组
    json query_bind(const std::string& sql, const std::vector<Bind>& params = {}) {
        json result = json::array();
        if (!db_) return result;
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "查询失败: " << sqlite3_errmsg(db_) << std::endl;
            return result;
        }
        for (size_t i = 0; i < params.size(); i++) {
            int idx = (int)i + 1;
            const auto& p = params[i];
            switch (p.type) {
                case Bind::INT: sqlite3_bind_int64(stmt, idx, p.i); break;
                case Bind::DBL: sqlite3_bind_double(stmt, idx, p.d); break;
                case Bind::STR: sqlite3_bind_text(stmt, idx, p.s.c_str(), (int)p.s.size(), SQLITE_TRANSIENT); break;
                case Bind::NUL: sqlite3_bind_null(stmt, idx); break;
            }
        }
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json row;
            for (int i = 0; i < cols; i++) {
                const char* col_name = sqlite3_column_name(stmt, i);
                int col_type = sqlite3_column_type(stmt, i);
                switch (col_type) {
                    case SQLITE_INTEGER: row[col_name] = sqlite3_column_int64(stmt, i); break;
                    case SQLITE_FLOAT:   row[col_name] = sqlite3_column_double(stmt, i); break;
                    case SQLITE_TEXT:    row[col_name] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)); break;
                    case SQLITE_NULL:    row[col_name] = nullptr; break;
                    default:             row[col_name] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                }
            }
            result.push_back(row);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool isOpen() const {
        return db_ != nullptr;
    }

    // 安全修复 V2：保留 escapeString 仅作为内部兼容兜底，新代码请用 execute_bind/query_bind
    std::string escapeString(const std::string& str) {
        std::string result;
        result.reserve(str.size() * 2);
        for (char c : str) {
            if (c == '\'') {
                result += "''";
            } else {
                result += c;
            }
        }
        return result;
    }

private:
    sqlite3* db_;
};

#endif
