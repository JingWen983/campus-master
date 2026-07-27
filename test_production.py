"""
生产环境综合测试脚本
覆盖：认证流程、前端渲染、API CRUD、权限控制、数据持久化、并发、错误处理
"""
import json
import time
import urllib.request
import urllib.error
import threading
from collections import Counter
from playwright.sync_api import sync_playwright

BASE = "http://localhost:8080"

# ====== 测试结果收集 ======
results = []

def record(category, name, passed, detail=""):
    status = "PASS" if passed else "FAIL"
    results.append({"category": category, "name": name, "passed": passed, "detail": detail})
    icon = "✅" if passed else "❌"
    print(f"  {icon} [{category}] {name}" + (f" - {detail}" if detail else ""))

def api_call(method, endpoint, data=None, token=None, parse_json=True):
    """统一 API 调用"""
    url = f"{BASE}{endpoint}"
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = token
    if method == "DELETE" and data is None:
        headers["Content-Length"] = "0"

    body = json.dumps(data).encode() if data else None
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        resp = urllib.request.urlopen(req, timeout=10)
        raw = resp.read().decode()
        if not parse_json:
            return resp.status, {"raw": raw[:200]}
        try:
            return resp.status, json.loads(raw)
        except:
            return resp.status, {"raw": raw[:200]}
    except urllib.error.HTTPError as e:
        raw = e.read().decode()
        try:
            return e.code, json.loads(raw)
        except:
            return e.code, {"raw": raw[:200]}
    except Exception as ex:
        return 0, {"error": str(ex)}


# ====== 1. 服务器健康检查 ======
def test_server_health():
    print("\n" + "=" * 60)
    print("【1】服务器健康检查")
    print("=" * 60)

    # 根路径返回登录页
    status, _ = api_call("GET", "/", parse_json=False)
    record("健康检查", "根路径返回登录页", status == 200, f"HTTP {status}")

    # Vite 构建产物：登录页应引用 /assets/ 下的 hashed 资源
    # 直接请求完整 HTML（api_call 会截断 raw 到 200 字符，此处需完整内容）
    import re
    try:
        with urllib.request.urlopen(f"{BASE}/", timeout=10) as r:
            html = r.read().decode()
        has_assets_ref = "/assets/" in html
    except Exception:
        html = ""
        has_assets_ref = False
    record("健康检查", "登录页引用 Vite 构建产物", has_assets_ref, "检查 /assets/ 引用")

    # 校验 /assets/* 资源可访问（从首页提取一个 asset 路径）
    asset_ok = False
    m = re.search(r'/(assets/[^\s"\']+\.js)', html)
    if m:
        a_status, _ = api_call("GET", "/" + m.group(1), parse_json=False)
        asset_ok = a_status == 200
    record("健康检查", "Vite hashed 资源可访问", asset_ok, "/assets/*.js")

    # 页面可访问
    for page in ["/admin.html", "/teacher.html", "/student.html", "/parent.html"]:
        status, _ = api_call("GET", page, parse_json=False)
        record("健康检查", f"页面 {page}", status == 200, f"HTTP {status}")


# ====== 2. 登录认证流程测试 ======
def test_auth_flow():
    print("\n" + "=" * 60)
    print("【2】登录认证流程测试")
    print("=" * 60)

    tokens = {}

    # 正确登录
    for username, password, role in [("admin", "admin123", 1), ("teacher", "teacher123", 2), ("student", "student123", 3)]:
        status, resp = api_call("POST", "/api/auth/login", {"username": username, "password": password})
        passed = status == 200 and resp.get("code") == 200 and "token" in resp.get("data", {})
        record("认证", f"登录 {username}", passed, f"code={resp.get('code')}")
        if passed:
            tokens[role] = resp["data"]["token"]

    # 错误密码
    status, resp = api_call("POST", "/api/auth/login", {"username": "admin", "password": "wrong"})
    record("认证", "错误密码拒绝", status == 200 and resp.get("code") == 401, f"code={resp.get('code')}")

    # 空字段
    status, resp = api_call("POST", "/api/auth/login", {"username": "", "password": ""})
    record("认证", "空字段拒绝", resp.get("code") == 400, f"code={resp.get('code')}")

    # 不存在的用户
    status, resp = api_call("POST", "/api/auth/login", {"username": "nouser", "password": "x"})
    record("认证", "不存在用户拒绝", resp.get("code") == 401, f"code={resp.get('code')}")

    # 无效 JSON
    req = urllib.request.Request(f"{BASE}/api/auth/login", data=b"invalid", headers={"Content-Type": "application/json"}, method="POST")
    try:
        resp = urllib.request.urlopen(req, timeout=5)
        body = json.loads(resp.read().decode())
        record("认证", "无效JSON拒绝", body.get("code") == 400, f"code={body.get('code')}")
    except urllib.error.HTTPError as e:
        record("认证", "无效JSON拒绝", True, f"HTTP {e.code}")

    return tokens


