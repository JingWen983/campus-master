"""测试四端兑换记录 UI 是否实时显示"""
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

    # === 测试 admin 端 ===
    print("=== ADMIN ===")
    ctx = browser.new_context()
    page = ctx.new_page()
    errs = []
    page.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
    page.goto(f"{BASE}/index.html")
    page.wait_for_load_state("networkidle")
    page.fill('input[autocomplete="username"]', "admin")
    page.fill('input[autocomplete="current-password"]', "admin123")
    page.click('button[type="submit"]')
    page.wait_for_url("**/admin.html", timeout=5000)
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(1500)

    # 切换到商城管理 tab (兑换记录在此 tab 内)
    page.click('button:has-text("商城管理")')
    page.wait_for_timeout(2000)
    content = page.content()
    print("admin mall tab has 兑换记录:", "兑换记录" in content)
    print("admin mall tab has 文具套装:", "文具套装" in content)
    print("admin errors:", len(errs), errs[:3])
    ctx.close()

    # === 测试 teacher 端 ===
    print("=== TEACHER ===")
    ctx = browser.new_context()
    page = ctx.new_page()
    errs = []
    page.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
    page.goto(f"{BASE}/index.html")
    page.wait_for_load_state("networkidle")
    page.fill('input[autocomplete="username"]', "teacher")
    page.fill('input[autocomplete="current-password"]', "teacher123")
    page.click('button[type="submit"]')
    page.wait_for_url("**/teacher.html", timeout=5000)
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(1500)

    # 切换到兑换记录 tab
    try:
        page.click('button:has-text("兑换记录")', timeout=3000)
        page.wait_for_timeout(2000)
        content = page.content()
        print("teacher redemptions tab has 文具套装:", "文具套装" in content)
        print("teacher redemptions tab has 张同学:", "张同学" in content)
    except Exception as e:
        print("teacher click 兑换记录 failed:", e)
    print("teacher errors:", len(errs), errs[:3])
    ctx.close()

    # === 测试 parent 端 ===
    print("=== PARENT ===")
    ctx = browser.new_context()
    page = ctx.new_page()
    errs = []
    page.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
    page.goto(f"{BASE}/index.html")
    page.wait_for_load_state("networkidle")
    page.fill('input[autocomplete="username"]', "parent")
    page.fill('input[autocomplete="current-password"]', "parent123")
    page.click('button[type="submit"]')
    page.wait_for_url("**/parent.html", timeout=5000)
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(2000)
    content = page.content()
    print("parent initial has 兑换记录 tab:", "兑换记录" in content)

    # 点击兑换记录 tab
    try:
        page.click('button:has-text("兑换记录")', timeout=3000)
        page.wait_for_timeout(2000)
        content = page.content()
        print("parent redemptions tab has 文具套装:", "文具套装" in content)
    except Exception as e:
        print("parent click 兑换记录 failed:", e)
    print("parent errors:", len(errs), errs[:3])
    ctx.close()

    browser.close()
print("DONE")
