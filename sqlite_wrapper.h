#ifndef SQLITE_WRAPPER_H
#define SQLITE_WRAPPER_H

#include <sqlite3.h>

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <cstring>
#include <cstdio>
#include "json.hpp"

using json = nlohmann::json;

// ===========================================================================
// 批次 1 / 1-1 连接模型：**每线程一条连接**
// ---------------------------------------------------------------------------
// 背景（实测依据，见 docs/audit/BATCH1_DESIGN_DECISIONS.md 决策①）：
//   * httplib.h:3443-3448 的 listen_internal() 只创建**一次** TaskQueue；
//   * httplib.h:371-379 / :408-426 的 worker 线程长驻，fn() 在同一线程反复执行；
//   * httplib.h:3481-3483 所有 process_and_close_socket 只 enqueue 到该池，
//     Server 侧不再起线程 → 请求 100% 落在固定的 g_config.thread_count 个线程上。
//   因此 main.cpp 的全局 SqliteDb db 曾被这 8 个线程共享同一 sqlite3*。
//
// 为什么「WAL + busy_timeout 已经存在」仍然错误：
//   * PRAGMA journal_mode=WAL 是**数据库文件**属性（读不阻塞写），不提供任何
//     应用层临界区；
//   * sqlite3_busy_timeout 只把 SQLITE_BUSY 变成阻塞重试，不把多条语句变成原子；
//   * sqlite3_open（不传 SQLITE_OPEN_FULLMUTEX）拿到的连接带一把递归互斥
//     （sqlite3.c:14050 SQLITE_THREADSAFE=1 → :22806 bFullMutex=1 →
//      :181367-181371/:181405-181418），它只保证**单次 API 调用**不撕裂，
//     不提供跨语句/跨调用临界区，也不解决
//       - BEGIN/COMMIT 的连接级事务归属（routes_admin.cpp 的导入端点）；
//       - sqlite3_changes64() 的连接级归属（本次新增的受影响行数通道）。
//   两者都是**连接级**状态：只有「连接按线程隔离」才能根治。
//
// 实现形态：SqliteDb 仍是一个**门面**（约 600+ 处 db.xxx(...) 调用点源码不变），
// 内部把 sqlite3* 换成「当前线程的那一条」。连接按线程惰性创建，**随线程退出销毁**
// （由线程局部的 ThreadConn 析构关闭，见下方 :336-349），并**禁止跨线程传递**。
// ===========================================================================
class SqliteDb {
public:
    SqliteDb() = default;
    ~SqliteDb() = default;
    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;

    // 记录期望的数据库路径；真正的连接在首次使用时按线程惰性建立。
    // 注意：db_path_ 只在启动期（listen() 之前、单线程）写一次，之后为只读；
    // 因此无需任何锁 —— 这与 g_config / Logger 的既有约定一致。
    bool open(const std::string& path) {
        db_path_ = path;
        // ====== 承接复核发现的第 2 处实质缺陷：同线程换库会**静默复用旧连接** ======
        // 句柄是「每线程一条」的 thread_local，同线程内的所有 SqliteDb 实例共享它。
        // 若此处对「已有连接」无条件返回 true，第二个实例（或其 open 的其他路径）
        // 就会在「以为连了新库」的状态下继续操作旧库。实测反例（修复前）：
        //   SqliteDb other; other.open("b1obj/no_such_dir/sub/never.db") → 返回 true，
        //   other.isOpen() → true，other.execute("SELECT 1") → true（实际打在旧库上）。
        // 现改为：同路径重复 open 幂等返回 true；**换库请求响亮失败**（先 close() 才可换库）。
        ThreadConn& c = tls_conn();
        if (c.handle) {
            if (std::strcmp(c.path, path.c_str()) == 0) return true;   // 幂等：重复 open 同一个库
            std::cerr << "拒绝在同一线程内切换数据库连接：已连接 " << c.path
                      << "，请求 " << path
                      << "（连接按线程唯一；如需换库请先调用 close()）" << std::endl;
            return false;
        }
        // 主线程立即建连：启动期（建表/迁移/种子/加载）必须在 listen() 之前完成，
        // 任何失败都要能被 main.cpp 立刻观察到并 fail-fast。
        return open_current_thread();
    }

    // 门面级别的 close()：只关闭**调用线程**的连接（连接永远属于建它的线程）。
    void close() {
        ThreadConn& c = tls_conn();
        if (c.handle) {
            sqlite3_close(c.handle);
            c.handle = nullptr;
            c.path[0] = '\0';
            live_connections().fetch_sub(1);
        }
    }

    bool isOpen() const {
        return tls_conn().handle != nullptr;
    }

    bool execute(const std::string& sql) {
        sqlite3* h = conn();
        if (!h) return false;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(h, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            if (errMsg) {
                std::cerr << "SQL错误: " << errMsg << std::endl;
                sqlite3_free(errMsg);
            }
            return false;
        }
        return true;
    }

