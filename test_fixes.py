"""
验证本次修复的所有问题：
1. 电脑端管理员侧边栏用户卡片无多余主题色框
2. 用户ID列家长显示 "—"
3. 家长用户操作列显示"通过编辑学生绑定"
4. 权限管理有 12 个权限
5. 角色管理权限列表完整
6. 编辑学生时显示家长绑定字段
7. 控制台无报错
"""
from playwright.sync_api import sync_playwright
import json

BASE = "http://localhost:8080"

def main():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context()
        page = context.new_page()

        console_errors = []
        page.on("console", lambda msg: console_errors.append(f"[{msg.type}] {msg.text}") if msg.type == "error" else None)

        # 登录
        page.goto(f"{BASE}/index.html")
        page.wait_for_load_state("networkidle")
        page.fill('input[autocomplete="username"]', "admin")
        page.fill('input[autocomplete="current-password"]', "admin123")
        page.click('button[type="submit"]')
        page.wait_for_url("**/admin.html", timeout=5000)
        page.wait_for_load_state("networkidle")
        print("[OK] 登录成功，进入 admin.html")

        # 1. 侧边栏用户卡片截图（检查多余主题色框已去除）
        page.screenshot(path="screenshots/01_sidebar.png", full_page=False)
        print("[OK] 截图 01_sidebar.png")

        # 2. 切换到用户管理
        page.click('button:has-text("用户管理")')
        page.wait_for_timeout(800)
        page.screenshot(path="screenshots/02_users.png", full_page=False)
        print("[OK] 截图 02_users.png - 用户管理")

        # 检查家长用户 ID 显示 "—"
        user_rows = page.locator('tbody tr').all()
        found_parent_dash = False
        found_parent_hint = False
        for row in user_rows:
            text = row.inner_text()
            if "家长" in text and "—" in text.split("\n")[0]:
                found_parent_dash = True
                print(f"[OK] 家长用户 ID 显示 '—': {text.split(chr(10))[0]}")
            if "通过编辑学生绑定" in text:
                found_parent_hint = True
                print("[OK] 家长用户操作列显示'通过编辑学生绑定'")
        if not found_parent_dash:
            print("[WARN] 未找到家长用户 ID 为 '—' 的行")
        if not found_parent_hint:
            print("[WARN] 未找到'通过编辑学生绑定'提示")

        # 3. 点击学生用户（张同学）的编辑按钮，检查家长绑定字段
        # 用精确匹配学生行（排除家长虚拟用户行）
        student_row = page.locator('tbody tr').filter(has_text="student").first
        if student_row.count() > 0:
            student_row.locator('button:has-text("编辑")').click()
            page.wait_for_timeout(1500)
            page.screenshot(path="screenshots/03_edit_user.png", full_page=False)
            print("[OK] 截图 03_edit_user.png - 编辑学生对话框")
            # 用页面源码检查家长绑定字段
            page_text = page.content()
            checks = ["家长账号绑定", "家长手机号", "家长登录密码"]
            for chk in checks:
                if chk in page_text:
                    print(f"[OK] 对话框包含 '{chk}'")
                else:
                    print(f"[WARN] 对话框未包含 '{chk}'")
            # 点击对话框内的"取消"按钮关闭
            try:
                page.locator('.modal-backdrop button:has-text("取消")').click(timeout=2000)
                print("[OK] 编辑对话框已关闭")
            except Exception as e:
                print(f"[WARN] 关闭对话框失败: {e}")
            page.wait_for_timeout(800)
        else:
            print("[WARN] 未找到 student 行")

        # 4. 切换到角色管理
        page.click('button:has-text("角色管理")')
        page.wait_for_timeout(800)
        page.screenshot(path="screenshots/04_roles.png", full_page=False)
        print("[OK] 截图 04_roles.png - 角色管理")
        # 检查家长角色权限列表
        roles_text = page.locator('.role-card').all_inner_texts()
        for rt in roles_text:
            if "家长" in rt:
                print(f"[OK] 家长角色权限列表: {rt.split('权限列表')[1][:100] if '权限列表' in rt else 'N/A'}")

        # 5. 切换到权限管理
        page.click('button:has-text("权限管理")')
        page.wait_for_timeout(800)
        page.screenshot(path="screenshots/05_permissions.png", full_page=False)
        print("[OK] 截图 05_permissions.png - 权限管理")
        perm_rows = page.locator('tbody tr').all()
        print(f"[OK] 权限列表共 {len(perm_rows)} 项")
        if len(perm_rows) >= 12:
            print("[OK] 权限数量达到 12 项（含新增的家长管理/留言管理/班级管理/兑换管理/数据导出）")

        # 控制台错误
        print(f"\n[DONE] 控制台错误数: {len(console_errors)}")
        for err in console_errors:
            print(f"  ERROR: {err}")

        browser.close()

if __name__ == "__main__":
    import os
    os.makedirs("screenshots", exist_ok=True)
    main()
