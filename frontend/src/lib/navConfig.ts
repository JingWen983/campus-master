/**
 * 各角色侧边栏导航配置 —— config-driven Sidebar
 *
 * 原始来源：student.html / admin.html / teacher.html / parent.html 的 nav button 列表
 * 统一抽到这里，Sidebar.vue 根据 navItems prop 渲染。
 */
export interface NavItem {
  /** tab 唯一标识，对应 v-show 判断值 */
  key: string
  /** 显示文字 */
  label: string
  /** FontAwesome 图标 class，如 'fa-solid fa-gauge' */
  icon: string
  /** 可选：未读数等徽章（teacher 家长留言用） */
  badge?: number
}

export interface NavSection {
  /** 小节标题（如 "功能导航"），不传则无标题 */
  title?: string
  items: NavItem[]
}

/** 管理员导航（admin.html L345-365） */
export const ADMIN_NAV: NavItem[] = [
  { key: 'dashboard', label: '系统概览', icon: 'fa-solid fa-gauge' },
  { key: 'users', label: '用户管理', icon: 'fa-solid fa-users' },
  { key: 'classes', label: '班级管理', icon: 'fa-solid fa-school' },
  { key: 'roles', label: '角色管理', icon: 'fa-solid fa-user-gear' },
  { key: 'permissions', label: '权限管理', icon: 'fa-solid fa-lock' },
  { key: 'system', label: '系统配置', icon: 'fa-solid fa-gear' },
  { key: 'mall', label: '商城管理', icon: 'fa-solid fa-shopping-cart' },
]

/** 教师导航（teacher.html L293-340） */
export const TEACHER_NAV: NavItem[] = [
  { key: 'dashboard', label: '工作台', icon: 'fa-solid fa-gauge' },
  { key: 'students', label: '学生管理', icon: 'fa-solid fa-users' },
  { key: 'points', label: '积分管理', icon: 'fa-solid fa-coins' },
  { key: 'evaluation', label: '学生评价', icon: 'fa-solid fa-star' },
  { key: 'statistics', label: '数据统计', icon: 'fa-solid fa-chart-column' },
  { key: 'parent-messages', label: '家长留言', icon: 'fa-solid fa-comment-dots', badge: 0 },
  { key: 'redemptions', label: '兑换记录', icon: 'fa-solid fa-receipt' },
]

/** 学生导航（student.html L347-372） */
export const STUDENT_NAV: NavItem[] = [
  { key: 'home', label: '能量大厅', icon: 'fa-solid fa-bolt' },
  { key: 'mall', label: '兑换商城', icon: 'fa-solid fa-store' },
  { key: 'evaluation', label: '个人评价', icon: 'fa-solid fa-star' },
  { key: 'redemptions', label: '兑换记录', icon: 'fa-solid fa-receipt' },
  { key: 'rank', label: '风采榜', icon: 'fa-solid fa-ranking-star' },
  { key: 'profile', label: '个人档案', icon: 'fa-solid fa-chart-pie' },
]

/** 家长导航（parent.html 功能导航分区） */
export const PARENT_NAV: NavItem[] = [
  { key: 'points', label: '积分记录', icon: 'fa-solid fa-coins' },
  { key: 'evaluation', label: '教师评价', icon: 'fa-solid fa-star' },
  { key: 'redemptions', label: '兑换记录', icon: 'fa-solid fa-receipt' },
  { key: 'messages', label: '家校留言', icon: 'fa-solid fa-comments' },
]
