/**
 * Mock 数据层 —— 用于 GitHub Pages 纯前端 Demo
 *
 * 工作原理：
 * - 所有 API 请求被 mockRequest() 拦截，返回 localStorage 中的数据
 * - 写操作（POST/PUT/DELETE）会更新 localStorage，让 Demo 有"真实感"
 * - 首次加载时初始化种子数据（seed）
 * - 任何角色用任意密码登录都能进入对应页面（演示用）
 *
 * 启用方式：import.meta.env.VITE_USE_MOCK === 'true'
 */
import type { ApiResponse } from '../lib/api'

// ===================== 类型定义 =====================
interface User {
  id: string
  username: string
  name: string
  role_id: number
  className?: string
  points?: number
  password_hash?: string
  parent_password?: string
}

interface PointsRecord {
  id: number
  student_id: string
  points: number
  reason: string
  created_at: string
  teacher_name?: string
}

interface Evaluation {
  id: number
  student_id: string
  dimension_id: number
  score: number
  comment: string
  evaluator_id: string
  evaluator_name: string
  created_at: string
}

interface SchoolClass {
  id: number
  name: string
  grade?: string
}

interface Product {
  id: number
  name: string
  cost: number
  stock: number
  description?: string
  category?: string
}

interface RedemptionRecord {
  id: number
  student_id: string
  student_name?: string
  item_id: number
  item_name?: string
  cost: number
  created_at: string
  status?: string
}

interface Role {
  id: number
  name: string
  description?: string
}

interface Permission {
  id: number
  role_id: number
  permission: string
}

interface ParentMessage {
  id: number
  student_id: string
  parent_id: string
  parent_name: string
  content: string
  reply?: string
  replied_at?: string
  is_read: number
  created_at: string
}

// ===================== 种子数据 =====================
const SEED_USERS: User[] = [
  { id: '1', username: 'admin', name: '系统管理员', role_id: 1, points: 0 },
  { id: '2', username: 'teacher', name: '张老师', role_id: 2, className: '三年级一班' },
  { id: '3', username: 'teacher2', name: '李老师', role_id: 2, className: '三年级二班' },
  { id: 'S001', username: 'student', name: '王小明', role_id: 3, className: '三年级一班', points: 85 },
  { id: 'S002', username: 'student2', name: '李小红', role_id: 3, className: '三年级一班', points: 92 },
  { id: 'S003', username: 'student3', name: '赵小刚', role_id: 3, className: '三年级一班', points: 78 },
  { id: 'S004', username: 'student4', name: '陈小华', role_id: 3, className: '三年级二班', points: 88 },
  { id: 'S005', username: 'student5', name: '刘小丽', role_id: 3, className: '三年级二班', points: 95 },
  { id: 'P001', username: 'parent', name: '王先生', role_id: 4, className: '三年级一班' },
]

const SEED_CLASSES: SchoolClass[] = [
  { id: 1, name: '三年级一班', grade: '三年级' },
  { id: 2, name: '三年级二班', grade: '三年级' },
  { id: 3, name: '四年级一班', grade: '四年级' },
]

const SEED_PRODUCTS: Product[] = [
  { id: 1, name: '文具盒', cost: 20, stock: 50, description: '精美文具盒', category: '文具' },
  { id: 2, name: '课外书', cost: 50, stock: 30, description: '经典读物', category: '图书' },
  { id: 3, name: '体育用品券', cost: 30, stock: 100, description: '可兑换体育用品', category: '体育' },
  { id: 4, name: '免写作业卡', cost: 100, stock: 10, description: '免一次作业', category: '特权' },
]

const SEED_ROLES: Role[] = [
  { id: 1, name: '管理员', description: '系统管理' },
  { id: 2, name: '教师', description: '教学管理' },
  { id: 3, name: '学生', description: '学生端' },
  { id: 4, name: '家长', description: '家长端' },
]