    json query(const std::string& sql) {
        json result = json::array();
        sqlite3* h = conn();
        if (!h) return result;

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(h, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "查询失败: " << sqlite3_errmsg(h) << std::endl;
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
        sqlite3* h = conn();
        if (!h) return -1;
        if (execute(sql)) {
            return (int)sqlite3_last_insert_rowid(h);
        }
        return -1;
    }

    int update(const std::string& sql) {
        sqlite3* h = conn();
        if (!h) return -1;
        if (execute(sql)) {
            return sqlite3_changes(h);
        }
        return -1;
    }

    // 批次 1 / 伴-2'：取当前线程连接上**最后一次成功 INSERT** 生成的 rowid。
    // 语义与 sqlite3_last_insert_rowid 一致（连接级值）；因连接按线程隔离，
    // 紧随 INSERT 之后在本线程内取值不可能被其他线程覆盖。
    long long last_insert_rowid() {
        sqlite3* h = conn();
        if (!h) return 0;
        return (long long)sqlite3_last_insert_rowid(h);
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
    //
    // 语义**保持不变**（只表达「语句是否成功执行完」）：
    //   * true  = sqlite3_step 返回 SQLITE_DONE（含「语法合法但影响 0 行」）
    //   * false = prepare 失败或 step 报错
    // 需要「受影响行数」的调用方请用 execute_bind_affected()。
    bool execute_bind(const std::string& sql, const std::vector<Bind>& params = {}) {
        int affected = 0;
        return execute_bind_affected(sql, params, affected) == 0;
    }

    // ====== 批次 1 / 1-4 + 交接缺陷②：受影响行数通道 ======
    //
    // 返回：0 = 语句成功执行完（SQLITE_DONE）；非 0 = SQLite 错误码（prepare 失败时返回 SQLITE_ERROR）
    // 成功时 *affected_rows = 本次语句实际影响的行数（sqlite3_changes64）。
    //
    // 为什么必须在 step 之后**立刻**取行数：sqlite3_changes64() 是**连接级**值，
    // 任何在同一连接上执行过写语句的线程都会覆盖它。因为本实现是「每线程一条连接」，
    // 「step 后立刻取」在同一线程内不可能被其他线程打断，该值才可信。
    // 这正是 1-1 连接模型必须先落地的原因。
    int execute_bind_affected(const std::string& sql, const std::vector<Bind>& params, int& affected_rows) {
        affected_rows = 0;
        sqlite3* h = conn();
        if (!h) return SQLITE_ERROR;

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(h, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare 错误: " << sqlite3_errmsg(h) << std::endl;
            return rc;
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
        if (rc == SQLITE_DONE) {
            affected_rows = (int)sqlite3_changes64(h);
        }
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE) ? 0 : rc;
    }

    // 参数化 query：返回 json 数组
    json query_bind(const std::string& sql, const std::vector<Bind>& params = {}) {
        json result = json::array();
        sqlite3* h = conn();
        if (!h) return result;

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(h, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "查询失败: " << sqlite3_errmsg(h) << std::endl;
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

    // ====== 批次 1 / 伴-1（B7）：RAII 事务守卫 ======
    //
    // 为什么必须有：事务是**连接级**的。旧代码在 routes_admin.cpp 的导入端点用
    // 裸 db.execute("BEGIN TRANSACTION") / ("COMMIT") / ("ROLLBACK")，而
    // BEGIN 与 COMMIT 之间隔着整个导入循环 —— 期间其他线程在同一连接上的写入
    // 会被卷入该事务，一旦 ROLLBACK 就连它们「已返回 200」的写入一起丢弃；
    // 且 BEGIN/COMMIT 的返回码从未被检查、json::parse_error 分支漏回滚。
    //
    // 现在：连接按线程隔离 + 本守卫保证「析构时若未 commit 必回滚」。
    // 用 BEGIN IMMEDIATE 而非 BEGIN：立即取写锁，避免「先读后升级为写」导致的
    // SQLITE_BUSY 死锁路径。
    class Transaction {
    public:
        explicit Transaction(SqliteDb& db) : db_(db) {
            active_ = db_.execute("BEGIN IMMEDIATE");
            if (!active_) {
                std::cerr << "BEGIN IMMEDIATE 失败，事务未开启" << std::endl;
            }
        }
        ~Transaction() {
            if (active_ && !committed_) {
                if (!db_.execute("ROLLBACK")) {
                    std::cerr << "ROLLBACK 失败，数据库可能处于不一致状态" << std::endl;
                }
            }
        }
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;

        bool active() const { return active_; }

        bool commit() {
            if (!active_ || committed_) return false;
            if (!db_.execute("COMMIT")) {
                std::cerr << "COMMIT 失败，将回滚" << std::endl;
                db_.execute("ROLLBACK");
                active_ = false;
                return false;
            }
            committed_ = true;
            return true;
        }

    private:
        SqliteDb& db_;
        bool active_ = false;
        bool committed_ = false;
    };

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

    // 诊断用：当前进程内已建立且未关闭的连接数（连接数的可执行证据使用）
    static int live_connection_count() {
        return live_connections().load();
    }

private:
    // ====== 每线程一条连接：**线程局部**句柄 ======
    // 为什么用 thread_local 而不是「全局表 + 互斥锁」（本任务对既有工作区改动的
    // 唯一实质修正，理由逐条）：
    //   * 决策② 的 R1 规定全进程只允许**一把**应用级互斥锁（内存状态锁），
    //     第二把 std::mutex 会破坏「单锁 → 无锁顺序问题」这一前提；
    //     原实现里的 registry_mutex() 正是一把新增的 std::mutex（违反 R1）；
    //   * 决策② 的 R7 明确「连接（线程局部）不是共享资源，不需要状态锁保护」；
    //   * 线程局部句柄让「一个线程只可能访问自己的连接」成为**类型/存储期**事实，
    //     既不需要锁，也不可能跨线程传递（①-C5）。
    // 生命周期：worker 线程由 ThreadPool::shutdown() 在 listen() 返回后统一 join
    // （httplib.h:389-402），线程退出时由 ThreadConn 的析构关闭自己的连接；
    // 主线程的连接在 main 返回时关闭；进程退出由 OS 兜底回收。
    struct ThreadConn {
        sqlite3* handle = nullptr;
        // 该连接**实际**打开的库路径（用于拒绝同线程静默换库）。
        // 必须是**平凡类型**（char 数组），**不得**改用 std::string —— 实测证据
        // （隔离实验 b1obj/tls_probe2.cpp，与产品相同的 -std=c++17 -O2 -static 配置）：
        //   * mode 1（thread_local 成员含 std::string）→ 8 线程 × 40 轮，
        //     进程以 0xC0000374 STATUS_HEAP_CORRUPTION 崩溃，两次运行均在首轮即崩；
        //   * mode 2（thread_local 成员为 char[512] + 用户析构）→ dtor 320/320、exit 0；
        //   * mode 4（thread_local 成员含 std::string、无用户析构）→ 同样崩溃；
        //   * mode 0/3（无线程局部对象 / 平凡成员）→ exit 0。
        // 结论：本 MinGW-w64 14.2.0 静态链接产物中，需要**动态初始化**的 thread_local
        // 成员会破坏堆；其可观测后果正是「线程退出未关闭连接（连接计数不回落）」
        // 与关停期崩溃 —— 因此这里坚持平凡成员。
        char path[4096] = {0};
        ~ThreadConn() {
            if (handle) {
                sqlite3_close(handle);
                handle = nullptr;
                live_connections().fetch_sub(1);
            }
        }
    };
    static ThreadConn& tls_conn() {
        static thread_local ThreadConn c;
        return c;
    }
    // 已建立连接数：**原子计数**（不是锁，仅供诊断与并发证据使用）
    static std::atomic<int>& live_connections() {
        static std::atomic<int> n{0};
        return n;
    }

    // 为**当前线程**建连（若尚未建）。线程局部，无需加锁。
    bool open_current_thread() {
        ThreadConn& c = tls_conn();
        if (c.handle) return true;

        sqlite3* h = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &h);
        if (rc != SQLITE_OK) {
            std::cerr << "无法打开数据库: " << (h ? sqlite3_errmsg(h) : "unknown") << std::endl;
            if (h) sqlite3_close(h);
            c.handle = nullptr;
            c.path[0] = '\0';
            return false;
        }
        c.handle = h;
        std::snprintf(c.path, sizeof(c.path), "%s", db_path_.c_str());
        live_connections().fetch_add(1);
        // 以下两项都是**连接级**设置，每条新连接都必须重新设置（①-C6）：
        sqlite3_busy_timeout(h, 5000);           // 写锁冲突时最多阻塞 5s 重试
        char* err = nullptr;
        sqlite3_exec(h, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
        return true;
    }

    // 取当前线程的连接；首次使用时惰性建立。
    // 返回 nullptr 表示该线程无法建连（调用方必须 fail-closed，不得冒充「无数据」）。
    sqlite3* conn() {
        ThreadConn& c = tls_conn();
        if (c.handle) return c.handle;
        if (!open_current_thread()) {
            std::cerr << "线程 " << std::this_thread::get_id() << " 建立数据库连接失败" << std::endl;
            return nullptr;
        }
        return c.handle;
    }

    // 期望的库路径（由 main 在启动期设置一次，之后只读）
    std::string db_path_;
};

#endif
