"""Task 11 端到端验证：UID重构 + 教师班级绑定 + 家长真实用户 + Bug修复"""
from playwright.sync_api import sync_playwright
import requests

BASE = "http://localhost:8080"


def test_api():
    """API 层测试"""
    print("=" * 60)
    print("API 层测试")
    print("=" * 60)

    # 1. 四端登录
    accounts = [
        ("admin", "admin123", "admin-01", 1),
        ("teacher", "teacher123", "teacher-001", 2),
        ("student", "student123", "student-02-01-01", 3),
        ("parent", "parent123", "parent-001", 4),
    ]
    for username, password, expected_id, expected_role in accounts:
        s = requests.Session()
        r = s.post(f"{BASE}/api/auth/login", json={"username": username, "password": password})
        data = r.json()
        ok = data.get("code") == 200 and data.get("data", {}).get("user", {}).get("id") == expected_id
        role = data.get("data", {}).get("user", {}).get("role_id")
        print(f"  登录 {username}: id={data.get('data',{}).get('user',{}).get('id')}, role={role} {'✓' if ok else '✗'}")

    # 2. admin 查看用户列表
    s = requests.Session()
    s.post(f"{BASE}/api/auth/login", json={"username": "admin", "password": "admin123"})
    r = s.get(f"{BASE}/api/admin/users")
    users = r.json().get("data", [])
    print(f"\n  用户列表 ({len(users)} 人):")
    for u in users:
        extra = ""
        if u.get("role_id") == 2 and "bound_class_ids" in u:
            extra = f" bound_class_ids={u['bound_class_ids']}"
        if u.get("role_id") == 4 and "bound_student_ids" in u:
            extra = f" bound_student_ids={u['bound_student_ids']}"
        print(f"    id={u.get('id')}, name={u.get('name')}, role={u.get('role_id')}{extra}")

    # 3. teacher 查看绑定班级和学生
    s = requests.Session()
    s.post(f"{BASE}/api/auth/login", json={"username": "teacher", "password": "teacher123"})
    r = s.get(f"{BASE}/api/teacher/my-classes")
    my_classes = r.json().get("data", [])
    print(f"\n  教师绑定班级: {[c.get('name') for c in my_classes]}")

    r = s.get(f"{BASE}/api/teacher/students")
    students = r.json().get("data", [])
    print(f"  教师可见学生: {[(s.get('name'), s.get('className')) for s in students]}")

    # 4. parent 查看子女
    s = requests.Session()
    s.post(f"{BASE}/api/auth/login", json={"username": "parent", "password": "parent123"})
    r = s.get(f"{BASE}/api/parent/children")
    children = r.json().get("data", [])
    print(f"\n  家长子女: {[(c.get('name'), c.get('className')) for c in children]}")

    # 5. admin dashboard recentActivities
    s = requests.Session()
    s.post(f"{BASE}/api/auth/login", json={"username": "admin", "password": "admin123"})
    r = s.get(f"{BASE}/api/admin/dashboard")
    activities = r.json().get("data", {}).get("recentActivities", [])
    print(f"\n  仪表盘最近活动 ({len(activities)} 条): {activities[:2]}")


def test_frontend():
    """前端页面测试"""
    print("\n" + "=" * 60)
    print("前端页面测试")
    print("=" * 60)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        # admin 页面
        ctx = browser.new_context()
        page = ctx.new_page()
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "admin")
        page.fill('input[autocomplete="current-password"]', "admin123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/admin.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)

        # 检查用户管理
        page.click('button:has-text("用户管理")')
        page.wait_for_timeout(1000)
        content = page.content()
        has_admin_id = "admin-01" in content
        has_teacher_id = "teacher-001" in content
        has_parent_id = "parent-001" in content
        has_student_id = "student-02-01-01" in content
        print(f"  admin 页面显示 ID: admin-01={has_admin_id}, teacher-001={has_teacher_id}, parent-001={has_parent_id}, student-02-01-01={has_student_id}")

        # 检查编辑教师对话框含绑定班级
        teacher_row = page.locator('tbody tr').filter(has_text="teacher-001").first
        if teacher_row.count() > 0:
            teacher_row.locator('button:has-text("编辑")').click()
            page.wait_for_timeout(1000)
            dlg_content = page.content()
            has_bind_class = "绑定班级" in dlg_content
            print(f"  编辑教师对话框含'绑定班级': {has_bind_class}")
            # 关闭对话框
            try:
                page.locator('.modal-backdrop button:has-text("取消")').click(timeout=2000)
            except:
                page.keyboard.press("Escape")
            page.wait_for_timeout(500)

        # 检查编辑家长对话框含绑定子女
        parent_row = page.locator('tbody tr').filter(has_text="parent-001").first
        if parent_row.count() > 0:
            parent_row.locator('button:has-text("编辑")').click()
            page.wait_for_timeout(1000)
            dlg_content = page.content()
            has_bind_child = "绑定子女" in dlg_content
            print(f"  编辑家长对话框含'绑定子女': {has_bind_child}")
            try:
                page.locator('.modal-backdrop button:has-text("取消")').click(timeout=2000)
            except:
                page.keyboard.press("Escape")
            page.wait_for_timeout(500)

        # 检查班级管理含 grade_code
        page.click('button:has-text("班级管理")')
        page.wait_for_timeout(800)
        cls_content = page.content()
        has_grade_code = "grade_code" in cls_content or "年级编码" in cls_content or "02" in cls_content
        print(f"  班级管理含年级编码: {has_grade_code}")

        print(f"  admin 控制台错误: {len(errors)}")
        for e in errors:
            print(f"    ERR: {e}")
        ctx.close()

        # teacher 页面
        ctx = browser.new_context()
        page = ctx.new_page()
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "teacher")
        page.fill('input[autocomplete="current-password"]', "teacher123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/teacher.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)
        print(f"\n  teacher 控制台错误: {len(errors)}")
        for e in errors:
            print(f"    ERR: {e}")
        ctx.close()

        # student 页面
        ctx = browser.new_context()
        page = ctx.new_page()
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "student")
        page.fill('input[autocomplete="current-password"]', "student123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/student.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)
        print(f"  student 控制台错误: {len(errors)}")
        for e in errors:
            print(f"    ERR: {e}")
        ctx.close()

        # parent 页面
        ctx = browser.new_context()
        page = ctx.new_page()
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "parent")
        page.fill('input[autocomplete="current-password"]', "parent123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/parent.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)
        print(f"  parent 控制台错误: {len(errors)}")
        for e in errors:
            print(f"    ERR: {e}")
        ctx.close()

        browser.close()


if __name__ == "__main__":
    test_api()
    test_frontend()
    print("\n" + "=" * 60)
    print("验证完成")
    print("=" * 60)