const SEED_PERMISSIONS: Permission[] = [
  { id: 1, role_id: 1, permission: 'admin' },
  { id: 2, role_id: 2, permission: 'teacher' },
  { id: 3, role_id: 3, permission: 'student' },
  { id: 4, role_id: 4, permission: 'parent' },
]

const SEED_POINTS: PointsRecord[] = [
  { id: 1, student_id: 'S001', points: 10, reason: '课堂积极发言', created_at: '2026-07-20 09:30:00', teacher_name: '张老师' },
  { id: 2, student_id: 'S001', points: -5, reason: '迟到', created_at: '2026-07-21 08:15:00', teacher_name: '张老师' },
  { id: 3, student_id: 'S001', points: 20, reason: '帮助同学', created_at: '2026-07-22 14:00:00', teacher_name: '张老师' },
  { id: 4, student_id: 'S002', points: 15, reason: '作业优秀', created_at: '2026-07-20 10:00:00', teacher_name: '张老师' },
  { id: 5, student_id: 'S002', points: 25, reason: '班级服务', created_at: '2026-07-23 11:30:00', teacher_name: '张老师' },
  { id: 6, student_id: 'S003', points: 8, reason: '课堂积极发言', created_at: '2026-07-21 09:00:00', teacher_name: '张老师' },
]

const SEED_EVALUATIONS: Evaluation[] = [
  { id: 1, student_id: 'S001', dimension_id: 1, score: 90, comment: '乐于助人，品德优秀', evaluator_id: '2', evaluator_name: '张老师', created_at: '2026-07-20 16:00:00' },
  { id: 2, student_id: 'S001', dimension_id: 2, score: 85, comment: '学习认真', evaluator_id: '2', evaluator_name: '张老师', created_at: '2026-07-20 16:00:00' },
  { id: 3, student_id: 'S001', dimension_id: 3, score: 88, comment: '体育积极', evaluator_id: '2', evaluator_name: '张老师', created_at: '2026-07-20 16:00:00' },
  { id: 4, student_id: 'S002', dimension_id: 1, score: 95, comment: '品德优秀', evaluator_id: '2', evaluator_name: '张老师', created_at: '2026-07-21 16:00:00' },
]

const SEED_REDEMPTIONS: RedemptionRecord[] = [
  { id: 1, student_id: 'S001', student_name: '王小明', item_id: 1, item_name: '文具盒', cost: 20, created_at: '2026-07-22 12:00:00', status: '已完成' },
  { id: 2, student_id: 'S002', student_name: '李小红', item_id: 2, item_name: '课外书', cost: 50, created_at: '2026-07-23 14:00:00', status: '已完成' },
]

const SEED_MESSAGES: ParentMessage[] = [
  { id: 1, student_id: 'S001', parent_id: 'P001', parent_name: '王先生', content: '老师您好，孩子最近表现如何？', is_read: 0, created_at: '2026-07-22 19:00:00' },
  { id: 2, student_id: 'S001', parent_id: 'P001', parent_name: '王先生', content: '感谢老师的悉心指导！', reply: '感谢您的支持，孩子进步很大。', replied_at: '2026-07-22 20:00:00', is_read: 1, created_at: '2026-07-22 18:00:00' },
]

// ===================== localStorage 持久化 =====================
const STORAGE_KEY = 'campus_mock_db_v1'

interface MockDB {
  users: User[]
  classes: SchoolClass[]
  products: Product[]
  roles: Role[]
  permissions: Permission[]
  points: PointsRecord[]
  evaluations: Evaluation[]
  redemptions: RedemptionRecord[]
  messages: ParentMessage[]
  currentSession: { user_id: string; role_id: number } | null
}

