from playwright.sync_api import sync_playwright

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    context = browser.new_context()
    page = context.new_page()

    # 捕获控制台错误
    errors = []
    page.on('console', lambda msg: errors.append(f'[{msg.type}] {msg.text}') if msg.type in ['error', 'warning'] else None)
    page.on('pageerror', lambda err: errors.append(f'[pageerror] {str(err)}'))

    # 先登录教师账号
    print('=== 登录教师账号 ===')
    page.goto('http://localhost:8080/')
    page.wait_for_load_state('networkidle')
    page.wait_for_timeout(1000)
    print(f'登录页标题: {page.title()}')

    # 填写登录表单
    page.fill('input[type="text"], input[placeholder*="用户名"], input[v-model="username"]', 'teacher')
    page.fill('input[type="password"], input[placeholder*="密码"], input[v-model="password"]', 'teacher123')
    page.wait_for_timeout(500)

    # 点击登录按钮
    login_btn = page.locator('button:has-text("登录"), button[type="submit"]').first
    print(f'登录按钮文本: {login_btn.inner_text()}')
    login_btn.click()
    page.wait_for_timeout(2000)
    page.wait_for_load_state('networkidle')
    print(f'登录后标题: {page.title()}')
    print(f'登录后 URL: {page.url}')

    # 检查 localStorage
    token = page.evaluate('localStorage.getItem("token")')
    userinfo = page.evaluate('localStorage.getItem("userInfo")')
    print(f'token: {token}')
    print(f'userInfo: {userinfo}')

    # 访问教师端页面
    print('\n=== 访问教师端页面 ===')
    page.goto('http://localhost:8080/teacher.html')
    page.wait_for_load_state('networkidle')
    page.wait_for_timeout(2000)
    print(f'教师端标题: {page.title()}')
    print(f'教师端 URL: {page.url}')

    # 截图
    page.screenshot(path='d:/邵敬文/comptation/test_teacher_after_login.png', full_page=True)
    print('截图已保存')

    # 检查侧边栏导航项
    nav_items = page.locator('.nav-item').count()
    print(f'侧边栏导航项数量: {nav_items}')

    if nav_items > 0:
        # 尝试点击每个导航项
        for i in range(nav_items):
            nav = page.locator('.nav-item').nth(i)
            text = nav.inner_text().strip().replace('\n', ' ')
            visible = nav.is_visible()
            enabled = nav.is_enabled()
            print(f'  导航项 {i}: "{text}" visible={visible} enabled={enabled}')
            try:
                nav.click(timeout=3000)
                print(f'    点击: 成功')
                page.wait_for_timeout(500)
            except Exception as e:
                print(f'    点击失败: {e}')

    # 检查是否有覆盖层阻挡点击
    print('\n=== 检查覆盖层 ===')
    overlays = page.evaluate('''() => {
        const elements = document.querySelectorAll('*');
        const overlays = [];
        for (const el of elements) {
            const style = window.getComputedStyle(el);
            const zIndex = parseInt(style.zIndex) || 0;
            if (zIndex > 10 && style.position !== 'static') {
                const rect = el.getBoundingClientRect();
                if (rect.width > 100 && rect.height > 100) {
                    overlays.push({
                        tag: el.tagName,
                        id: el.id,
                        class: (el.className || '').toString().substring(0, 60),
                        zIndex: zIndex,
                        position: style.position,
                        display: style.display,
                        size: `${Math.round(rect.width)}x${Math.round(rect.height)}`
                    });
                }
            }
        }
        return overlays;
    }''')
    for o in overlays:
        print(o)

    # 打印控制台错误
    print(f'\n=== 控制台错误/警告 ===')
    for e in errors[:30]:
        print(e)

    browser.close()
