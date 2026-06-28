from playwright.sync_api import sync_playwright

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    context = browser.new_context()
    page = context.new_page()

    errors = []
    page.on('console', lambda msg: errors.append(f'[{msg.type}] {msg.text}') if msg.type in ['error', 'warning'] else None)
    page.on('pageerror', lambda err: errors.append(f'[pageerror] {str(err)}'))

    # ========== 测试教师端 ==========
    print('========== 测试教师端 ==========')
    page.goto('http://localhost:8080/')
    page.wait_for_load_state('networkidle')
    page.wait_for_timeout(1000)

    # 登录教师账号
    page.fill('input[type="text"], input[placeholder*="用户名"], input[v-model="username"]', 'teacher')
    page.fill('input[type="password"], input[placeholder*="密码"], input[v-model="password"]', 'teacher123')
    page.wait_for_timeout(500)
    page.locator('button:has-text("登录"), button[type="submit"]').first.click()
    page.wait_for_timeout(2000)
    page.wait_for_load_state('networkidle')
    print(f'登录后 URL: {page.url}')
    print(f'登录后标题: {page.title()}')

    # 检查是否有 formatDateTime 错误
    fmt_errors = [e for e in errors if 'formatDateTime' in e]
    print(f'\nformatDateTime 错误数量: {len(fmt_errors)}')
    if fmt_errors:
        for e in fmt_errors[:3]:
            print(f'  {e}')

    # 测试所有导航项点击
    nav_items = page.locator('.nav-item').count()
    print(f'\n侧边栏导航项数量: {nav_items}')
    all_click_ok = True
    for i in range(nav_items):
        nav = page.locator('.nav-item').nth(i)
        text = nav.inner_text().strip().replace('\n', ' ')
        try:
            nav.click(timeout=3000)
            page.wait_for_timeout(500)
            print(f'  导航项 {i} "{text}": 点击成功')
        except Exception as e:
            print(f'  导航项 {i} "{text}": 点击失败 - {e}')
            all_click_ok = False

    print(f'\n教师端所有导航项可点击: {"是" if all_click_ok else "否"}')

    # ========== 测试家长端 ==========
    print('\n========== 测试家长端 ==========')
    errors.clear()
    page.goto('http://localhost:8080/')
    page.wait_for_load_state('networkidle')
    page.wait_for_timeout(1000)

    # 切换到家长登录模式
    parent_btn = page.locator('text=家长登录').first
    if parent_btn.is_visible():
        parent_btn.click()
        page.wait_for_timeout(500)

    # 登录家长账号
    page.fill('input[type="text"], input[placeholder*="学号"], input[v-model="username"]', 'student')
    page.fill('input[type="password"], input[placeholder*="密码"], input[v-model="password"]', 'parent123')
    page.wait_for_timeout(500)
    page.locator('button:has-text("登录"), button[type="submit"]').first.click()
    page.wait_for_timeout(2000)
    page.wait_for_load_state('networkidle')
    print(f'登录后 URL: {page.url}')
    print(f'登录后标题: {page.title()}')

    # 检查是否有 tab-btn 横栏
    tab_btn_count = page.locator('.tab-btn').count()
    print(f'\n主页面 tab-btn 横栏数量: {tab_btn_count}（应为 0）')

    # 检查侧边栏导航
    side_nav_count = page.locator('aside .nav-item').count()
    print(f'侧边栏导航项数量: {side_nav_count}（应为 4）')

    # 测试侧边栏导航点击
    print('\n测试侧边栏导航点击:')
    for i in range(side_nav_count):
        nav = page.locator('aside .nav-item').nth(i)
        text = nav.inner_text().strip().replace('\n', ' ')
        try:
            nav.click(timeout=3000)
            page.wait_for_timeout(500)
            print(f'  侧边栏导航 {i} "{text}": 点击成功')
        except Exception as e:
            print(f'  侧边栏导航 {i} "{text}": 点击失败 - {e}')

    # 检查移动端底部导航
    bottom_nav_count = page.locator('.bottom-nav-item').count()
    print(f'\n移动端底部导航项数量: {bottom_nav_count}（应为 4）')

    # 截图
    page.screenshot(path='d:/邵敬文/comptation/test_parent_fixed.png', full_page=True)
    print('家长端截图已保存')

    # 打印错误
    parent_errors = [e for e in errors if 'formatDateTime' in e or 'pageerror' in e]
    print(f'\n家长端错误数量: {len(parent_errors)}')
    for e in parent_errors[:5]:
        print(f'  {e}')

    browser.close()
    print('\n========== 验证完成 ==========')