function loadDB(): MockDB {
  const raw = localStorage.getItem(STORAGE_KEY)
  if (raw) {
    try {
      return JSON.parse(raw)
    } catch {
      // 损坏数据，重新初始化
    }
  }
  const db: MockDB = {
    users: [...SEED_USERS],
    classes: [...SEED_CLASSES],
    products: [...SEED_PRODUCTS],
    roles: [...SEED_ROLES],
    permissions: [...SEED_PERMISSIONS],
    points: [...SEED_POINTS],
    evaluations: [...SEED_EVALUATIONS],
    redemptions: [...SEED_REDEMPTIONS],
    messages: [...SEED_MESSAGES],
    currentSession: null,
  }
  saveDB(db)
  return db
}

function saveDB(db: MockDB) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(db))
}

function nextId(records: { id: number }[]): number {
  return records.length ? Math.max(...records.map(r => r.id)) + 1 : 1
}

// ===================== 路由匹配 + 处理器 =====================
type Handler = (db: MockDB, body: any, params: string[]) => ApiResponse

interface Route {
  method: string
  pattern: RegExp
  paramNames: string[]
  handler: Handler
}

function matchRoute(method: string, url: string): { route: Route; params: string[] } | null {
  for (const route of routes) {
    if (route.method !== method) continue
    const match = url.match(route.pattern)
    if (match) {
      return { route, params: match.slice(1) }
    }
  }
  return null
}

// 工具：按 role_id 过滤教师可见学生
function studentsForTeacher(db: MockDB, teacherId: string): User[] {
  const teacher = db.users.find(u => u.id === teacherId && u.role_id === 2)
  if (!teacher?.className) return db.users.filter(u => u.role_id === 3)
  return db.users.filter(u => u.role_id === 3 && u.className === teacher.className)
}

