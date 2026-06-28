"""测试三端"最近活动"组件是否包含兑换记录"""
from playwright.sync_api import sync_playwright
import requests

BASE = "http://localhost:8080"

# 先让学生兑换一次，确保有新记录
s = requests.Session()
s.post(f"{BASE}/api/auth/login", json={"username": "student", "password": "student123"})
r = s.post(f"{BASE}/api/mall/redeem", json={"item_id": 1, "cost": 10})
print("pre: student redeem =", r.json().get("code"))

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)

    # === admin 端"最近活动" ===
    print("=== ADMIN 最近活动 ===")
    ctx = browser.new_context()
    page = ctx.new_page()
    page.goto(f"{BASE}/index.html")
    page.wait_for_load_state("networkidle")
    page.fill('input[autocomplete="username"]', "admin")
    page.fill('input[autocomplete="current-password"]', "admin123")
    page.click('button[type="submit"]')
    page.wait_for_url("**/admin.html", timeout=5000)
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(2000)
    # dashboard 是默认 tab，"最近活动"应该已加载
    content = page.content()
    has_redeem_activity = "文具套装" in content or "兑换" in content
    print("admin 最近活动 has 兑换/文具套装:", has_redeem_activity)
    # 查看最近活动卡片内容
    activities = page.locator('text=最近活动').locator('..').locator('..').inner_text()
    print("admin 最近活动内容:")
    print(activities[:500])
    ctx.close()

    # === teacher 端"最近操作记录" ===
    print("\n=== TEACHER 最近操作记录 ===")
    ctx = browser.new_context()
    page = ctx.new_page()
    page.goto(f"{BASE}/index.html")
    page.wait_for_load_state("networkidle")
    page.fill('input[autocomplete="username"]', "teacher")
    page.fill('input[autocomplete="current-password"]', "teacher123")
    page.click('button[type="submit"]')
    page.wait_for_url("**/teacher.html", timeout=5000)
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(2000)
    content = page.content()
    has_redeem_activity = "兑换" in content or "文具套装" in content
    print("teacher 最近操作记录 has 兑换/文具套装:", has_redeem_activity)
    activities = page.locator('text=最近操作记录').locator('..').locator('..').inner_text()
    print("teacher 最近操作记录内容:")
    print(activities[:500])
    ctx.close()

    # === student 端"最近能量动态" ===
    print("\n=== STUDENT 最近能量动态 ===")
    ctx = browser.new_context()
    page = ctx.new_page()
    page.goto(f"{BASE}/index.html")
    page.wait_for_load_state("networkidle")
    page.fill('input[autocomplete="username"]', "student")
    page.fill('input[autocomplete="current-password"]', "student123")
    page.click('button[type="submit"]')
    page.wait_for_url("**/student.html", timeout=5000)
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(2000)
    content = page.content()
    has_redeem_activity = "兑换" in content or "文具套装" in content
    print("student 最近能量动态 has 兑换/文具套装:", has_redeem_activity)
    activities = page.locator('text=最近能量动态').locator('..').locator('..').inner_text()
    print("student 最近能量动态内容:")
    print(activities[:500])
    ctx.close()

    browser.close()
print("\nDONE")
