"""
权限漏洞测试脚本
测试未登录用户是否能直接访问各角色页面和调用 API
"""
from playwright.sync_api import sync_playwright

BASE = "http://localhost:8080"

def test_unauthenticated_access():
    """测试未登录用户是否能访问各角色页面"""
    vulnerabilities = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context()

        # 测试各页面是否可以无登录访问
        pages_to_test = [
            ("admin.html", "管理员页面"),
            ("teacher.html", "教师页面"),
            ("student.html", "学生页面"),
        ]

        for page_name, desc in pages_to_test:
            page = context.new_page()
            page.goto(f"{BASE}/{page_name}")
            page.wait_for_load_state("networkidle")
            page.wait_for_timeout(1000)

            # 截图
            page.screenshot(path=f"/tmp/test_{page_name}_no_login.png", full_page=True)

            # 获取页面 URL 和内容
            content = page.content()
            url = page.url

            # 检查是否有登录相关的 DOM 元素（如果没有，说明可以直接访问）
            has_login_form = page.locator("text=登录").count() > 0 or page.locator("text=用户名").count() > 0

            # 检查页面是否显示实际内容（而非重定向到登录页）
            has_dashboard = page.locator("text=管理员").count() > 0 or page.locator("text=教师").count() > 0 or page.locator("text=学生").count() > 0

            if not has_login_form and has_dashboard:
                vulnerabilities.append(f"⚠️ {desc} ({page_name}) - 未登录可直接访问！")
            else:
                print(f"✅ {desc} ({page_name}) - 已受保护或有登录表单")

            page.close()

        browser.close()

    return vulnerabilities


def test_api_without_token():
    """测试未带 Token 时是否可以调用需要权限的 API"""
    import urllib.request
    import json

    vulnerabilities = []

    apis_to_test = [
        ("GET", "/api/admin/users", "管理员-用户列表"),
        ("GET", "/api/teacher/students", "教师-学生列表"),
        ("GET", "/api/student/info", "学生-个人信息"),
        ("GET", "/api/admin/classes", "管理员-班级列表"),
        ("GET", "/api/teacher/dashboard", "教师-工作台"),
        ("GET", "/api/admin/roles", "管理员-角色列表"),
    ]

    for method, endpoint, desc in apis_to_test:
        try:
            req = urllib.request.Request(f"{BASE}{endpoint}", method=method)
            req.add_header("Content-Type", "application/json")
            resp = urllib.request.urlopen(req, timeout=5)
            body = json.loads(resp.read().decode())

            # 如果返回 200 且有数据，说明未授权也能访问
            if body.get("code") == 200:
                vulnerabilities.append(f"⚠️ API {method} {endpoint} ({desc}) - 未带 Token 可返回 200 数据！")
            else:
                print(f"✅ API {method} {endpoint} ({desc}) - 正确拒绝未授权请求")
        except urllib.error.HTTPError as e:
            if e.code == 401:
                print(f"✅ API {method} {endpoint} ({desc}) - 正确返回 401 未授权")
            elif e.code == 404:
                print(f"⚠️ API {method} {endpoint} ({desc}) - 路由不存在 (404)")
            else:
                print(f"? API {method} {endpoint} ({desc}) - HTTP {e.code}")
        except Exception as ex:
            print(f"? API {method} {endpoint} ({desc}) - 错误: {ex}")

    return vulnerabilities


def test_cross_role_access():
    """测试低权限用户是否能访问高权限 API"""
    import urllib.request
    import json

    vulnerabilities = []

    # 1. 学生 token 访问教师 API
    try:
        login_req = urllib.request.Request(
            f"{BASE}/api/auth/login",
            data=json.dumps({"username": "student", "password": "student123"}).encode(),
            headers={"Content-Type": "application/json"},
            method="POST"
        )
        login_resp = json.loads(urllib.request.urlopen(login_req).read().decode())
        student_token = login_resp["data"]["token"]

        # 学生尝试访问教师 API
        for endpoint, desc in [("/api/teacher/students", "学生→教师学生列表"), ("/api/teacher/dashboard", "学生→教师工作台")]:
            try:
                req = urllib.request.Request(f"{BASE}{endpoint}", method="GET")
                req.add_header("Authorization", student_token)
                resp = urllib.request.urlopen(req, timeout=5)
                body = json.loads(resp.read().decode())
                if body.get("code") == 200:
                    vulnerabilities.append(f"⚠️ 越权访问 {endpoint} ({desc}) - 学生 Token 可访问！")
                else:
                    print(f"✅ {endpoint} ({desc}) - 正确拒绝")
            except urllib.error.HTTPError as e:
                print(f"✅ {endpoint} ({desc}) - HTTP {e.code} 正确拒绝")
    except Exception as e:
        print(f"登录失败: {e}")

    # 2. 教师 token 访问管理员 API
    try:
        login_req = urllib.request.Request(
            f"{BASE}/api/auth/login",
            data=json.dumps({"username": "teacher", "password": "teacher123"}).encode(),
            headers={"Content-Type": "application/json"},
            method="POST"
        )
        login_resp = json.loads(urllib.request.urlopen(login_req).read().decode())
        teacher_token = login_resp["data"]["token"]

        for endpoint, desc in [("/api/admin/users", "教师→管理员用户列表"), ("/api/admin/classes", "教师→管理员班级")]:
            try:
                req = urllib.request.Request(f"{BASE}{endpoint}", method="GET")
                req.add_header("Authorization", teacher_token)
                resp = urllib.request.urlopen(req, timeout=5)
                body = json.loads(resp.read().decode())
                if body.get("code") == 200:
                    vulnerabilities.append(f"⚠️ 越权访问 {endpoint} ({desc}) - 教师 Token 可访问！")
                else:
                    print(f"✅ {endpoint} ({desc}) - 正确拒绝")
            except urllib.error.HTTPError as e:
                print(f"✅ {endpoint} ({desc}) - HTTP {e.code} 正确拒绝")
    except Exception as e:
        print(f"登录失败: {e}")

    return vulnerabilities


if __name__ == "__main__":
    print("=" * 60)
    print("🔍 权限漏洞测试开始")
    print("=" * 60)

    print("\n【1】测试未登录用户访问各角色页面")
    print("-" * 40)
    vulns1 = test_unauthenticated_access()

    print("\n【2】测试未带 Token 访问 API")
    print("-" * 40)
    vulns2 = test_api_without_token()

    print("\n【3】测试跨角色越权访问")
    print("-" * 40)
    vulns3 = test_cross_role_access()

    print("\n" + "=" * 60)
    print("📊 测试结果汇总")
    print("=" * 60)
    all_vulns = vulns1 + vulns2 + vulns3
    if all_vulns:
        print(f"\n🚨 发现 {len(all_vulns)} 个权限漏洞:\n")
        for v in all_vulns:
            print(f"  {v}")
    else:
        print("\n✅ 未发现权限漏洞，系统安全！")

    print("\n提示: 请查看 /tmp/test_*.png 截图确认页面访问情况")