const routes: Route[] = [
  // ========== 认证 ==========
  { method: 'GET', pattern: /^\/api\/auth\/me$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const user = db.users.find(u => u.id === db.currentSession!.user_id)
    if (!user) return { code: 401, msg: '用户不存在' }
    return { code: 200, data: { id: user.id, username: user.username, name: user.name, role_id: user.role_id, className: user.className, points: user.points } }
  }},
  { method: 'POST', pattern: /^\/api\/auth\/login$/, paramNames: [], handler: (db, body) => {
    const user = db.users.find(u => u.username === body.username)
    if (!user) return { code: 401, msg: '用户不存在' }
    // Mock 模式：任意密码都能登录（演示用）
    db.currentSession = { user_id: user.id, role_id: user.role_id }
    saveDB(db)
    return { code: 200, msg: '登录成功', data: { user: { id: user.id, username: user.username, name: user.name, role_id: user.role_id, className: user.className, points: user.points }, csrf_token: 'mock-csrf-token' } }
  }},
  { method: 'POST', pattern: /^\/api\/parent\/login$/, paramNames: [], handler: (db, body) => {
    const user = db.users.find(u => u.username === body.username && u.role_id === 4)
    if (!user) return { code: 401, msg: '家长账号不存在' }
    db.currentSession = { user_id: user.id, role_id: 4 }
    saveDB(db)
    const children = db.users.filter(u => u.role_id === 3 && u.className === user.className)
    return { code: 200, msg: '登录成功', data: { user: { id: user.id, username: user.username, name: user.name, role_id: 4, className: user.className }, children, csrf_token: 'mock-csrf-token' } }
  }},
  { method: 'POST', pattern: /^\/api\/auth\/logout$/, paramNames: [], handler: (db) => {
    db.currentSession = null
    saveDB(db)
    return { code: 200, msg: '退出成功' }
  }},
  { method: 'POST', pattern: /^\/api\/parent\/logout$/, paramNames: [], handler: (db) => {
    db.currentSession = null
    saveDB(db)
    return { code: 200, msg: '退出成功' }
  }},

  // ========== 管理员 API ==========
  { method: 'GET', pattern: /^\/api\/admin\/users$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.users }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/users$/, paramNames: [], handler: (db, body) => {
    const id = 'U' + String(nextId(db.users)).padStart(4, '0')
    const user: User = { id, username: body.username, name: body.name, role_id: body.role_id, className: body.className, points: 0 }
    db.users.push(user)
    saveDB(db)
    return { code: 200, msg: '创建成功', data: user }
  }},
  { method: 'PUT', pattern: /^\/api\/admin\/users\/([^/]+)$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.users.findIndex(u => u.id === params[0])
    if (idx < 0) return { code: 404, msg: '用户不存在' }
    db.users[idx] = { ...db.users[idx], ...body }
    saveDB(db)
    return { code: 200, msg: '更新成功', data: db.users[idx] }
  }},
  { method: 'DELETE', pattern: /^\/api\/admin\/users$/, paramNames: [], handler: (db, body) => {
    db.users = db.users.filter(u => u.id !== body.id)
    saveDB(db)
    return { code: 200, msg: '删除成功' }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/users\/reset-password$/, paramNames: [], handler: (db, body) => {
    return { code: 200, msg: '密码已重置', data: { new_password: 'newpass123' } }
  }},
  { method: 'GET', pattern: /^\/api\/admin\/roles$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.roles }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/roles$/, paramNames: [], handler: (db, body) => {
    const role: Role = { id: nextId(db.roles), name: body.name, description: body.description }
    db.roles.push(role)
    saveDB(db)
    return { code: 200, msg: '创建成功', data: role }
  }},
  { method: 'PUT', pattern: /^\/api\/admin\/roles\/([^/]+)$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.roles.findIndex(r => r.id === Number(params[0]))
    if (idx < 0) return { code: 404, msg: '角色不存在' }
    db.roles[idx] = { ...db.roles[idx], ...body }
    saveDB(db)
    return { code: 200, msg: '更新成功', data: db.roles[idx] }
  }},
  { method: 'DELETE', pattern: /^\/api\/admin\/roles$/, paramNames: [], handler: (db, body) => {
    const id = Number(body.id)
    db.roles = db.roles.filter(r => r.id !== id)
    saveDB(db)
    return { code: 200, msg: '删除成功' }
  }},
  { method: 'GET', pattern: /^\/api\/admin\/permissions$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.permissions }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/permissions$/, paramNames: [], handler: (db, body) => {
    const perm: Permission = { id: nextId(db.permissions), role_id: body.role_id, permission: body.permission }
    db.permissions.push(perm)
    saveDB(db)
    return { code: 200, msg: '创建成功', data: perm }
  }},
  { method: 'PUT', pattern: /^\/api\/admin\/permissions\/([^/]+)$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.permissions.findIndex(p => p.id === Number(params[0]))
    if (idx < 0) return { code: 404, msg: '权限不存在' }
    db.permissions[idx] = { ...db.permissions[idx], ...body }
    saveDB(db)
    return { code: 200, msg: '更新成功', data: db.permissions[idx] }
  }},
  { method: 'DELETE', pattern: /^\/api\/admin\/permissions$/, paramNames: [], handler: (db, body) => {
    const id = Number(body.id)
    db.permissions = db.permissions.filter(p => p.id !== id)
    saveDB(db)
    return { code: 200, msg: '删除成功' }
  }},
  { method: 'GET', pattern: /^\/api\/admin\/classes$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.classes }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/classes$/, paramNames: [], handler: (db, body) => {
    const cls: SchoolClass = { id: nextId(db.classes), name: body.name, grade: body.grade }
    db.classes.push(cls)
    saveDB(db)
    return { code: 200, msg: '创建成功', data: cls }
  }},
  { method: 'PUT', pattern: /^\/api\/admin\/classes\/([^/]+)$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.classes.findIndex(c => c.id === Number(params[0]))
    if (idx < 0) return { code: 404, msg: '班级不存在' }
    db.classes[idx] = { ...db.classes[idx], ...body }
    saveDB(db)
    return { code: 200, msg: '更新成功', data: db.classes[idx] }
  }},
  { method: 'DELETE', pattern: /^\/api\/admin\/classes$/, paramNames: [], handler: (db, body) => {
    const id = Number(body.id)
    db.classes = db.classes.filter(c => c.id !== id)
    saveDB(db)
    return { code: 200, msg: '删除成功' }
  }},
  { method: 'GET', pattern: /^\/api\/admin\/mall$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.products }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/mall$/, paramNames: [], handler: (db, body) => {
    const product: Product = { id: nextId(db.products), name: body.name, cost: body.cost, stock: body.stock, description: body.description, category: body.category }
    db.products.push(product)
    saveDB(db)
    return { code: 200, msg: '创建成功', data: product }
  }},
  { method: 'PUT', pattern: /^\/api\/admin\/mall\/([^/]+)$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.products.findIndex(p => p.id === Number(params[0]))
    if (idx < 0) return { code: 404, msg: '商品不存在' }
    db.products[idx] = { ...db.products[idx], ...body }
    saveDB(db)
    return { code: 200, msg: '更新成功', data: db.products[idx] }
  }},
  { method: 'DELETE', pattern: /^\/api\/admin\/mall$/, paramNames: [], handler: (db, body) => {
    const id = Number(body.id)
    db.products = db.products.filter(p => p.id !== id)
    saveDB(db)
    return { code: 200, msg: '删除成功' }
  }},
  { method: 'GET', pattern: /^\/api\/admin\/redemptions$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.redemptions }
  }},
  { method: 'GET', pattern: /^\/api\/admin\/dashboard$/, paramNames: [], handler: (db) => {
    return { code: 200, data: {
      total_users: db.users.length,
      total_students: db.users.filter(u => u.role_id === 3).length,
      total_teachers: db.users.filter(u => u.role_id === 2).length,
      total_classes: db.classes.length,
      total_products: db.products.length,
      total_redemptions: db.redemptions.length,
      total_points: db.points.reduce((s, r) => s + r.points, 0),
      recent_points: db.points.slice(-5).reverse(),
      recent_redemptions: db.redemptions.slice(-5).reverse(),
    }}
  }},
  { method: 'GET', pattern: /^\/api\/admin\/export$/, paramNames: [], handler: (db) => {
    // Mock：返回 CSV 格式文本（前端会触发下载）
    const csv = 'id,username,name,role_id,className,points\n' +
      db.users.map(u => `${u.id},${u.username},${u.name},${u.role_id},${u.className || ''},${u.points || 0}`).join('\n')
    return { code: 200, data: { csv, filename: 'users_export.csv' } }
  }},
  { method: 'POST', pattern: /^\/api\/admin\/import$/, paramNames: [], handler: (db, body) => {
    return { code: 200, msg: '导入成功', data: { imported: 3, skipped: 0 } }
  }},

  // ========== 教师 API ==========
  { method: 'GET', pattern: /^\/api\/teacher\/students$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    return { code: 200, data: studentsForTeacher(db, db.currentSession.user_id) }
  }},
  { method: 'POST', pattern: /^\/api\/teacher\/students$/, paramNames: [], handler: (db, body) => {
    const id = 'S' + String(nextId(db.users)).padStart(4, '0')
    const student: User = { id, username: body.username, name: body.name, role_id: 3, className: body.className, points: 0 }
    db.users.push(student)
    saveDB(db)
    return { code: 200, msg: '添加成功', data: student }
  }},
  { method: 'PUT', pattern: /^\/api\/teacher\/students\/([^/]+)$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.users.findIndex(u => u.id === params[0])
    if (idx < 0) return { code: 404, msg: '学生不存在' }
    db.users[idx] = { ...db.users[idx], ...body }
    saveDB(db)
    return { code: 200, msg: '更新成功', data: db.users[idx] }
  }},
  { method: 'DELETE', pattern: /^\/api\/teacher\/students$/, paramNames: [], handler: (db, body) => {
    db.users = db.users.filter(u => u.id !== body.id)
    saveDB(db)
    return { code: 200, msg: '删除成功' }
  }},
  { method: 'POST', pattern: /^\/api\/teacher\/students\/import$/, paramNames: [], handler: (db, body) => {
    return { code: 200, msg: '导入成功', data: { success: 5, failed: 0 } }
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/my-classes$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const teacher = db.users.find(u => u.id === db.currentSession!.user_id)
    const classes = teacher?.className ? db.classes.filter(c => c.name === teacher.className) : db.classes
    return { code: 200, data: classes }
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/points\/records$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const studentIds = studentsForTeacher(db, db.currentSession.user_id).map(s => s.id)
    return { code: 200, data: db.points.filter(p => studentIds.includes(p.student_id)) }
  }},
  { method: 'POST', pattern: /^\/api\/teacher\/points$/, paramNames: [], handler: (db, body) => {
    const record: PointsRecord = {
      id: nextId(db.points),
      student_id: body.student_id,
      points: body.points,
      reason: body.reason,
      created_at: new Date().toISOString().replace('T', ' ').slice(0, 19),
      teacher_name: db.users.find(u => u.id === db.currentSession?.user_id)?.name,
    }
    db.points.push(record)
    // 更新学生积分
    const student = db.users.find(u => u.id === body.student_id)
    if (student) {
      student.points = (student.points || 0) + body.points
    }
    saveDB(db)
    return { code: 200, msg: '积分记录已添加', data: record }
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/evaluation\/dimensions$/, paramNames: [], handler: () => {
    return { code: 200, data: [
      { id: 1, name: '德育' },
      { id: 2, name: '智育' },
      { id: 3, name: '体育' },
      { id: 4, name: '美育' },
      { id: 5, name: '劳育' },
    ]}
  }},
  { method: 'POST', pattern: /^\/api\/teacher\/evaluation$/, paramNames: [], handler: (db, body) => {
    const evalRecord: Evaluation = {
      id: nextId(db.evaluations),
      student_id: body.student_id,
      dimension_id: body.dimension_id,
      score: body.score,
      comment: body.comment,
      evaluator_id: db.currentSession?.user_id || '',
      evaluator_name: db.users.find(u => u.id === db.currentSession?.user_id)?.name || '',
      created_at: new Date().toISOString().replace('T', ' ').slice(0, 19),
    }
    db.evaluations.push(evalRecord)
    saveDB(db)
    return { code: 200, msg: '评价已提交', data: evalRecord }
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/dashboard$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const students = studentsForTeacher(db, db.currentSession.user_id)
    return { code: 200, data: {
      total_students: students.length,
      total_points: students.reduce((s, st) => s + (st.points || 0), 0),
      avg_points: students.length ? Math.round(students.reduce((s, st) => s + (st.points || 0), 0) / students.length) : 0,
      recent_points: db.points.slice(-5).reverse(),
    }}
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/statistics$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const students = studentsForTeacher(db, db.currentSession.user_id)
    return { code: 200, data: {
      students,
      points_distribution: [
        { range: '0-50', count: students.filter(s => (s.points || 0) < 50).length },
        { range: '50-80', count: students.filter(s => (s.points || 0) >= 50 && (s.points || 0) < 80).length },
        { range: '80-100', count: students.filter(s => (s.points || 0) >= 80 && (s.points || 0) < 100).length },
        { range: '100+', count: students.filter(s => (s.points || 0) >= 100).length },
      ],
      monthly_points: [
        { month: '2026-04', points: 120 },
        { month: '2026-05', points: 180 },
        { month: '2026-06', points: 150 },
        { month: '2026-07', points: 200 },
      ],
    }}
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/redemptions$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const studentIds = studentsForTeacher(db, db.currentSession.user_id).map(s => s.id)
    return { code: 200, data: db.redemptions.filter(r => studentIds.includes(r.student_id)) }
  }},
  { method: 'GET', pattern: /^\/api\/teacher\/parent-messages$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const studentIds = studentsForTeacher(db, db.currentSession.user_id).map(s => s.id)
    return { code: 200, data: db.messages.filter(m => studentIds.includes(m.student_id)) }
  }},
  { method: 'POST', pattern: /^\/api\/teacher\/parent-messages\/([^/]+)\/reply$/, paramNames: ['id'], handler: (db, body, params) => {
    const idx = db.messages.findIndex(m => m.id === Number(params[0]))
    if (idx < 0) return { code: 404, msg: '消息不存在' }
    db.messages[idx].reply = body.reply
    db.messages[idx].replied_at = new Date().toISOString().replace('T', ' ').slice(0, 19)
    db.messages[idx].is_read = 1
    saveDB(db)
    return { code: 200, msg: '回复成功', data: db.messages[idx] }
  }},
  { method: 'PUT', pattern: /^\/api\/teacher\/parent-messages\/([^/]+)\/read$/, paramNames: ['id'], handler: (db, _body, params) => {
    const idx = db.messages.findIndex(m => m.id === Number(params[0]))
    if (idx < 0) return { code: 404, msg: '消息不存在' }
    db.messages[idx].is_read = 1
    saveDB(db)
    return { code: 200, msg: '已标记为已读' }
  }},

  // ========== 学生 API ==========
  { method: 'GET', pattern: /^\/api\/student\/info$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const student = db.users.find(u => u.id === db.currentSession!.user_id && u.role_id === 3)
    if (!student) return { code: 404, msg: '学生不存在' }
    // 计算班级排名
    const classmates = db.users.filter(u => u.className === student.className && u.role_id === 3)
    const rank = classmates.filter(s => (s.points || 0) > (student.points || 0)).length + 1
    return { code: 200, data: { ...student, rank } }
  }},
  { method: 'GET', pattern: /^\/api\/student\/points\/records$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    return { code: 200, data: db.points.filter(p => p.student_id === db.currentSession!.user_id) }
  }},
  { method: 'GET', pattern: /^\/api\/student\/redemptions$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    return { code: 200, data: db.redemptions.filter(r => r.student_id === db.currentSession!.user_id) }
  }},
  { method: 'GET', pattern: /^\/api\/student\/mall$/, paramNames: [], handler: (db) => {
    return { code: 200, data: db.products }
  }},
  { method: 'GET', pattern: /^\/api\/student\/evaluation$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    return { code: 200, data: db.evaluations.filter(e => e.student_id === db.currentSession!.user_id) }
  }},

  // ========== 通用 API ==========
  { method: 'GET', pattern: /^\/api\/rank\/class$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const user = db.users.find(u => u.id === db.currentSession!.user_id)
    if (!user?.className) return { code: 200, data: [] }
    const classmates = db.users.filter(u => u.className === user.className && u.role_id === 3)
      .sort((a, b) => (b.points || 0) - (a.points || 0))
    return { code: 200, data: classmates.map((s, i) => ({ ...s, rank: i + 1 })) }
  }},
  { method: 'POST', pattern: /^\/api\/mall\/redeem$/, paramNames: [], handler: (db, body) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const product = db.products.find(p => p.id === body.item_id)
    if (!product) return { code: 404, msg: '商品不存在' }
    if (product.stock <= 0) return { code: 400, msg: '库存不足' }
    const student = db.users.find(u => u.id === db.currentSession!.user_id)
    if (!student || (student.points || 0) < product.cost) return { code: 400, msg: '积分不足' }
    product.stock--
    student.points = (student.points || 0) - product.cost
    const record: RedemptionRecord = {
      id: nextId(db.redemptions),
      student_id: student.id,
      student_name: student.name,
      item_id: product.id,
      item_name: product.name,
      cost: product.cost,
      created_at: new Date().toISOString().replace('T', ' ').slice(0, 19),
      status: '已完成',
    }
    db.redemptions.push(record)
    saveDB(db)
    return { code: 200, msg: '兑换成功', data: record }
  }},

  // ========== 家长 API ==========
  { method: 'GET', pattern: /^\/api\/parent\/children$/, paramNames: [], handler: (db) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const parent = db.users.find(u => u.id === db.currentSession!.user_id)
    if (!parent) return { code: 401, msg: '未登录' }
    // Mock：家长与同班学生关联
    const children = db.users.filter(u => u.role_id === 3 && u.className === parent.className)
    return { code: 200, data: children }
  }},
  { method: 'GET', pattern: /^\/api\/parent\/student\/([^/]+)\/info$/, paramNames: ['id'], handler: (db, _body, params) => {
    const student = db.users.find(u => u.id === params[0] && u.role_id === 3)
    if (!student) return { code: 404, msg: '学生不存在' }
    const classmates = db.users.filter(u => u.className === student.className && u.role_id === 3)
    const rank = classmates.filter(s => (s.points || 0) > (student.points || 0)).length + 1
    return { code: 200, data: { ...student, rank } }
  }},
  { method: 'GET', pattern: /^\/api\/parent\/student\/([^/]+)\/points$/, paramNames: ['id'], handler: (db, _body, params) => {
    return { code: 200, data: db.points.filter(p => p.student_id === params[0]) }
  }},
  { method: 'GET', pattern: /^\/api\/parent\/student\/([^/]+)\/evaluation$/, paramNames: ['id'], handler: (db, _body, params) => {
    return { code: 200, data: db.evaluations.filter(e => e.student_id === params[0]) }
  }},
  { method: 'GET', pattern: /^\/api\/parent\/student\/([^/]+)\/redemptions$/, paramNames: ['id'], handler: (db, _body, params) => {
    return { code: 200, data: db.redemptions.filter(r => r.student_id === params[0]) }
  }},
  { method: 'GET', pattern: /^\/api\/parent\/student\/([^/]+)\/messages$/, paramNames: ['id'], handler: (db, _body, params) => {
    return { code: 200, data: db.messages.filter(m => m.student_id === params[0]) }
  }},
  { method: 'POST', pattern: /^\/api\/parent\/student\/([^/]+)\/messages$/, paramNames: ['id'], handler: (db, body, params) => {
    if (!db.currentSession) return { code: 401, msg: '未登录' }
    const parent = db.users.find(u => u.id === db.currentSession!.user_id)
    const msg: ParentMessage = {
      id: nextId(db.messages),
      student_id: params[0],
      parent_id: parent?.id || '',
      parent_name: parent?.name || '',
      content: body.content,
      is_read: 0,
      created_at: new Date().toISOString().replace('T', ' ').slice(0, 19),
    }
    db.messages.push(msg)
    saveDB(db)
    return { code: 200, msg: '发送成功', data: msg }
  }},
]

