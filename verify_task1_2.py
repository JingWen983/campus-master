"""验证 Task 1 (对话框滚动) 和 Task 2 (操作记录显示)"""
from playwright.sync_api import sync_playwright

BASE = "http://localhost:8080"


def main():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        # === Task 2.4: 验证三端操作记录 ===
        # 1. 学生端行为记录
        ctx = browser.new_context()
        page = ctx.new_page()
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "student")
        page.fill('input[autocomplete="current-password"]', "student123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/student.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)
        # 检查行为记录是否有积分数字
        history_items = page.locator('[v-for*="record in history"], .history-item, [class*="record"]').all()
        content = page.content()
        has_points = "points" in content or any("+" in (i.inner_text() if i.count() else "") for i in history_items[:5])
        print(f"[student] 控制台错误: {len(errors)}, 页面含积分相关: {has_points}")
        for e in errors:
            print(f"  ERR: {e}")
        ctx.close()

        # 2. 管理员端仪表盘最近活动
        ctx = browser.new_context()
        page = ctx.new_page()
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "admin")
        page.fill('input[autocomplete="current-password"]', "admin123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/admin.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        page.wait_for_timeout(1500)
        content = page.content()
        # 检查是否有 recentActivities 渲染（非"暂无活动记录"）
        no_activity = "暂无活动记录" in content
        print(f"[admin] 控制台错误: {len(errors)}, 显示'暂无活动记录': {no_activity}")
        for e in errors:
            print(f"  ERR: {e}")

        # === Task 1: 验证编辑对话框滚动 ===
        # 切换到用户管理，打开编辑学生对话框
        try:
            page.click('button:has-text("用户管理")')
            page.wait_for_timeout(800)
            student_row = page.locator('tbody tr').filter(has_text="student").first
            if student_row.count() > 0:
                student_row.locator('button:has-text("编辑")').click()
                page.wait_for_timeout(1000)
                # 检查 modal-panel 是否含 max-h 和 overflow-y-auto
                panel_class = page.locator('.modal-panel').first.get_attribute('class')
                has_scroll = panel_class and 'max-h' in panel_class and 'overflow-y-auto' in panel_class
                print(f"[admin] 编辑对话框 modal-panel class 含滚动样式: {has_scroll}")
                if panel_class:
                    print(f"  class: {panel_class[:120]}...")
        except Exception as e:
            print(f"[admin] 验证编辑对话框失败: {e}")
        ctx.close()

        browser.close()
        print("\n[DONE] 验证完成")


if __name__ == "__main__":
    main()
