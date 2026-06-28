from playwright.sync_api import sync_playwright
import json

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    context = browser.new_context()
    page = context.new_page()

    # ===== Test 1: Admin login =====
    print("=== Test 1: Admin login ===")
    page.goto('http://localhost:8080')
    page.wait_for_load_state('networkidle')
    page.fill('input[type="text"]', 'admin')
    page.fill('input[type="password"]', 'admin123')
    page.click('button[type="submit"]')
    page.wait_for_timeout(3000)
    if 'admin.html' in page.url:
        print("  [PASS] Admin login redirected to admin.html")
    else:
        print(f"  [FAIL] URL: {page.url}")

    # ===== Test 2: Roles API returns permissions =====
    print("\n=== Test 2: Roles API permissions ===")
    response = page.evaluate('''
        async () => {
            const res = await fetch('/api/admin/roles', { credentials: 'include' });
            return await res.json();
        }
    ''')
    roles = response.get('data', [])
    all_pass = True
    for role in roles:
        perms = role.get('permissions', [])
        if perms:
            print(f"  [PASS] {role.get('name')}: {len(perms)} perms - {[p.get('name') for p in perms]}")
        else:
            print(f"  [FAIL] {role.get('name')}: no permissions")
            all_pass = False

    # ===== Test 3: Users API includes parent users =====
    print("\n=== Test 3: Users API parent users ===")
    response = page.evaluate('''
        async () => {
            const res = await fetch('/api/admin/users', { credentials: 'include' });
            return await res.json();
        }
    ''')
    users = response.get('data', [])
    parent_users = [u for u in users if u.get('role_id') == 4]
    print(f"  Total users: {len(users)}, Parents: {len(parent_users)}")
    if parent_users:
        print(f"  [PASS] Parent user: {parent_users[0].get('name')}")
    else:
        print("  [FAIL] No parent users")

    # ===== Test 4: Auto-redirect when logged in =====
    print("\n=== Test 4: Auto-redirect ===")
    page.goto('http://localhost:8080/index.html')
    page.wait_for_timeout(3000)
    if 'admin.html' in page.url:
        print("  [PASS] Auto-redirected to admin.html")
    else:
        print(f"  [FAIL] Still at: {page.url}")

    # ===== Test 5: Unified login form =====
    print("\n=== Test 5: Unified login form ===")
    page.evaluate('''async () => { await fetch('/api/auth/logout', {method:'POST',credentials:'include'}); }''')
    page.wait_for_timeout(500)
    page.goto('http://localhost:8080')
    page.wait_for_load_state('networkidle')
    page.wait_for_timeout(1000)
    switch_btn = page.locator('text=家长登录')
    if switch_btn.count() == 0:
        print("  [PASS] No mode switch button")
    else:
        print("  [FAIL] Mode switch still exists")

    # Test parent login
    print("\n=== Test 5b: Parent login ===")
    page.fill('input[type="text"]', 'student')
    page.fill('input[type="password"]', 'parent123')
    page.click('button[type="submit"]')
    page.wait_for_timeout(3000)
    if 'parent.html' in page.url:
        print("  [PASS] Parent login -> parent.html")
    else:
        print(f"  [FAIL] URL: {page.url}")

    # ===== Test 6: Student logout button =====
    print("\n=== Test 6: Student logout button ===")
    page.evaluate('''async () => { await fetch('/api/auth/logout', {method:'POST',credentials:'include'}); }''')
    page.wait_for_timeout(500)
    page.goto('http://localhost:8080')
    page.wait_for_load_state('networkidle')
    page.wait_for_timeout(500)
    page.fill('input[type="text"]', 'student')
    page.fill('input[type="password"]', 'student123')
    page.click('button[type="submit"]')
    page.wait_for_timeout(2000)
    if 'student.html' in page.url:
        page.wait_for_timeout(1000)
        btn = page.locator('button:has(i.fa-right-from-bracket)')
        if btn.count() > 0:
            cls = btn.first.get_attribute('class')
            if 'emerald' in cls:
                print(f"  [PASS] Logout button uses emerald theme: {cls}")
            else:
                print(f"  [INFO] Button class: {cls}")
        else:
            print("  [FAIL] No logout button found")
    else:
        print(f"  [FAIL] Student login failed: {page.url}")

    browser.close()
    print("\n=== All tests completed ===")