// ===================== 导出 Mock 请求函数 =====================
export function isMockEnabled(): boolean {
  return import.meta.env.VITE_USE_MOCK === 'true'
}

export async function mockRequest<T = any>(
  method: string,
  url: string,
  data?: any
): Promise<ApiResponse<T>> {
  // 模拟网络延迟，让 Demo 更真实
  await new Promise(resolve => setTimeout(resolve, 200 + Math.random() * 300))

  const db = loadDB()
  const matched = matchRoute(method.toUpperCase(), url.split('?')[0])

  if (!matched) {
    console.warn(`[Mock] 未匹配的 API: ${method} ${url}`)
    return { code: 404, msg: `[Mock] 接口未实现: ${method} ${url}` }
  }

  try {
    const result = matched.route.handler(db, data, matched.params)
    if (import.meta.env.DEV) {
      console.log(`[Mock] ${method} ${url}`, { data, response: result })
    }
    return result as ApiResponse<T>
  } catch (e) {
    console.error(`[Mock] 处理出错: ${method} ${url}`, e)
    return { code: 500, msg: 'Mock 处理异常: ' + (e as Error).message }
  }
}

/** 重置 Mock 数据（开发调试用） */
export function resetMockDB() {
  localStorage.removeItem(STORAGE_KEY)
  loadDB()
}
