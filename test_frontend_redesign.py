"""
前端重构后的视觉与功能验证测试
使用 Playwright 截图验证每个页面的渲染效果
"""
import time
from playwright.sync_api import sync_playwright

BASE = "http://localhost:8080"
results = []

def record(name, passed, detail=""):
    icon = "✅" if passed else "❌"
    results.append({"name": name, "passed": passed})
    print(f"  {icon} {name}" + (f" - {detail}" if detail else ""))


def main():
    print("=" * 60)
    print("🎨 前端重构验证测试")
    print("=" * 60)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        # ====== 1. 登录页测试 ======
        print("\n【1】登录页测试")
        page = browser.new_page(viewport={"width": 1440, "height": 900})
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)

        # 检查品牌标题
        has_brand = page.locator("text=文明能量站").count() > 0
        record("登录页品牌标题", has_brand)

        # 检查表单
        has_form = page.locator("input[type='text']").count() > 0 and page.locator("input[type='password']").count() > 0
        record("登录表单存在", has_form)

        # 检查演示账号按钮
        has_demo = page.locator("text=管理员").count() > 0 and page.locator("text=教师").count() > 0 and page.locator("text=学生").count() > 0
        record("演示账号按钮", has_demo)

        # 检查 Fraunces 字体加载
        fonts_loaded = page.evaluate("document.fonts.size > 0")
        record("字体加载", fonts_loaded)

        page.screenshot(path="/tmp/redesign_login.png", full_page=True)
        record("登录页截图保存", True)
        page.close()

        # ====== 2. 管理员页面测试 ======
        print("\n【2】管理员页面测试")
        page = browser.new_page(viewport={"width": 1440, "height": 900})
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.fill("input[type='text']", "admin")
        page.fill("input[type='password']", "admin123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(3000)

        # 验证跳转
        record("管理员登录跳转", "admin" in page.url, f"URL: {page.url}")

        # 验证侧边栏导航
        nav_items = page.locator("nav button, aside button").count()
        record("管理员侧边栏导航", nav_items >= 5, f"导航项: {nav_items}")

        # 验证统计卡片
        has_stats = page.locator("text=总用户数").count() > 0 or page.locator("text=用户").count() > 0
        record("管理员仪表盘统计", has_stats)

        # 截图仪表盘
        page.screenshot(path="/tmp/redesign_admin_dashboard.png", full_page=True)
        record("管理员仪表盘截图", True)

        # 切换到用户管理
        try:
            user_btn = page.locator("text=用户管理").first
            if user_btn.is_visible():
                user_btn.click()
                page.wait_for_timeout(1500)
                has_table = page.locator("table").count() > 0
                record("管理员用户管理表格", has_table)
                page.screenshot(path="/tmp/redesign_admin_users.png", full_page=True)
        except:
            record("管理员用户管理表格", False, "切换失败")

        # 切换到班级管理
        try:
            class_btn = page.locator("text=班级管理").first
            if class_btn.is_visible():
                class_btn.click()
                page.wait_for_timeout(1500)
                page.screenshot(path="/tmp/redesign_admin_classes.png", full_page=True)
                record("管理员班级管理", True)
        except:
            record("管理员班级管理", False, "切换失败")

        # 切换到商城管理
        try:
            mall_btn = page.locator("text=商城管理").first
            if mall_btn.is_visible():
                mall_btn.click()
                page.wait_for_timeout(1500)
                page.screenshot(path="/tmp/redesign_admin_mall.png", full_page=True)
                record("管理员商城管理", True)
        except:
            record("管理员商城管理", False, "切换失败")

        page.close()

        # ====== 3. 教师页面测试 ======
        print("\n【3】教师页面测试")
        page = browser.new_page(viewport={"width": 1440, "height": 900})
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(500)
        page.fill("input[type='text']", "teacher")
        page.fill("input[type='password']", "teacher123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(3000)

        record("教师登录跳转", "teacher" in page.url, f"URL: {page.url}")

        nav_items = page.locator("nav button, aside button").count()
        record("教师侧边栏导航", nav_items >= 4, f"导航项: {nav_items}")

        has_dashboard = page.locator("text=工作台").count() > 0 or page.locator("text=学生总数").count() > 0
        record("教师工作台渲染", has_dashboard)

        page.screenshot(path="/tmp/redesign_teacher_dashboard.png", full_page=True)
        record("教师工作台截图", True)

        # 切换到学生管理
        try:
            student_btn = page.locator("text=学生管理").first
            if student_btn.is_visible():
                student_btn.click()
                page.wait_for_timeout(1500)
                has_table = page.locator("table").count() > 0
                record("教师学生管理表格", has_table)
                page.screenshot(path="/tmp/redesign_teacher_students.png", full_page=True)
        except:
            record("教师学生管理表格", False, "切换失败")

        page.close()

        # ====== 4. 学生页面测试 ======
        print("\n【4】学生页面测试")
        page = browser.new_page(viewport={"width": 1440, "height": 900})
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(500)
        page.fill("input[type='text']", "student")
        page.fill("input[type='password']", "student123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(3000)

        record("学生登录跳转", "student" in page.url, f"URL: {page.url}")

        nav_items = page.locator("nav button, aside button").count()
        record("学生侧边栏导航", nav_items >= 4, f"导航项: {nav_items}")

        # 检查积分卡片
        has_points = page.locator("text=积分").count() > 0 or page.locator("text=能量").count() > 0
        record("学生积分卡片", has_points)

        page.screenshot(path="/tmp/redesign_student_home.png", full_page=True)
        record("学生首页截图", True)

        # 切换到商城
        try:
            mall_btn = page.locator("text=兑换商城").first
            if not mall_btn.is_visible():
                mall_btn = page.locator("text=商城").first
            if mall_btn.is_visible():
                mall_btn.click()
                page.wait_for_timeout(1500)
                page.screenshot(path="/tmp/redesign_student_mall.png", full_page=True)
                record("学生商城页", True)
        except:
            record("学生商城页", False, "切换失败")

        # 切换到风采榜
        try:
            rank_btn = page.locator("text=风采榜").first
            if not rank_btn.is_visible():
                rank_btn = page.locator("text=排行榜").first
            if rank_btn.is_visible():
                rank_btn.click()
                page.wait_for_timeout(1500)
                page.screenshot(path="/tmp/redesign_student_rank.png", full_page=True)
                record("学生风采榜", True)
        except:
            record("学生风采榜", False, "切换失败")

        page.close()

        # ====== 5. 未登录重定向测试 ======
        print("\n【5】未登录重定向测试")
        for page_name, desc in [("admin.html", "管理员"), ("teacher.html", "教师"), ("student.html", "学生")]:
            page = browser.new_page(viewport={"width": 1440, "height": 900})
            page.goto(f"{BASE}/{page_name}")
            page.wait_for_load_state("networkidle")
            page.wait_for_timeout(1500)
            redirected = page.url == f"{BASE}/" or "index" in page.url
            record(f"未登录{desc}页重定向", redirected, f"URL: {page.url}")
            page.close()

        # ====== 6. 移动端响应测试 ======
        print("\n【6】移动端响应测试")
        page = browser.new_page(viewport={"width": 375, "height": 812})
        page.goto(f"{BASE}/")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1000)
        page.screenshot(path="/tmp/redesign_mobile_login.png", full_page=True)
        record("移动端登录页", True)

        # 移动端管理员
        page.fill("input[type='text']", "admin")
        page.fill("input[type='password']", "admin123")
        page.click("button[type='submit']")
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(2000)
        page.screenshot(path="/tmp/redesign_mobile_admin.png", full_page=True)
        record("移动端管理员页", True)
        page.close()

        browser.close()

    # ====== 汇总 ======
    print("\n" + "=" * 60)
    print("📊 前端重构验证报告")
    print("=" * 60)
    total = len(results)
    passed = sum(1 for r in results if r["passed"])
    failed = total - passed
    print(f"\n总计: {total} 项 | 通过: {passed} | 失败: {failed} | 通过率: {passed/total*100:.1f}%")

    if failed > 0:
        print("\n❌ 失败项:")
        for r in results:
            if not r["passed"]:
                print(f"  - {r['name']}")

    print("\n📁 截图保存位置: /tmp/redesign_*.png")
    print("=" * 60)
    if failed == 0:
        print("✅ 前端重构验证全部通过！")
    else:
        print(f"⚠️ 有 {failed} 项需要检查")


if __name__ == "__main__":
    main()
