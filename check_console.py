"""
检查所有前端页面的控制台报错
"""
from playwright.sync_api import sync_playwright

BASE = "http://localhost:8080"


def check_page(page, name, url, login_user=None, login_pass=None, login_btn=None,
               expect_url=None, post_login_action=None):
    """检查单个页面的控制台错误"""
    errors = []
    page.on("console", lambda msg: errors.append(f"[{msg.type}] {msg.text}") if msg.type == "error" else None)
    page.on("pageerror", lambda exc: errors.append(f"[pageerror] {exc}"))
    page.on("requestfailed", lambda req: errors.append(f"[requestfailed] {req.url} - {req.failure}"))
    page.on("response", lambda resp: errors.append(f"[404] {resp.url}") if resp.status == 404 else None)

    if login_user:
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', login_user)
        page.fill('input[autocomplete="current-password"]', login_pass)
        page.click(login_btn or 'button[type="submit"]')
        if expect_url:
            page.wait_for_url(expect_url, timeout=5000)
        page.wait_for_load_state("networkidle")
    else:
        page.goto(f"{BASE}/{url}")
        page.wait_for_load_state("networkidle")

    if post_login_action:
        post_login_action(page)
        page.wait_for_timeout(1500)

    # 触发各 tab 切换以捕获潜在错误
    tabs = page.locator('button:has-text("管理"), button:has-text("查询"), button:has-text("中心"), button:has-text("留言"), button:has-text("商城"), button:has-text("排行榜"), button:has-text("记录"), button:has-text("信息")').all()
    for tab in tabs[:8]:
        try:
            tab.click(timeout=1000)
            page.wait_for_timeout(400)
        except Exception:
            pass

    return errors


def main():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        results = {}

        # 1. admin
        ctx = browser.new_context()
        page = ctx.new_page()
        errs = check_page(page, "admin", "admin.html",
                          login_user="admin", login_pass="admin123",
                          expect_url="**/admin.html")
        results["admin"] = errs
        ctx.close()

        # 2. teacher
        ctx = browser.new_context()
        page = ctx.new_page()
        errs = check_page(page, "teacher", "teacher.html",
                          login_user="teacher", login_pass="teacher123",
                          expect_url="**/teacher.html")
        results["teacher"] = errs
        ctx.close()

        # 3. student
        ctx = browser.new_context()
        page = ctx.new_page()
        errs = check_page(page, "student", "student.html",
                          login_user="student", login_pass="student123",
                          expect_url="**/student.html")
        results["student"] = errs
        ctx.close()

        # 4. parent (家长用子女学号 + 家长密码登录)
        ctx = browser.new_context()
        page = ctx.new_page()
        errs = check_page(page, "parent", "parent.html",
                          login_user="student", login_pass="parent123",
                          expect_url="**/parent.html")
        results["parent"] = errs
        ctx.close()

        print("=" * 60)
        print("控制台错误检查结果")
        print("=" * 60)
        total = 0
        for name, errs in results.items():
            print(f"\n[{name}] 错误数: {len(errs)}")
            total += len(errs)
            for e in errs:
                print(f"  - {e}")
        print(f"\n{'=' * 60}")
        print(f"总错误数: {total}")
        print("=" * 60)

        browser.close()


if __name__ == "__main__":
    main()