# ====== 3. 前端页面渲染与交互测试 ======
def test_frontend_rendering(tokens):
    print("\n" + "=" * 60)
    print("【3】前端页面渲染与交互测试")
    print("=" * 60)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        # 3.1 登录页面渲染
        page = browser.new_page()
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(500)

        has_title = page.locator("text=文明能量站").count() > 0 or page.locator("text=欢迎回来").count() > 0
        record("前端", "登录页标题渲染", has_title)

        has_form = page.locator("input[type='text']").count() > 0 and page.locator("input[type='password']").count() > 0
        record("前端", "登录表单存在", has_form)

        # 3.2 实际登录测试
        page.fill("input[type='text']", "admin")
        page.fill("input[type='password']", "admin123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)

        # 检查是否跳转到管理页面
        current_url = page.url
        record("前端", "管理员登录跳转", "admin" in current_url, f"URL: {current_url}")

        # 截图
        page.screenshot(path="/tmp/prod_admin_dashboard.png", full_page=True)

        # 检查管理页面是否渲染了内容
        has_content = page.locator("text=管理员").count() > 0 or page.locator("text=仪表盘").count() > 0 or page.locator("text=工作台").count() > 0
        record("前端", "管理页面内容渲染", has_content)

        page.close()

        # 3.3 教师登录测试
        page = browser.new_page()
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.fill("input[type='text']", "teacher")
        page.fill("input[type='password']", "teacher123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)

        current_url = page.url
        record("前端", "教师登录跳转", "teacher" in current_url, f"URL: {current_url}")
        page.screenshot(path="/tmp/prod_teacher_dashboard.png", full_page=True)
        page.close()

        # 3.4 学生登录测试
        page = browser.new_page()
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.fill("input[type='text']", "student")
        page.fill("input[type='password']", "student123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)

        current_url = page.url
        record("前端", "学生登录跳转", "student" in current_url, f"URL: {current_url}")
        page.screenshot(path="/tmp/prod_student_dashboard.png", full_page=True)
        page.close()

        # 3.5 未登录访问页面应重定向
        for page_name, desc in [("admin.html", "管理员"), ("teacher.html", "教师"), ("student.html", "学生")]:
            page = browser.new_page()
            page.goto(f"{BASE}/{page_name}")
            page.wait_for_load_state("networkidle")
            page.wait_for_timeout(1500)
            # 未登录应被重定向到登录页
            redirected = page.url == f"{BASE}/" or "index" in page.url
            record("前端", f"未登录{desc}页重定向", redirected, f"URL: {page.url}")
            page.close()

        browser.close()


# ====== 4. API 功能完整性测试 ======
def test_api_crud(tokens):
    print("\n" + "=" * 60)
    print("【4】API 功能完整性测试")
    print("=" * 60)

    admin_token = tokens.get(1)
    teacher_token = tokens.get(2)
    student_token = tokens.get(3)

    # 4.1 管理员 - 用户管理 CRUD
    # 读取用户列表
    status, resp = api_call("GET", "/api/admin/users", token=admin_token)
    record("API-CRUD", "管理员读取用户列表", resp.get("code") == 200, f"用户数: {len(resp.get('data', []))}")

    # 创建用户（使用时间戳确保唯一）
    unique_suffix = str(int(time.time()))
    status, resp = api_call("POST", "/api/admin/users", {
        "username": f"testuser_{unique_suffix}",
        "password": "test123",
        "name": "测试用户",
        "role_id": 3,
        "className": "高二(1)班"
    }, token=admin_token)
    created_user_id = resp.get("data", {}).get("id") if resp.get("data") else None
    record("API-CRUD", "管理员创建用户", resp.get("code") == 200, f"code={resp.get('code')}, msg={resp.get('msg', '')}")

    # 修改用户
    if created_user_id:
        status, resp = api_call("PUT", f"/api/admin/users/{created_user_id}", {
            "name": "测试用户改",
            "className": "高二(2)班"
        }, token=admin_token)
        record("API-CRUD", "管理员修改用户", resp.get("code") == 200, f"code={resp.get('code')}")

    # 删除用户
    if created_user_id:
        status, resp = api_call("DELETE", "/api/admin/users", {"id": created_user_id}, token=admin_token)
        record("API-CRUD", "管理员删除用户", resp.get("code") == 200, f"code={resp.get('code')}")

    # 4.2 管理员 - 班级管理 CRUD
    status, resp = api_call("GET", "/api/admin/classes", token=admin_token)
    record("API-CRUD", "管理员读取班级列表", resp.get("code") == 200, f"班级数: {len(resp.get('data', []))}")

    status, resp = api_call("POST", "/api/admin/classes", {
        "name": f"测试班级_{unique_suffix}",
        "grade": "高三",
        "head_teacher": "测试老师",
        "description": "测试用"
    }, token=admin_token)
    class_created = resp.get("code") == 200
    record("API-CRUD", "管理员创建班级", class_created, f"code={resp.get('code')}, msg={resp.get('msg', '')}")

    # 4.3 管理员 - 商城管理 CRUD
    status, resp = api_call("GET", "/api/admin/mall", token=admin_token)
    record("API-CRUD", "管理员读取商城列表", resp.get("code") == 200, f"商品数: {len(resp.get('data', []))}")

    status, resp = api_call("POST", "/api/admin/mall", {
        "name": f"测试商品_{unique_suffix}",
        "description": "测试",
        "cost": 10,
        "stock": 50
    }, token=admin_token)
    item_id = resp.get("data", {}).get("id") if resp.get("data") else None
    record("API-CRUD", "管理员创建商品", resp.get("code") == 200, f"code={resp.get('code')}")

    if item_id:
        status, resp = api_call("PUT", f"/api/admin/mall/{item_id}", {
            "name": "测试商品改",
            "cost": 20
        }, token=admin_token)
        record("API-CRUD", "管理员修改商品", resp.get("code") == 200)

        status, resp = api_call("DELETE", f"/api/admin/mall/{item_id}", token=admin_token)
        record("API-CRUD", "管理员删除商品", resp.get("code") == 200)

    # 4.4 管理员 - 角色权限
    status, resp = api_call("GET", "/api/admin/roles", token=admin_token)
    record("API-CRUD", "管理员读取角色列表", resp.get("code") == 200, f"角色数: {len(resp.get('data', []))}")

    status, resp = api_call("GET", "/api/admin/permissions", token=admin_token)
    record("API-CRUD", "管理员读取权限列表", resp.get("code") == 200, f"权限数: {len(resp.get('data', []))}")

    # 4.5 管理员 - 统计与导出
    status, resp = api_call("GET", "/api/admin/dashboard", token=admin_token)
    record("API-CRUD", "管理员仪表盘", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/admin/statistics", token=admin_token)
    record("API-CRUD", "管理员统计数据", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/admin/export", token=admin_token)
    record("API-CRUD", "管理员数据导出", status == 200, f"HTTP {status}")

    # 4.6 教师 - 学生管理
    status, resp = api_call("GET", "/api/teacher/students", token=teacher_token)
    record("API-CRUD", "教师读取学生列表", resp.get("code") == 200, f"学生数: {len(resp.get('data', []))}")

    # 4.7 教师 - 积分操作
    status, resp = api_call("GET", "/api/teacher/points/records", token=teacher_token)
    record("API-CRUD", "教师读取积分记录", resp.get("code") == 200)

    # 4.8 教师 - 评价
    status, resp = api_call("GET", "/api/teacher/evaluation/dimensions", token=teacher_token)
    record("API-CRUD", "教师读取评价维度", resp.get("code") == 200, f"维度数: {len(resp.get('data', []))}")

    # 4.9 教师 - 工作台
    status, resp = api_call("GET", "/api/teacher/dashboard", token=teacher_token)
    record("API-CRUD", "教师工作台数据", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/teacher/statistics", token=teacher_token)
    record("API-CRUD", "教师统计数据", resp.get("code") == 200)

    # 4.10 学生 - 个人信息
    status, resp = api_call("GET", "/api/student/info", token=student_token)
    record("API-CRUD", "学生个人信息", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/student/points/records", token=student_token)
    record("API-CRUD", "学生积分记录", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/student/evaluation", token=student_token)
    record("API-CRUD", "学生评价信息", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/student/mall", token=student_token)
    record("API-CRUD", "学生商城列表", resp.get("code") == 200)

    status, resp = api_call("GET", "/api/student/redemptions", token=student_token)
    record("API-CRUD", "学生兑换记录", resp.get("code") == 200)


# ====== 5. 权限控制测试 ======
def test_permission_control(tokens):
    print("\n" + "=" * 60)
    print("【5】权限控制测试")
    print("=" * 60)

    admin_token = tokens.get(1)
    teacher_token = tokens.get(2)
    student_token = tokens.get(3)

    # 5.1 无 Token 访问
    for endpoint, desc in [
        ("/api/admin/users", "管理员-用户列表"),
        ("/api/teacher/students", "教师-学生列表"),
        ("/api/student/info", "学生-个人信息"),
        ("/api/admin/classes", "管理员-班级"),
        ("/api/teacher/dashboard", "教师-工作台"),
    ]:
        status, resp = api_call("GET", endpoint)
        passed = status == 401 or resp.get("code") == 401
        record("权限", f"无Token访问 {desc}", passed, f"HTTP {status}")

    # 5.2 跨角色访问
    # 学生 -> 教师API
    for endpoint, desc in [
        ("/api/teacher/students", "学生→教师学生列表"),
        ("/api/teacher/dashboard", "学生→教师工作台"),
        ("/api/teacher/points/records", "学生→教师积分记录"),
    ]:
        status, resp = api_call("GET", endpoint, token=student_token)
        passed = status == 403 or resp.get("code") == 403
        record("权限", desc, passed, f"HTTP {status}")

    # 学生 -> 管理员API
    for endpoint, desc in [
        ("/api/admin/users", "学生→管理员用户"),
        ("/api/admin/classes", "学生→管理员班级"),
        ("/api/admin/mall", "学生→管理员商城"),
        ("/api/admin/dashboard", "学生→管理员仪表盘"),
    ]:
        status, resp = api_call("GET", endpoint, token=student_token)
        passed = status == 403 or resp.get("code") == 403
        record("权限", desc, passed, f"HTTP {status}")

    # 教师 -> 管理员API
    for endpoint, desc in [
        ("/api/admin/users", "教师→管理员用户"),
        ("/api/admin/classes", "教师→管理员班级"),
        ("/api/admin/roles", "教师→管理员角色"),
    ]:
        status, resp = api_call("GET", endpoint, token=teacher_token)
        passed = status == 403 or resp.get("code") == 403
        record("权限", desc, passed, f"HTTP {status}")

    # 5.3 无效 Token
    status, resp = api_call("GET", "/api/admin/users", token="invalid_token_123")
    record("权限", "无效Token拒绝", status == 401 or resp.get("code") == 401, f"HTTP {status}")

    # 5.4 伪造 Token
    status, resp = api_call("GET", "/api/admin/users", token="token_99999_1234567890")
    record("权限", "伪造用户Token拒绝", status in [401, 403], f"HTTP {status}")


# ====== 6. 数据持久化测试 ======
def test_data_persistence(tokens):
    print("\n" + "=" * 60)
    print("【6】数据持久化测试")
    print("=" * 60)

    admin_token = tokens.get(1)

    # 创建测试数据
    unique_suffix = str(int(time.time()))
    status, resp = api_call("POST", "/api/admin/mall", {
        "name": f"持久化测试商品_{unique_suffix}",
        "description": "用于测试数据持久化",
        "cost": 999,
        "stock": 1
    }, token=admin_token)
    record("持久化", "创建测试商品", resp.get("code") == 200, f"code={resp.get('code')}")

    # 立即查询验证
    status, resp = api_call("GET", "/api/admin/mall", token=admin_token)
    items = resp.get("data", [])
    found = any(item.get("name", "").startswith("持久化测试商品_") for item in items)
    record("持久化", "立即查询验证", found, f"在{len(items)}个商品中找到")

    # 验证用户数据持久化
    status, resp = api_call("GET", "/api/admin/users", token=admin_token)
    users = resp.get("data", [])
    has_admin = any(u.get("username") == "admin" for u in users)
    record("持久化", "用户数据持久化", has_admin, f"共{len(users)}个用户")


# ====== 7. 并发测试 ======
def test_concurrency(tokens):
    print("\n" + "=" * 60)
    print("【7】并发测试")
    print("=" * 60)

    admin_token = tokens.get(1)
    teacher_token = tokens.get(2)

    # 7.1 并发读取
    results_list = []
    errors = []

    def concurrent_read():
        try:
            status, resp = api_call("GET", "/api/teacher/students", token=teacher_token)
            results_list.append(status == 200 and resp.get("code") == 200)
        except Exception as e:
            errors.append(str(e))

    threads = []
    for _ in range(20):
        t = threading.Thread(target=concurrent_read)
        threads.append(t)

    start = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=15)
    elapsed = time.time() - start

    success_count = sum(results_list)
    record("并发", f"20并发读取学生列表", success_count == 20, f"成功{success_count}/20, {elapsed:.2f}s, 错误{len(errors)}")

    # 7.2 并发登录
    login_results = []

    def concurrent_login():
        try:
            status, resp = api_call("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
            login_results.append(resp.get("code") == 200)
        except:
            login_results.append(False)

    threads = []
    for _ in range(10):
        t = threading.Thread(target=concurrent_login)
        threads.append(t)

    start = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=15)
    elapsed = time.time() - start

    success_count = sum(login_results)
    record("并发", f"10并发登录", success_count == 10, f"成功{success_count}/10, {elapsed:.2f}s")


# ====== 8. 错误处理测试 ======
def test_error_handling(tokens):
    print("\n" + "=" * 60)
    print("【8】错误处理测试")
    print("=" * 60)

    admin_token = tokens.get(1)
    teacher_token = tokens.get(2)

    # 不存在的路由
    status, resp = api_call("GET", "/api/nonexistent")
    record("错误处理", "不存在路由处理", status == 404, f"HTTP {status}")

    # 不存在的用户ID
    status, resp = api_call("PUT", "/api/admin/users/99999", {"name": "x"}, token=admin_token)
    record("错误处理", "不存在用户更新", resp.get("code") in [404, 400], f"code={resp.get('code')}")

    # 无效的JSON格式
    req = urllib.request.Request(f"{BASE}/api/teacher/points", data=b"{{invalid", headers={"Content-Type": "application/json", "Authorization": teacher_token}, method="POST")
    try:
        resp = urllib.request.urlopen(req, timeout=5)
        body = json.loads(resp.read().decode())
        record("错误处理", "无效JSON积分操作", body.get("code") == 400, f"code={body.get('code')}")
    except urllib.error.HTTPError as e:
        record("错误处理", "无效JSON积分操作", True, f"HTTP {e.code}")

    # 缺少必填字段
    status, resp = api_call("POST", "/api/teacher/points", {"studentId": 3}, token=teacher_token)
    record("错误处理", "缺少必填字段", resp.get("code") == 400, f"code={resp.get('code')}")

    # 积分不足扣减
    status, resp = api_call("POST", "/api/teacher/points", {
        "studentId": 3,
        "type": "deduct",
        "points": 999999,
        "reason": "测试扣减"
    }, token=teacher_token)
    record("错误处理", "积分不足扣减拒绝", resp.get("code") == 400, f"code={resp.get('code')}")


# ====== 9. 前端交互功能测试 ======
def test_frontend_interaction(tokens):
    print("\n" + "=" * 60)
    print("【9】前端交互功能测试")
    print("=" * 60)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        # 管理员登录并测试导航
        page = browser.new_page()
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.fill("input[type='text']", "admin")
        page.fill("input[type='password']", "admin123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)

        # 检查导航按钮
        nav_buttons = page.locator("nav button, nav a, .sidebar button, .sidebar a").count()
        record("交互", "管理员导航按钮存在", nav_buttons > 0, f"按钮数: {nav_buttons}")

        # 尝试切换标签页
        try:
            tabs = page.locator("button:has-text('用户'), button:has-text('班级'), button:has-text('商城'), button:has-text('角色')").all()
            if tabs:
                tabs[0].click()
                page.wait_for_timeout(1000)
                record("交互", "标签页切换", True)
            else:
                record("交互", "标签页切换", False, "未找到标签按钮")
        except Exception as e:
            record("交互", "标签页切换", False, str(e))

        page.screenshot(path="/tmp/prod_admin_interaction.png", full_page=True)
        page.close()

        # 教师登录并测试
        page = browser.new_page()
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.fill("input[type='text']", "teacher")
        page.fill("input[type='password']", "teacher123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)

        # 检查教师页面功能
        has_student_mgmt = page.locator("text=学生").count() > 0
        record("交互", "教师页面学生管理", has_student_mgmt)

        page.screenshot(path="/tmp/prod_teacher_interaction.png", full_page=True)
        page.close()

        # 学生登录并测试
        page = browser.new_page()
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.fill("input[type='text']", "student")
        page.fill("input[type='password']", "student123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)

        # 检查学生页面
        has_points = page.locator("text=积分").count() > 0
        record("交互", "学生页面积分显示", has_points)

        page.screenshot(path="/tmp/prod_student_interaction.png", full_page=True)
        page.close()

        browser.close()


# ====== 主函数 ======
def main():
    print("=" * 60)
    print("🚀 生产环境综合测试开始")
    print(f"   目标: {BASE}")
    print(f"   时间: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 60)

    # 1. 服务器健康检查
    test_server_health()

    # 2. 登录认证流程
    tokens = test_auth_flow()
    if not tokens:
        print("\n❌ 登录失败，无法继续测试！")
        return

    # 3. 前端页面渲染
    test_frontend_rendering(tokens)

    # 4. API 功能完整性
    test_api_crud(tokens)

    # 5. 权限控制
    test_permission_control(tokens)

    # 6. 数据持久化
    test_data_persistence(tokens)

    # 7. 并发测试
    test_concurrency(tokens)

    # 8. 错误处理
    test_error_handling(tokens)

    # 9. 前端交互
    test_frontend_interaction(tokens)

    # ====== 汇总报告 ======
    print("\n" + "=" * 60)
    print("📊 生产环境测试报告")
    print("=" * 60)

    total = len(results)
    passed = sum(1 for r in results if r["passed"])
    failed = total - passed

    # 按分类统计
    categories = {}
    for r in results:
        cat = r["category"]
        if cat not in categories:
            categories[cat] = {"pass": 0, "fail": 0}
        if r["passed"]:
            categories[cat]["pass"] += 1
        else:
            categories[cat]["fail"] += 1

    print(f"\n总计: {total} 项 | 通过: {passed} | 失败: {failed} | 通过率: {passed/total*100:.1f}%")
    print()

    print(f"{'分类':<15} {'通过':>6} {'失败':>6} {'通过率':>8}")
    print("-" * 40)
    for cat, counts in categories.items():
        total_cat = counts["pass"] + counts["fail"]
        rate = counts["pass"] / total_cat * 100 if total_cat > 0 else 0
        icon = "✅" if counts["fail"] == 0 else "⚠️"
        print(f"{icon} {cat:<13} {counts['pass']:>6} {counts['fail']:>6} {rate:>7.1f}%")

    if failed > 0:
        print(f"\n❌ 失败项详情:")
        for r in results:
            if not r["passed"]:
                print(f"  - [{r['category']}] {r['name']}: {r['detail']}")

    print("\n" + "=" * 60)
    if failed == 0:
        print("✅ 所有测试通过！系统可投入生产环境。")
    else:
        print(f"⚠️ 有 {failed} 项测试失败，请修复后再上线。")
    print("=" * 60)


if __name__ == "__main__":
    main()
