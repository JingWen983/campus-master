from playwright.sync_api import sync_playwright

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    context = browser.new_context()
    page = context.new_page()

    # Capture console errors
    errors = []
    page.on('console', lambda msg: errors.append(f"{msg.type}: {msg.text}") if msg.type == 'error' else None)

    # Login as admin first
    page.goto('http://localhost:8080')
    page.wait_for_load_state('networkidle')
    page.fill('input[type="text"]', 'admin')
    page.fill('input[type="password"]', 'admin123')
    page.click('button[type="submit"]')
    page.wait_for_timeout(3000)
    print(f"After login: {page.url}")

    # Check /api/auth/me directly
    response = page.evaluate('''
        async () => {
            const res = await fetch('/api/auth/me', { credentials: 'include' });
            return await res.json();
        }
    ''')
    print(f"/api/auth/me response: {response}")

    # Now navigate to index.html and wait longer
    print("\nNavigating to index.html...")
    page.goto('http://localhost:8080/index.html')
    page.wait_for_timeout(5000)
    print(f"URL after 5s: {page.url}")

    if 'admin.html' in page.url:
        print("[PASS] Auto-redirect worked!")
    else:
        print(f"[FAIL] Still at: {page.url}")
        print(f"Console errors: {errors}")
        # Check page content
        content = page.content()
        if 'mounted' in content:
            print("mounted keyword found in page source")
        else:
            print("mounted keyword NOT found in page source")

    browser.close()
