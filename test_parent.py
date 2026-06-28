import requests
import json

BASE = 'http://localhost:8080'

# 1. 测试家长登录
print('=== 测试家长登录 ===')
resp = requests.post(BASE + '/api/parent/login', json={'username': 'student', 'parent_password': 'parent123'})
print('状态码:', resp.status_code)
data = resp.json()
print('响应:', json.dumps(data, ensure_ascii=False, indent=2))

if data.get('code') != 200:
    print('家长登录失败！')
    exit(1)

token = data['data']['token']
children = data['data']['children']
print('\nToken:', token)
print('子女列表:', children)

child_id = children[0]['id']

# 2. 测试获取子女列表
print('\n=== 测试获取子女列表 ===')
resp = requests.get(BASE + '/api/parent/children', headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 3. 测试获取子女信息
print('\n=== 测试获取子女信息 (id=%d) ===' % child_id)
resp = requests.get(BASE + '/api/parent/student/%d/info' % child_id, headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 4. 测试获取积分记录
print('\n=== 测试获取积分记录 ===')
resp = requests.get(BASE + '/api/parent/student/%d/points' % child_id, headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 5. 测试获取评价记录
print('\n=== 测试获取评价记录 ===')
resp = requests.get(BASE + '/api/parent/student/%d/evaluation' % child_id, headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 6. 测试获取兑换记录
print('\n=== 测试获取兑换记录 ===')
resp = requests.get(BASE + '/api/parent/student/%d/redemptions' % child_id, headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 7. 测试获取留言列表
print('\n=== 测试获取留言列表 ===')
resp = requests.get(BASE + '/api/parent/student/%d/messages' % child_id, headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 8. 测试发送留言
print('\n=== 测试发送留言 ===')
resp = requests.post(BASE + '/api/parent/student/%d/messages' % child_id,
                    json={'content': '老师您好，请问孩子最近表现如何？'},
                    headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 9. 再次获取留言列表验证
print('\n=== 验证留言已发送 ===')
resp = requests.get(BASE + '/api/parent/student/%d/messages' % child_id, headers={'Authorization': token})
print('状态码:', resp.status_code)
msgs = resp.json().get('data', [])
print('留言数量:', len(msgs))
for m in msgs:
    sender = m.get('sender_type', '')
    content = m.get('content', '')
    created = m.get('created_at', '')
    print('  - [%s] %s (%s)' % (sender, content, created))

# 10. 测试越权访问（访问不存在的学生）
print('\n=== 测试越权访问 (id=999) ===')
resp = requests.get(BASE + '/api/parent/student/999/info', headers={'Authorization': token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 11. 测试教师端留言 API
print('\n=== 测试教师登录 ===')
resp = requests.post(BASE + '/api/auth/login', json={'username': 'teacher', 'password': 'teacher123'})
teacher_token = resp.json()['data']['token']
print('教师 Token:', teacher_token)

# 12. 教师获取家长留言列表
print('\n=== 教师获取家长留言列表 ===')
resp = requests.get(BASE + '/api/teacher/parent-messages', headers={'Authorization': teacher_token})
print('状态码:', resp.status_code)
print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 13. 教师回复留言
if resp.json().get('data') and len(resp.json()['data']) > 0:
    msg_id = resp.json()['data'][0]['id']
    print('\n=== 教师回复留言 (id=%d) ===' % msg_id)
    resp = requests.post(BASE + '/api/teacher/parent-messages/%d/reply' % msg_id,
                        json={'content': '家长您好，孩子最近表现很好，上课认真听讲。'},
                        headers={'Authorization': teacher_token})
    print('状态码:', resp.status_code)
    print('响应:', json.dumps(resp.json(), ensure_ascii=False, indent=2))

# 14. 验证家长端能看到教师回复
print('\n=== 家长端验证教师回复 ===')
resp = requests.get(BASE + '/api/parent/student/%d/messages' % child_id, headers={'Authorization': token})
msgs = resp.json().get('data', [])
print('留言总数:', len(msgs))
for m in msgs:
    sender = m.get('sender_type', '')
    content = m.get('content', '')
    print('  - [%s] %s' % (sender, content))

print('\n=== 所有测试完成 ===')
