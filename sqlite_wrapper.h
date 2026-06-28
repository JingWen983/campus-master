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

    bool isOpen() const {
        return db_ != nullptr;
    }

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
