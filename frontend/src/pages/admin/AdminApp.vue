<script setup lang="ts">
/**
 * 管理员控制台 —— 迁移自 admin.html
 *
 * 单 SFC + v-show 切换 7 个 tab：系统概览 / 用户管理 / 班级管理 /
 * 角色管理 / 权限管理 / 系统配置 / 商城管理
 * 12 个 BaseModal：添加用户 / 批量导入 / 编辑用户 / 添加角色 / 添加权限 /
 * 编辑角色 / 编辑权限 / 重置密码 / 添加班级 / 编辑班级 / 添加商品 / 编辑商品
 * 1 个 BaseChart（仪表盘）：用户角色分布饼图
 * 批量导入学生：xlsx 解析 + pinyin-pro 生成账号密码
 * API 走统一 api 封装，alert/confirm → toast/confirmDialog
 */
import { ref, computed, watch, onMounted, nextTick } from 'vue'
import type { EChartsOption } from 'echarts'
import * as XLSX from 'xlsx'
import { pinyin } from 'pinyin-pro'
import { api } from '../../lib/api'
import { checkAuth, logout, type UserInfo } from '../../lib/auth'
import { formatDateTime } from '../../lib/format'
import { THEMES } from '../../lib/theme'
import { ADMIN_NAV } from '../../lib/navConfig'
import { toast } from '../../composables/useToast'
import { confirmDialog } from '../../composables/useConfirm'
import AppLayout from '../../components/AppLayout.vue'
import BaseChart from '../../components/BaseChart.vue'
import BaseModal from '../../components/BaseModal.vue'
import EmptyState from '../../components/EmptyState.vue'
import Pagination from '../../components/Pagination.vue'
import StatCard from '../../components/StatCard.vue'
import RoleBadge from '../../components/RoleBadge.vue'

// ===== Types =====
interface User {
  id: number | string
  username: string
  name: string
  role_id: number
  status?: string
  className?: string
  bound_class_ids?: (number | string)[]
  bound_student_ids?: (number | string)[]
}

interface Permission {
  id: number
  name: string
  code?: string
  description?: string
}

interface Role {
  id: number
  name: string
  description: string
  permissions: Permission[]
}

interface SchoolClass {
  id: number | string
  name: string
  grade?: string
  grade_code?: string
  class_code?: string
  head_teacher?: string
  description?: string
  student_count?: number
}

interface Product {
  id: number | string
  name: string
  description: string
  cost: number
  stock: number
  status: number
}

interface ExchangeRecord {
  id: number | string
  username: string
  product_name: string
  cost: number
  created_at: string
}

interface DashboardStats {
  totalUsers: number
  studentCount: number
  teacherCount: number
  todayLogin: number
  recentActivities?: { id: number | string; description: string; time: string }[]
}

interface BatchImportRow {
  row: number
  name: string
  className: string
  matched: boolean
}

interface BatchImportSuccessItem {
  row: number
  name: string
  username: string
  password: string
  student_id: string | number
}

interface BatchImportFailedItem {
  row: number
  name: string
  reason: string
}

interface BatchImportResult {
  success: number
  failed: number
  successList: BatchImportSuccessItem[]
  failedList: BatchImportFailedItem[]
}

interface ImportResult {
  success: boolean
  message: string
  imported?: number
  skipped?: number
}

interface NewUserForm {
  username: string
  name: string
  role_id: number
  className: string
  password: string
  bound_class_ids: (number | string)[]
  bound_student_ids: (number | string)[]
}

interface EditUserForm {
  id: number | string
  username: string
  name: string
  role_id: number
  className: string
  bound_class_ids: (number | string)[]
  bound_student_ids: (number | string)[]
}

// ===== State =====
const currentTab = ref<string>('dashboard')
const userInfo = ref<UserInfo>({
  id: '',
  username: '',
  name: '',
  role_id: 1,
})

// Dialog visibility
const showAddUserDialog = ref(false)
const showBatchImportDialog = ref(false)
const showEditUserDialog = ref(false)
const showAddRoleDialog = ref(false)
const showAddPermissionDialog = ref(false)
const showEditRoleDialog = ref(false)
const showEditPermissionDialog = ref(false)
const showResetPasswordDialog = ref(false)
const showAddClassDialog = ref(false)
const showEditClassDialog = ref(false)
const showAddProductDialog = ref(false)
const showEditProductDialog = ref(false)

// Users
const userSearch = ref('')
const userRoleFilter = ref('')
const currentPage = ref(1)
const pageSize = 10
const users = ref<User[]>([])
const newUser = ref<NewUserForm>({
  username: '',
  name: '',
  role_id: 3,
  className: '',
  password: '',
  bound_class_ids: [],
  bound_student_ids: [],
})
const editUser = ref<EditUserForm>({
  id: 0,
  username: '',
  name: '',
  role_id: 3,
  className: '',
  bound_class_ids: [],
  bound_student_ids: [],
})

// Dashboard
const dashboardStats = ref<DashboardStats>({
  totalUsers: 0,
  studentCount: 0,
  teacherCount: 0,
  todayLogin: 0,
})
const recentActivities = ref<{ id: number | string; description: string; time: string }[]>([])
const dashboardChartRef = ref<InstanceType<typeof BaseChart>>()

// Roles & permissions
const roles = ref<Role[]>([])
const permissions = ref<Permission[]>([])
const newRole = ref<{ name: string; description: string; permissions: number[] }>({
  name: '',
  description: '',
  permissions: [],
})
const newPermission = ref<{ name: string; code: string; description: string }>({
  name: '',
  code: '',
  description: '',
})
const editRoleData = ref<{ id: number | string; name: string; description: string }>({
  id: 0,
  name: '',
  description: '',
})
const editPermissionData = ref<{ id: number | string; name: string; code: string; description: string }>({
  id: 0,
  name: '',
  code: '',
  description: '',
})

// Reset password
const resetPasswordUser = ref<Partial<User>>({})
const resetPasswordMode = ref<'auto' | 'manual'>('auto')
const manualPassword = ref('')
const resetPasswordResult = ref('')

// System config
const systemConfig = ref<{ systemName: string; version: string; description: string }>({
  systemName: '校园文明能量站',
  version: '1.0.0',
  description: '校园文明能量站管理系统',
})
const securityConfig = ref<{ loginTimeout: number; minPasswordLength: number; enableCaptcha: boolean }>({
  loginTimeout: 30,
  minPasswordLength: 6,
  enableCaptcha: false,
})
const backupConfig = ref<{ frequency: string; retentionDays: number }>({
  frequency: 'daily',
  retentionDays: 7,
})
const exportLoading = ref(false)
const importLoading = ref(false)
const importMode = ref<'skip' | 'overwrite'>('skip')
const selectedFile = ref<File | null>(null)
const selectedFileName = ref('')
const importResult = ref<ImportResult | null>(null)

// Mall
const products = ref<Product[]>([])
const exchangeRecords = ref<ExchangeRecord[]>([])
const currentMallPage = ref(1)
const mallPageSize = 10
const newProduct = ref<{ name: string; description: string; cost: number; stock: number; status: number }>({
  name: '',
  description: '',
  cost: 0,
  stock: 0,
  status: 1,
})
const editProduct = ref<Product>({
  id: 0,
  name: '',
  description: '',
  cost: 0,
  stock: 0,
  status: 1,
})

// Classes
const classes = ref<SchoolClass[]>([])
const newClass = ref<{ grade: string; classNo: string; head_teacher: string; description: string }>({
  grade: '',
  classNo: '',
  head_teacher: '',
  description: '',
})
const editClassData = ref<{ id: number | string; grade: string; classNo: string; head_teacher: string; description: string }>({
  id: 0,
  grade: '',
  classNo: '',
  head_teacher: '',
  description: '',
})

// Batch import
const batchImportStep = ref<'select' | 'preview' | 'result'>('select')
const batchImportRows = ref<BatchImportRow[]>([])
const batchImportResult = ref<BatchImportResult | null>(null)
const batchImportLoading = ref(false)

// ===== Computed =====
const filteredUsersList = computed(() =>
  users.value.filter(user => {
    const kw = userSearch.value.toLowerCase()
    const matchesSearch =
      user.username.toLowerCase().includes(kw) || user.name.toLowerCase().includes(kw)
    const matchesRole = !userRoleFilter.value || Number(user.role_id) === Number(userRoleFilter.value)
    return matchesSearch && matchesRole
  })
)

const pagedUsers = computed(() => {
  const start = (currentPage.value - 1) * pageSize
  return filteredUsersList.value.slice(start, start + pageSize)
})

const userTotalPages = computed(() => Math.ceil(filteredUsersList.value.length / pageSize) || 1)
const userStartIndex = computed(() =>
  filteredUsersList.value.length === 0 ? 0 : (currentPage.value - 1) * pageSize + 1
)
const userEndIndex = computed(() =>
  Math.min(currentPage.value * pageSize, filteredUsersList.value.length)
)

const studentsList = computed(() => users.value.filter(u => Number(u.role_id) === 3))

const pagedProducts = computed(() => {
  const start = (currentMallPage.value - 1) * mallPageSize
  return products.value.slice(start, start + mallPageSize)
})
const mallTotalPages = computed(() => Math.ceil(products.value.length / mallPageSize) || 1)
const mallStartIndex = computed(() =>
  products.value.length === 0 ? 0 : (currentMallPage.value - 1) * mallPageSize + 1
)
const mallEndIndex = computed(() => Math.min(currentMallPage.value * mallPageSize, products.value.length))

// 用户角色分布饼图
const roleDistributionOption = computed<EChartsOption>(() => {
  const counts: Record<number, number> = { 1: 0, 2: 0, 3: 0, 4: 0 }
  users.value.forEach(u => {
    counts[Number(u.role_id)] = (counts[Number(u.role_id)] || 0) + 1
  })
  const data = [
    { name: '管理员', value: counts[1] },
    { name: '教师', value: counts[2] },
    { name: '学生', value: counts[3] },
    { name: '家长', value: counts[4] },
  ].filter(d => d.value > 0)
  return {
    tooltip: { trigger: 'item', formatter: '{b}: {c} ({d}%)' },
    legend: { top: '5%', left: 'center' },
    color: ['#e11d48', '#f59e0b', '#10b981', '#8b5cf6'],
    series: [{
      name: '角色分布',
      type: 'pie',
      radius: ['40%', '70%'],
      avoidLabelOverlap: false,
      itemStyle: { borderRadius: 10, borderColor: '#fff', borderWidth: 2 },
      label: { show: false, position: 'center' },
      emphasis: { label: { show: true, fontSize: '18', fontWeight: 'bold' } },
      labelLine: { show: false },
      data,
    }],
  }
})

// ===== Watchers =====
watch(userSearch, () => { currentPage.value = 1 })
watch(userRoleFilter, () => { currentPage.value = 1 })

// ===== API Methods =====
async function loadUsers() {
  try {
    const res = await api.get<User[]>('/api/admin/users')
    if (res.code === 200) users.value = res.data || []
  } catch (e) {
    console.error('Load users error:', e)
  }
}

async function loadRoles() {
  try {
    const res = await api.get<Role[]>('/api/admin/roles')
    if (res.code === 200) {
      roles.value = (res.data || []).map(r => ({ ...r, permissions: r.permissions || [] }))
    }
  } catch (e) {
    console.error('Load roles error:', e)
  }
}

async function loadPermissions() {
  try {
    const res = await api.get<Permission[]>('/api/admin/permissions')
    if (res.code === 200) permissions.value = res.data || []
  } catch (e) {
    console.error('Load permissions error:', e)
  }
}

async function loadDashboard() {
  try {
    const res = await api.get<DashboardStats>('/api/admin/dashboard')
    if (res.code === 200 && res.data) {
      dashboardStats.value = {
        totalUsers: res.data.totalUsers,
        studentCount: res.data.studentCount,
        teacherCount: res.data.teacherCount,
        todayLogin: res.data.todayLogin,
      }
      recentActivities.value = res.data.recentActivities || []
    }
  } catch (e) {
    console.error('Load dashboard error:', e)
  }
}

async function loadClasses() {
  try {
    const res = await api.get<SchoolClass[]>('/api/admin/classes')
    if (res.code === 200) classes.value = res.data || []
  } catch (e) {
    console.error('Load classes error:', e)
  }
}

async function loadProducts() {
  try {
    const res = await api.get<Product[]>('/api/admin/mall')
    if (res.code === 200) products.value = res.data || []
  } catch (e) {
    console.error('Load products error:', e)
  }
}

async function loadExchangeRecords() {
  try {
    const res = await api.get<ExchangeRecord[]>('/api/admin/redemptions')
    if (res.code === 200) exchangeRecords.value = res.data || []
  } catch (e) {
    console.error('Load exchange records error:', e)
  }
}

// ===== User actions =====
async function openAddUser() {
  if (classes.value.length === 0) await loadClasses()
  showAddUserDialog.value = true
}

async function openEditUser(user: User) {
  if (classes.value.length === 0) await loadClasses()
  editUser.value = {
    id: user.id,
    username: user.username,
    name: user.name,
    role_id: Number(user.role_id),
    className: user.className || '',
    bound_class_ids: Array.isArray(user.bound_class_ids) ? [...user.bound_class_ids] : [],
    bound_student_ids: Array.isArray(user.bound_student_ids) ? [...user.bound_student_ids] : [],
  }
  showEditUserDialog.value = true
}

async function addUser() {
  if (!newUser.value.username || !newUser.value.name || !newUser.value.password) {
    toast.warning('请填写完整信息')
    return
  }
  try {
    const payload: Record<string, any> = {
      username: newUser.value.username,
      name: newUser.value.name,
      role_id: Number(newUser.value.role_id),
      className: Number(newUser.value.role_id) === 3 ? newUser.value.className : '',
      password: newUser.value.password,
    }
    if (Number(newUser.value.role_id) === 2) payload.bound_class_ids = newUser.value.bound_class_ids
    if (Number(newUser.value.role_id) === 4) payload.bound_student_ids = newUser.value.bound_student_ids
    const res = await api.post('/api/admin/users', payload)
    if (res.code === 200) {
      await loadUsers()
      showAddUserDialog.value = false
      newUser.value = {
        username: '', name: '', role_id: 3, className: '', password: '',
        bound_class_ids: [], bound_student_ids: [],
      }
      toast.success('用户添加成功！')
    } else {
      toast.error(res.msg || '添加失败')
    }
  } catch (e) {
    console.error('Add user error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function updateUser() {
  if (!editUser.value.username || !editUser.value.name) {
    toast.warning('用户名和姓名不能为空')
    return
  }
  try {
    const payload: Record<string, any> = {
      username: editUser.value.username,
      name: editUser.value.name,
      role_id: Number(editUser.value.role_id),
      className: Number(editUser.value.role_id) === 3 ? editUser.value.className : '',
    }
    if (Number(editUser.value.role_id) === 2) payload.bound_class_ids = editUser.value.bound_class_ids
    if (Number(editUser.value.role_id) === 4) payload.bound_student_ids = editUser.value.bound_student_ids
    const res = await api.put(`/api/admin/users/${editUser.value.id}`, payload)
    if (res.code === 200) {
      await loadUsers()
      showEditUserDialog.value = false
      toast.success('用户编辑成功！')
    } else {
      toast.error(res.msg || '编辑失败')
    }
  } catch (e) {
    console.error('Update user error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function deleteUser(user: User) {
  if (Number(user.role_id) === 1) {
    toast.warning('不能删除管理员用户')
    return
  }
  const ok = await confirmDialog({
    message: `确定要删除用户 ${user.name} 吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    const res = await api.delete('/api/admin/users', { id: user.id })
    if (res.code === 200) {
      await loadUsers()
      toast.success('用户删除成功！')
    } else {
      toast.error(res.msg || '删除失败')
    }
  } catch (e) {
    console.error('Delete user error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function resetPassword(user: User) {
  resetPasswordUser.value = user
  resetPasswordMode.value = 'auto'
  manualPassword.value = ''
  resetPasswordResult.value = ''
  showResetPasswordDialog.value = true
}

async function confirmResetPassword() {
  if (resetPasswordMode.value === 'manual') {
    if (!manualPassword.value || manualPassword.value.length < 6) {
      toast.warning('请输入至少6位的密码')
      return
    }
  }
  try {
    const res = await api.post<{ new_password: string }>('/api/admin/users/reset-password', {
      user_id: resetPasswordUser.value.id,
      auto_generate: resetPasswordMode.value === 'auto',
      new_password: resetPasswordMode.value === 'manual' ? manualPassword.value : '',
    })
    if (res.code === 200 && res.data) {
      resetPasswordResult.value = res.data.new_password
    } else {
      toast.error(res.msg || '重置失败')
    }
  } catch (e) {
    console.error('Reset password error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function copyPassword() {
  const text = resetPasswordResult.value
  navigator.clipboard.writeText(text).then(() => {
    toast.success('密码已复制到剪贴板！')
  }).catch(() => {
    const textArea = document.createElement('textarea')
    textArea.value = text
    document.body.appendChild(textArea)
    textArea.select()
    document.execCommand('copy')
    document.body.removeChild(textArea)
    toast.success('密码已复制到剪贴板！')
  })
}

function closeResetPasswordDialog() {
  showResetPasswordDialog.value = false
  resetPasswordUser.value = {}
  resetPasswordMode.value = 'auto'
  manualPassword.value = ''
  resetPasswordResult.value = ''
}

function searchUsers() {
  currentPage.value = 1
}

function changeUserPage(page: number) {
  currentPage.value = page
}

// ===== Batch import (xlsx + pinyin-pro) =====
async function openBatchImport() {
  if (classes.value.length === 0) await loadClasses()
  showBatchImportDialog.value = true
  batchImportStep.value = 'select'
  batchImportRows.value = []
  batchImportResult.value = null
}

function downloadStudentTemplate() {
  const aoa = [
    ['姓名', '班级名称'],
    ['张三', '高二(1)班'],
    ['李四', '高二(1)班'],
  ]
  const ws = XLSX.utils.aoa_to_sheet(aoa)
  ws['!cols'] = [{ wch: 15 }, { wch: 20 }]
  const wb = XLSX.utils.book_new()
  XLSX.utils.book_append_sheet(wb, ws, '学生导入')
  XLSX.writeFile(wb, '学生批量导入模板.xlsx')
}

function handleBatchFileSelect(event: Event) {
  const input = event.target as HTMLInputElement
  const file = input.files && input.files[0]
  if (!file) return
  const reader = new FileReader()
  reader.onload = (e) => {
    try {
      const buf = e.target?.result
      const data = new Uint8Array(buf as ArrayBuffer)
      const wb = XLSX.read(data, { type: 'array' })
      const sheet = wb.Sheets[wb.SheetNames[0]]
      const rows = XLSX.utils.sheet_to_json(sheet, { header: 1 }) as unknown[][]
      const parsed: BatchImportRow[] = []
      for (let i = 1; i < rows.length; i++) {
        const row = rows[i]
        const name = (row[0] || '').toString().trim()
        const className = (row[1] || '').toString().trim()
        if (!name && !className) continue
        const matched = classes.value.some(c => c.name === className)
        parsed.push({ row: i + 1, name, className, matched })
      }
      batchImportRows.value = parsed
      batchImportStep.value = 'preview'
    } catch (err) {
      toast.error('文件解析失败：' + (err as Error).message)
    }
  }
  reader.readAsArrayBuffer(file)
  input.value = ''
}

// 生成账号：姓名拼音首字母 + class_code + 序号
function genStudentUsername(name: string, classCode: string, seq: number): string {
  const full = pinyin(name, { type: 'array' })
  const initials = full.map(p => {
    const noTone = p.normalize('NFD').replace(/[\u0300-\u036f]/g, '')
    return noTone.charAt(0).toLowerCase()
  }).join('')
  const seqStr = String(seq).padStart(2, '0')
  return initials + classCode + seqStr
}

function genStudentPassword(classCode: string, seq: number): string {
  const seqStr = String(seq).padStart(2, '0')
  return 'Stu@' + classCode + seqStr
}

async function submitBatchImport() {
  batchImportLoading.value = true
  try {
    const classSeqMap: Record<string, number> = {}
    const students: { name: string; className: string; username: string; password: string }[] = []
    const preFailed: BatchImportFailedItem[] = []
    for (const row of batchImportRows.value) {
      if (!row.matched) {
        preFailed.push({ row: row.row, name: row.name, reason: '班级不存在：' + row.className })
        continue
      }
      const cls = classes.value.find(c => c.name === row.className)
      const classCode = cls?.class_code || '00'
      if (!classSeqMap[row.className]) classSeqMap[row.className] = 1
      const seq = classSeqMap[row.className]++
      students.push({
        name: row.name,
        className: row.className,
        username: genStudentUsername(row.name, classCode, seq),
        password: genStudentPassword(classCode, seq),
      })
    }
    const res = await api.post<{ success: number; failed: number; records: { success_list: BatchImportSuccessItem[]; failed_list: BatchImportFailedItem[] } }>(
      '/api/admin/students/batch-import',
      { students }
    )
    if (res.code === 200 && res.data) {
      const backendFailed = res.data.records.failed_list || []
      const allFailed = [...backendFailed, ...preFailed]
      batchImportResult.value = {
        success: res.data.success,
        failed: res.data.failed + preFailed.length,
        successList: res.data.records.success_list,
        failedList: allFailed,
      }
      batchImportStep.value = 'result'
      await loadUsers()
    } else {
      toast.error(res.msg || '导入失败')
    }
  } catch (err) {
    toast.error('导入出错：' + (err as Error).message)
  } finally {
    batchImportLoading.value = false
  }
}

function copyBatchAccounts() {
  if (!batchImportResult.value) return
  const text = batchImportResult.value.successList
    .map(r => `${r.name}\t${r.username}\t${r.password}`).join('\n')
  navigator.clipboard.writeText(`姓名\t用户名\t密码\n${text}`).then(() => {
    toast.success('已复制！可粘贴到 Excel 下发给学生')
  })
}

function resetBatchImport() {
  batchImportStep.value = 'select'
  batchImportRows.value = []
  batchImportResult.value = null
}

// ===== Role actions =====
async function addRole() {
  if (!newRole.value.name || !newRole.value.description) {
    toast.warning('请填写完整信息')
    return
  }
  try {
    const res = await api.post('/api/admin/roles', {
      name: newRole.value.name,
      description: newRole.value.description,
    })
    if (res.code === 200) {
      await loadRoles()
      showAddRoleDialog.value = false
      newRole.value = { name: '', description: '', permissions: [] }
      toast.success('角色添加成功！')
    } else {
      toast.error(res.msg || '添加失败')
    }
  } catch (e) {
    console.error('Add role error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function editRole(role: Role) {
  editRoleData.value = { id: role.id, name: role.name, description: role.description }
  showEditRoleDialog.value = true
}

async function updateRole() {
  if (!editRoleData.value.name || !editRoleData.value.description) {
    toast.warning('角色名称和描述不能为空')
    return
  }
  try {
    const res = await api.put(`/api/admin/roles/${editRoleData.value.id}`, {
      name: editRoleData.value.name,
      description: editRoleData.value.description,
    })
    if (res.code === 200) {
      await loadRoles()
      showEditRoleDialog.value = false
      toast.success('角色编辑成功！')
    } else {
      toast.error(res.msg || '编辑失败')
    }
  } catch (e) {
    console.error('Update role error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function deleteRole(role: Role) {
  if (role.id <= 3) {
    toast.warning('不能删除系统内置角色')
    return
  }
  const ok = await confirmDialog({
    message: `确定要删除角色 ${role.name} 吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    const res = await api.delete(`/api/admin/roles?id=${role.id}`)
    if (res.code === 200) {
      await loadRoles()
      toast.success('角色删除成功！')
    } else {
      toast.error(res.msg || '删除失败')
    }
  } catch (e) {
    console.error('Delete role error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

// ===== Permission actions =====
async function addPermission() {
  if (!newPermission.value.name || !newPermission.value.code || !newPermission.value.description) {
    toast.warning('请填写完整信息')
    return
  }
  try {
    const res = await api.post('/api/admin/permissions', {
      name: newPermission.value.name,
      code: newPermission.value.code,
      description: newPermission.value.description,
    })
    if (res.code === 200) {
      await loadPermissions()
      showAddPermissionDialog.value = false
      newPermission.value = { name: '', code: '', description: '' }
      toast.success('权限添加成功！')
    } else {
      toast.error(res.msg || '添加失败')
    }
  } catch (e) {
    console.error('Add permission error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function editPermission(permission: Permission) {
  editPermissionData.value = {
    id: permission.id,
    name: permission.name,
    code: permission.code || '',
    description: permission.description || '',
  }
  showEditPermissionDialog.value = true
}

async function updatePermission() {
  if (!editPermissionData.value.name || !editPermissionData.value.code || !editPermissionData.value.description) {
    toast.warning('权限名称、代码和描述不能为空')
    return
  }
  try {
    const res = await api.put(`/api/admin/permissions/${editPermissionData.value.id}`, {
      name: editPermissionData.value.name,
      code: editPermissionData.value.code,
      description: editPermissionData.value.description,
    })
    if (res.code === 200) {
      await loadPermissions()
      showEditPermissionDialog.value = false
      toast.success('权限编辑成功！')
    } else {
      toast.error(res.msg || '编辑失败')
    }
  } catch (e) {
    console.error('Update permission error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function deletePermission(permission: Permission) {
  if (permission.id <= 7) {
    toast.warning('不能删除系统内置权限')
    return
  }
  const ok = await confirmDialog({
    message: `确定要删除权限 ${permission.name} 吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    const res = await api.delete(`/api/admin/permissions?id=${permission.id}`)
    if (res.code === 200) {
      await loadPermissions()
      toast.success('权限删除成功！')
    } else {
      toast.error(res.msg || '删除失败')
    }
  } catch (e) {
    console.error('Delete permission error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

// ===== Class actions =====
function gradeToCode(grade: string): string {
  const map: Record<string, string> = {
    '高一': '01', '高二': '02', '高三': '03',
    '初一': '07', '初二': '08', '初三': '09',
  }
  return map[grade] || ''
}

function buildClassName(grade: string, classNo: string): string {
  if (!grade || !classNo) return ''
  return `${grade}(${classNo})班`
}

function classNoToCode(classNo: string): string {
  const n = parseInt(classNo, 10)
  return isNaN(n) ? '' : String(n).padStart(2, '0')
}

async function addClass() {
  if (!newClass.value.grade || !newClass.value.classNo) {
    toast.warning('请选择年级并填写班号')
    return
  }
  try {
    const res = await api.post('/api/admin/classes', {
      name: buildClassName(newClass.value.grade, newClass.value.classNo),
      grade: newClass.value.grade,
      grade_code: gradeToCode(newClass.value.grade),
      class_code: classNoToCode(newClass.value.classNo),
      head_teacher: newClass.value.head_teacher,
      description: newClass.value.description,
    })
    if (res.code === 200) {
      await loadClasses()
      showAddClassDialog.value = false
      newClass.value = { grade: '', classNo: '', head_teacher: '', description: '' }
      toast.success('班级添加成功！')
    } else {
      toast.error(res.msg || '添加失败')
    }
  } catch (e) {
    console.error('Add class error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function openEditClass(cls: SchoolClass) {
  editClassData.value = {
    id: cls.id,
    grade: cls.grade || '',
    classNo: cls.class_code ? String(parseInt(cls.class_code, 10)) : '',
    head_teacher: cls.head_teacher || '',
    description: cls.description || '',
  }
  showEditClassDialog.value = true
}

async function updateClass() {
  if (!editClassData.value.grade || !editClassData.value.classNo) {
    toast.warning('请选择年级并填写班号')
    return
  }
  try {
    const res = await api.put(`/api/admin/classes/${editClassData.value.id}`, {
      name: buildClassName(editClassData.value.grade, editClassData.value.classNo),
      grade: editClassData.value.grade,
      grade_code: gradeToCode(editClassData.value.grade),
      class_code: classNoToCode(editClassData.value.classNo),
      head_teacher: editClassData.value.head_teacher,
      description: editClassData.value.description,
    })
    if (res.code === 200) {
      await loadClasses()
      showEditClassDialog.value = false
      toast.success('班级编辑成功！')
    } else {
      toast.error(res.msg || '编辑失败')
    }
  } catch (e) {
    console.error('Update class error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function deleteClass(cls: SchoolClass) {
  const ok = await confirmDialog({
    message: `确定要删除班级 ${cls.name} 吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    const res = await api.delete(`/api/admin/classes/${cls.id}`)
    if (res.code === 200) {
      await loadClasses()
      toast.success('班级删除成功！')
    } else {
      toast.error(res.msg || '删除失败')
    }
  } catch (e) {
    console.error('Delete class error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

// ===== Product actions =====
async function addProduct() {
  if (!newProduct.value.name || !newProduct.value.description || newProduct.value.cost <= 0 || newProduct.value.stock < 0) {
    toast.warning('请填写完整且有效的商品信息')
    return
  }
  try {
    const res = await api.post('/api/admin/mall', {
      name: newProduct.value.name,
      description: newProduct.value.description,
      cost: parseInt(String(newProduct.value.cost)),
      stock: parseInt(String(newProduct.value.stock)),
      status: parseInt(String(newProduct.value.status)),
    })
    if (res.code === 200) {
      await loadProducts()
      showAddProductDialog.value = false
      newProduct.value = { name: '', description: '', cost: 0, stock: 0, status: 1 }
      toast.success('商品添加成功！')
    } else {
      toast.error(res.msg || '添加失败')
    }
  } catch (e) {
    console.error('Add product error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function openEditProductDialog(product: Product) {
  editProduct.value = { ...product }
  showEditProductDialog.value = true
}

async function updateProduct() {
  if (!editProduct.value.name || !editProduct.value.description || editProduct.value.cost <= 0 || editProduct.value.stock < 0) {
    toast.warning('请填写完整且有效的商品信息')
    return
  }
  try {
    const res = await api.put(`/api/admin/mall/${editProduct.value.id}`, {
      name: editProduct.value.name,
      description: editProduct.value.description,
      cost: parseInt(String(editProduct.value.cost)),
      stock: parseInt(String(editProduct.value.stock)),
      status: parseInt(String(editProduct.value.status)),
    })
    if (res.code === 200) {
      await loadProducts()
      showEditProductDialog.value = false
      toast.success('商品更新成功！')
    } else {
      toast.error(res.msg || '更新失败')
    }
  } catch (e) {
    console.error('Update product error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function deleteProduct(product: Product) {
  const ok = await confirmDialog({
    message: `确定要删除商品 ${product.name} 吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    const res = await api.delete(`/api/admin/mall/${product.id}`)
    if (res.code === 200) {
      await loadProducts()
      toast.success('商品删除成功！')
    } else {
      toast.error(res.msg || '删除失败')
    }
  } catch (e) {
    console.error('Delete product error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function changeMallPage(page: number) {
  currentMallPage.value = page
}

// ===== System config =====
function saveSystemConfig() {
  toast.success('系统配置保存成功！')
}

function saveSecurityConfig() {
  toast.success('安全配置保存成功！')
}

function saveBackupConfig() {
  toast.success('备份配置保存成功！')
}

function backupNow() {
  toast.info('备份已开始，请稍后查看备份结果')
}

async function exportData() {
  exportLoading.value = true
  try {
    const res = await api.get('/api/admin/export')
    if (res.code === 200) {
      const blob = new Blob([JSON.stringify(res.data, null, 2)], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `campus_system_backup_${new Date().toISOString().slice(0, 10)}.json`
      document.body.appendChild(a)
      a.click()
      document.body.removeChild(a)
      URL.revokeObjectURL(url)
      toast.success('数据导出成功！')
    } else {
      toast.error(res.msg || '导出失败')
    }
  } catch (e) {
    console.error('Export error:', e)
    toast.error('导出失败，请稍后重试')
  } finally {
    exportLoading.value = false
  }
}

function handleFileSelect(event: Event) {
  const input = event.target as HTMLInputElement
  const file = input.files && input.files[0]
  if (file) {
    selectedFile.value = file
    selectedFileName.value = file.name
    importResult.value = null
  }
}

function readFileAsText(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = (e) => resolve(e.target?.result as string)
    reader.onerror = () => reject(new Error('文件读取失败'))
    reader.readAsText(file)
  })
}

async function importData() {
  if (!selectedFile.value) {
    toast.warning('请先选择要导入的文件')
    return
  }
  const ok = await confirmDialog({
    message: '确定要导入数据吗？此操作可能会修改现有数据。',
    variant: 'warning',
    confirmText: '确认导入',
  })
  if (!ok) return
  importLoading.value = true
  importResult.value = null
  try {
    const fileContent = await readFileAsText(selectedFile.value)
    const importPayload = JSON.parse(fileContent)
    const res = await api.post<{ imported: number; skipped: number }>('/api/admin/import', {
      data: importPayload,
      mode: importMode.value,
    })
    if (res.code === 200 && res.data) {
      importResult.value = {
        success: true,
        message: '数据导入成功！',
        imported: res.data.imported,
        skipped: res.data.skipped,
      }
      await loadUsers()
      await loadRoles()
      await loadPermissions()
      await loadDashboard()
    } else {
      importResult.value = { success: false, message: res.msg || '导入失败' }
    }
  } catch (e) {
    console.error('Import error:', e)
    importResult.value = { success: false, message: '导入失败：' + (e as Error).message }
  } finally {
    importLoading.value = false
  }
}

// ===== Tab Switching =====
function switchTab(tab: string) {
  if (tab === '__logout__') {
    logout()
    return
  }
  currentTab.value = tab
  if (tab === 'dashboard') {
    loadDashboard()
    nextTick(() => dashboardChartRef.value?.resize())
  } else if (tab === 'users') {
    loadUsers()
  } else if (tab === 'classes') {
    loadClasses()
  } else if (tab === 'mall') {
    loadProducts()
    loadExchangeRecords()
  }
}

// ===== Lifecycle =====
onMounted(async () => {
  const user = await checkAuth(1)
  if (!user) return
  userInfo.value = { ...user }
  await loadUsers()
  await loadRoles()
  await loadPermissions()
  await loadDashboard()
  await loadProducts()
  await loadExchangeRecords()
  nextTick(() => dashboardChartRef.value?.resize())
})
</script>

<template>
  <AppLayout
    :theme="THEMES.rose"
    brand="管理控制台"
    brand-en="Admin"
    brand-icon="fa-user-shield"
    :nav-items="ADMIN_NAV"
    :current-tab="currentTab"
    :user-info="userInfo"
    sidebar-width="w-64"
    @switch-tab="switchTab"
  >
    <!-- ====== Tab 1: 系统概览 ====== -->
    <div v-show="currentTab === 'dashboard'">
      <div class="mb-8 enter-up">
        <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Dashboard</p>
        <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">系统概览</h2>
        <p class="text-stone-500 mt-4 text-sm">实时洞察校园能量站的运行状态与关键指标</p>
      </div>

      <!-- 统计卡片 -->
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
        <StatCard
          label="总用户数"
          :value="dashboardStats.totalUsers"
          icon="fa-users"
          gradient-from="#7dd3fc"
          gradient-to="#0284c7"
          sublabel="All Users"
          :delay="1"
        />
        <StatCard
          label="学生数"
          :value="dashboardStats.studentCount"
          icon="fa-user-graduate"
          gradient-from="#6ee7b7"
          gradient-to="#059669"
          sublabel="Students"
          :delay="2"
        />
        <StatCard
          label="教师数"
          :value="dashboardStats.teacherCount"
          icon="fa-chalkboard-user"
          gradient-from="#fcd34d"
          gradient-to="#d97706"
          sublabel="Teachers"
          :delay="3"
        />
        <StatCard
          label="今日登录"
          :value="dashboardStats.todayLogin"
          icon="fa-right-to-bracket"
          gradient-from="#fb7185"
          gradient-to="#e11d48"
          sublabel="Today Login"
          :delay="4"
        />
      </div>

      <!-- 用户角色分布 -->
      <div class="glass-card rounded-3xl p-6 mb-6 enter-up enter-delay-5">
        <div class="flex items-center gap-3 mb-5">
          <div class="w-9 h-9 rounded-xl bg-rose-100 text-rose-600 flex items-center justify-center">
            <i class="fa-solid fa-chart-pie"></i>
          </div>
          <h3 class="font-display text-xl font-bold text-stone-800">用户角色分布</h3>
        </div>
        <BaseChart :option="roleDistributionOption" height="320px" />
      </div>

      <!-- 系统状态 -->
      <div class="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
        <div class="glass-card rounded-3xl p-6 enter-up enter-delay-5">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-9 h-9 rounded-xl bg-rose-100 text-rose-600 flex items-center justify-center">
              <i class="fa-solid fa-server"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">系统状态</h3>
          </div>
          <div class="space-y-4">
            <div class="flex justify-between items-center">
              <span class="text-sm text-stone-600">服务器状态</span>
              <span class="badge bg-emerald-100 text-emerald-700"><i class="fa-solid fa-circle text-[6px] mr-1.5"></i>运行中</span>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-sm text-stone-600">数据库状态</span>
              <span class="badge bg-emerald-100 text-emerald-700"><i class="fa-solid fa-circle text-[6px] mr-1.5"></i>正常</span>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-sm text-stone-600">内存使用</span>
              <div class="w-28 progress-track h-2">
                <div class="progress-fill bg-gradient-to-r from-sky-400 to-sky-600" style="width: 45%"></div>
              </div>
            </div>
            <div class="flex justify-between items-center">
              <span class="text-sm text-stone-600">磁盘使用</span>
              <div class="w-28 progress-track h-2">
                <div class="progress-fill bg-gradient-to-r from-amber-400 to-amber-600" style="width: 65%"></div>
              </div>
            </div>
          </div>
        </div>
        <div class="glass-card rounded-3xl p-6 enter-up enter-delay-6">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-9 h-9 rounded-xl bg-amber-100 text-amber-600 flex items-center justify-center">
              <i class="fa-solid fa-clock-rotate-left"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">最近活动</h3>
            <button type="button" @click="loadDashboard" class="ml-auto btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-1.5">
              <i class="fa-solid fa-rotate-right"></i>刷新
            </button>
          </div>
          <EmptyState v-if="recentActivities.length === 0" icon="fa-clock-rotate-left" text="暂无活动记录" variant="inline" />
          <div v-else class="space-y-4">
            <div v-for="activity in recentActivities" :key="activity.id" class="flex items-start">
              <div class="w-2 h-2 bg-rose-500 rounded-full mt-2 mr-3 shrink-0 shadow-[0_0_0_4px_rgba(225,29,72,0.15)]"></div>
              <div class="flex-1">
                <p class="text-sm text-stone-800">{{ activity.description }}</p>
                <p class="text-xs text-stone-400 mt-1">{{ activity.time }}</p>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 2: 用户管理 ====== -->
    <div v-show="currentTab === 'users'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-end mb-8 gap-4 enter-up">
        <div>
          <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Users</p>
          <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">用户管理</h2>
        </div>
        <div class="flex flex-col md:flex-row gap-3">
          <button type="button" @click="openAddUser()" class="btn-ruby px-5 py-3 rounded-2xl font-medium flex items-center self-start">
            <i class="fa-solid fa-plus mr-2"></i> 添加用户
          </button>
          <button type="button" @click="openBatchImport()" class="btn-ghost px-5 py-3 rounded-2xl font-medium flex items-center self-start">
            <i class="fa-solid fa-file-import mr-2"></i> 批量导入学生
          </button>
        </div>
      </div>

      <!-- 搜索和筛选 -->
      <div class="glass-card rounded-3xl p-5 mb-6 enter-up enter-delay-1">
        <div class="flex flex-col md:flex-row gap-3">
          <div class="flex-1">
            <input v-model="userSearch" type="text" placeholder="搜索用户名或姓名..." class="field-input w-full px-4 py-3 rounded-2xl text-sm">
          </div>
          <div class="md:w-48">
            <select v-model="userRoleFilter" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
              <option value="">所有角色</option>
              <option :value="1">管理员</option>
              <option :value="2">教师</option>
              <option :value="3">学生</option>
              <option :value="4">家长</option>
            </select>
          </div>
          <button type="button" @click="searchUsers" class="btn-ruby px-5 py-3 rounded-2xl text-sm font-medium flex items-center justify-center">
            <i class="fa-solid fa-search mr-2"></i> 搜索
          </button>
        </div>
      </div>

      <!-- 用户列表 -->
      <div class="glass-card rounded-3xl overflow-hidden enter-up enter-delay-2">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead class="bg-stone-50/60">
              <tr>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">ID</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">用户名</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">姓名</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">角色</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">状态</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-100">
              <tr v-for="user in pagedUsers" :key="user.id + '-' + user.role_id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-bold text-stone-900">{{ user.id }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ user.username }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ user.name }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <RoleBadge :role-id="Number(user.role_id)" />
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span class="badge bg-emerald-100 text-emerald-700">
                    <i class="fa-solid fa-circle text-[6px] mr-1.5"></i>活跃
                  </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <button type="button" @click="openEditUser(user)" class="text-sky-600 hover:text-sky-800 mr-3 font-medium hover:underline">
                    <i class="fa-solid fa-pen-to-square mr-1"></i> 编辑
                  </button>
                  <button type="button" @click="deleteUser(user)" class="text-rose-600 hover:text-rose-800 mr-3 font-medium hover:underline">
                    <i class="fa-solid fa-trash mr-1"></i> 删除
                  </button>
                  <button type="button" @click="resetPassword(user)" class="text-amber-600 hover:text-amber-800 font-medium hover:underline">
                    <i class="fa-solid fa-key mr-1"></i> 重置密码
                  </button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
        <div class="px-6 py-4 border-t border-stone-100">
          <Pagination
            :current-page="currentPage"
            :total-pages="userTotalPages"
            :total="filteredUsersList.length"
            :start-index="userStartIndex"
            :end-index="userEndIndex"
            @change="changeUserPage"
          />
        </div>
      </div>
    </div>

    <!-- ====== Tab 3: 班级管理 ====== -->
    <div v-show="currentTab === 'classes'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-end mb-8 gap-4 enter-up">
        <div>
          <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Classes</p>
          <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">班级管理</h2>
        </div>
        <button type="button" @click="showAddClassDialog = true" class="btn-ruby px-5 py-3 rounded-2xl font-medium flex items-center self-start">
          <i class="fa-solid fa-plus mr-2"></i> 添加班级
        </button>
      </div>

      <div class="glass-card rounded-3xl overflow-hidden enter-up enter-delay-1">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead class="bg-stone-50/60">
              <tr>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">ID</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">班级名称</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">年级</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">编码</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">班主任</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">描述</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">学生人数</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-100">
              <tr v-for="cls in classes" :key="cls.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-bold text-stone-900">{{ cls.id }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-medium text-stone-700">{{ cls.name }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ cls.grade }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-500 font-mono text-xs">{{ cls.grade_code || '—' }} / {{ cls.class_code || '—' }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ cls.head_teacher }}</td>
                <td class="px-6 py-4 text-sm text-stone-500 max-w-xs">{{ cls.description }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span class="badge bg-sky-100 text-sky-700">{{ cls.student_count }} 人</span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <button type="button" @click="openEditClass(cls)" class="text-sky-600 hover:text-sky-800 mr-3 font-medium hover:underline">
                    <i class="fa-solid fa-pen-to-square mr-1"></i> 编辑
                  </button>
                  <button type="button" @click="deleteClass(cls)" class="text-rose-600 hover:text-rose-800 font-medium hover:underline">
                    <i class="fa-solid fa-trash mr-1"></i> 删除
                  </button>
                </td>
              </tr>
              <EmptyState v-if="classes.length === 0" icon="fa-school" text="暂无班级数据" variant="table-row" :colspan="8" />
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Tab 4: 角色管理 ====== -->
    <div v-show="currentTab === 'roles'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-end mb-8 gap-4 enter-up">
        <div>
          <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Roles</p>
          <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">角色管理</h2>
        </div>
        <button type="button" @click="showAddRoleDialog = true" class="btn-ruby px-5 py-3 rounded-2xl font-medium flex items-center self-start">
          <i class="fa-solid fa-plus mr-2"></i> 添加角色
        </button>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-5">
        <div v-for="(role, idx) in roles" :key="role.id" class="role-card rounded-3xl p-6 enter-up" :class="'enter-delay-' + (idx + 1)">
          <div class="flex justify-between items-start mb-4">
            <div class="flex items-center gap-3">
              <div class="w-11 h-11 rounded-2xl bg-gradient-to-br from-rose-400 to-rose-700 text-white flex items-center justify-center shadow-md shadow-rose-500/30">
                <i class="fa-solid fa-user-gear"></i>
              </div>
              <h3 class="font-display text-xl font-bold text-stone-800">{{ role.name }}</h3>
            </div>
            <div class="flex space-x-1">
              <button type="button" @click="editRole(role)" class="w-8 h-8 rounded-lg flex items-center justify-center text-sky-600 hover:bg-sky-50 transition">
                <i class="fa-solid fa-pen-to-square text-sm"></i>
              </button>
              <button type="button" @click="deleteRole(role)" class="w-8 h-8 rounded-lg flex items-center justify-center text-rose-600 hover:bg-rose-50 transition">
                <i class="fa-solid fa-trash text-sm"></i>
              </button>
            </div>
          </div>
          <p class="text-sm text-stone-500 mb-5 leading-relaxed">{{ role.description }}</p>
          <div class="border-t border-stone-100 pt-4">
            <h4 class="text-[11px] font-bold text-stone-500 uppercase tracking-wider mb-3">权限列表</h4>
            <div class="flex flex-wrap gap-1.5">
              <span v-for="permission in role.permissions" :key="permission.id" class="badge bg-stone-100 text-stone-700">
                {{ permission.name }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 5: 权限管理 ====== -->
    <div v-show="currentTab === 'permissions'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-end mb-8 gap-4 enter-up">
        <div>
          <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Permissions</p>
          <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">权限管理</h2>
        </div>
        <button type="button" @click="showAddPermissionDialog = true" class="btn-ruby px-5 py-3 rounded-2xl font-medium flex items-center self-start">
          <i class="fa-solid fa-plus mr-2"></i> 添加权限
        </button>
      </div>

      <div class="glass-card rounded-3xl overflow-hidden enter-up enter-delay-1">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead class="bg-stone-50/60">
              <tr>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">ID</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">权限名称</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">权限代码</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">描述</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-100">
              <tr v-for="permission in permissions" :key="permission.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-bold text-stone-900">{{ permission.id }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-medium text-stone-700">{{ permission.name }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <code class="px-2 py-1 bg-stone-100 text-rose-700 rounded-md font-mono text-xs">{{ permission.code }}</code>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ permission.description }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <button type="button" @click="editPermission(permission)" class="text-sky-600 hover:text-sky-800 mr-3 font-medium hover:underline">
                    <i class="fa-solid fa-pen-to-square mr-1"></i> 编辑
                  </button>
                  <button type="button" @click="deletePermission(permission)" class="text-rose-600 hover:text-rose-800 font-medium hover:underline">
                    <i class="fa-solid fa-trash mr-1"></i> 删除
                  </button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Tab 6: 系统配置 ====== -->
    <div v-show="currentTab === 'system'">
      <div class="mb-8 enter-up">
        <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Settings</p>
        <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">系统配置</h2>
      </div>

      <div class="space-y-6">
        <!-- 基本配置 -->
        <div class="glass-card rounded-3xl p-6 enter-up enter-delay-1">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-10 h-10 rounded-xl bg-rose-100 text-rose-600 flex items-center justify-center">
              <i class="fa-solid fa-sliders"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">基本配置</h3>
          </div>
          <div class="space-y-4">
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">系统名称</label>
              <input v-model="systemConfig.systemName" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            </div>
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">系统版本</label>
              <input v-model="systemConfig.version" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            </div>
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">系统描述</label>
              <textarea v-model="systemConfig.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
            </div>
          </div>
          <div class="mt-6 flex justify-end">
            <button type="button" @click="saveSystemConfig" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存配置</button>
          </div>
        </div>

        <!-- 安全配置 -->
        <div class="glass-card rounded-3xl p-6 enter-up enter-delay-2">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-10 h-10 rounded-xl bg-amber-100 text-amber-600 flex items-center justify-center">
              <i class="fa-solid fa-shield-halved"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">安全配置</h3>
          </div>
          <div class="space-y-4">
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">登录超时时间（分钟）</label>
              <input v-model="securityConfig.loginTimeout" type="number" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            </div>
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">密码最小长度</label>
              <input v-model="securityConfig.minPasswordLength" type="number" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            </div>
            <div class="flex items-center p-3 rounded-xl hover:bg-stone-50/60 transition">
              <input v-model="securityConfig.enableCaptcha" type="checkbox" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300 rounded">
              <label class="ml-3 block text-sm text-stone-700">启用登录验证码</label>
            </div>
          </div>
          <div class="mt-6 flex justify-end">
            <button type="button" @click="saveSecurityConfig" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存配置</button>
          </div>
        </div>

        <!-- 数据备份 -->
        <div class="glass-card rounded-3xl p-6 enter-up enter-delay-3">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-10 h-10 rounded-xl bg-emerald-100 text-emerald-600 flex items-center justify-center">
              <i class="fa-solid fa-database"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">数据备份</h3>
          </div>
          <div class="space-y-4">
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">备份频率</label>
              <select v-model="backupConfig.frequency" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
                <option value="daily">每天</option>
                <option value="weekly">每周</option>
                <option value="monthly">每月</option>
              </select>
            </div>
            <div>
              <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">备份保留天数</label>
              <input v-model="backupConfig.retentionDays" type="number" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            </div>
          </div>
          <div class="mt-6 flex flex-wrap gap-3">
            <button type="button" @click="backupNow" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">
              <i class="fa-solid fa-download mr-2"></i> 立即备份
            </button>
            <button type="button" @click="saveBackupConfig" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存配置</button>
          </div>
        </div>

        <!-- 数据管理 -->
        <div class="glass-card rounded-3xl p-6 enter-up enter-delay-4">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-10 h-10 rounded-xl bg-sky-100 text-sky-600 flex items-center justify-center">
              <i class="fa-solid fa-arrow-right-arrow-left"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">数据管理</h3>
          </div>
          <p class="text-sm text-stone-500 mb-5 leading-relaxed">导出或导入系统数据，支持 JSON 格式。导出数据包含用户、角色、权限、积分记录、评价、商城商品和兑换记录。</p>

          <div class="space-y-4">
            <div class="p-5 bg-stone-50/70 rounded-2xl border border-stone-100">
              <h4 class="font-bold text-stone-700 mb-2 flex items-center gap-2">
                <i class="fa-solid fa-file-export text-emerald-600"></i> 数据导出
              </h4>
              <p class="text-sm text-stone-500 mb-4">将系统所有数据导出为 JSON 文件，可用于备份或迁移。</p>
              <button type="button" @click="exportData" :disabled="exportLoading" class="px-5 py-2.5 rounded-2xl text-sm font-medium text-white bg-gradient-to-r from-emerald-500 to-emerald-700 shadow-lg shadow-emerald-500/30 hover:shadow-emerald-500/50 transition disabled:opacity-50">
                <i class="fa-solid fa-file-export mr-2"></i>
                <span v-if="exportLoading">导出中...</span>
                <span v-else>导出数据</span>
              </button>
            </div>

            <div class="p-5 bg-stone-50/70 rounded-2xl border border-stone-100">
              <h4 class="font-bold text-stone-700 mb-2 flex items-center gap-2">
                <i class="fa-solid fa-file-import text-sky-600"></i> 数据导入
              </h4>
              <p class="text-sm text-stone-500 mb-4">从 JSON 文件导入数据到系统。</p>

              <div class="space-y-3">
                <div>
                  <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">导入模式</label>
                  <select v-model="importMode" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
                    <option value="skip">跳过重复数据（保留现有数据）</option>
                    <option value="overwrite">覆盖现有数据（更新重复项）</option>
                  </select>
                </div>

                <div class="flex items-center space-x-3">
                  <input type="file" id="importFile" @change="handleFileSelect" accept=".json" class="hidden">
                  <label for="importFile" class="cursor-pointer btn-ghost px-4 py-2.5 rounded-2xl text-sm font-medium">
                    <i class="fa-solid fa-folder-open mr-2"></i> 选择文件
                  </label>
                  <span v-if="selectedFileName" class="text-sm text-stone-600">{{ selectedFileName }}</span>
                  <span v-else class="text-sm text-stone-400">未选择文件</span>
                </div>

                <button type="button" @click="importData" :disabled="importLoading || !selectedFile" class="px-5 py-2.5 rounded-2xl text-sm font-medium text-white bg-gradient-to-r from-sky-500 to-sky-700 shadow-lg shadow-sky-500/30 hover:shadow-sky-500/50 transition disabled:opacity-50">
                  <i class="fa-solid fa-file-import mr-2"></i>
                  <span v-if="importLoading">导入中...</span>
                  <span v-else>开始导入</span>
                </button>
              </div>

              <div v-if="importResult" class="mt-4 p-4 rounded-2xl" :class="importResult.success ? 'bg-emerald-50 border border-emerald-200' : 'bg-rose-50 border border-rose-200'">
                <div class="flex items-center">
                  <i :class="importResult.success ? 'fa-solid fa-check-circle text-emerald-500' : 'fa-solid fa-times-circle text-rose-500'" class="text-xl mr-2"></i>
                  <span :class="importResult.success ? 'text-emerald-700' : 'text-rose-700'" class="font-medium">{{ importResult.message }}</span>
                </div>
                <div v-if="importResult.success" class="mt-2 text-sm text-stone-600">
                  <p>成功导入: {{ importResult.imported }} 条</p>
                  <p>跳过重复: {{ importResult.skipped }} 条</p>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 7: 商城管理 ====== -->
    <div v-show="currentTab === 'mall'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-end mb-8 gap-4 enter-up">
        <div>
          <p class="text-xs font-bold tracking-[0.25em] text-rose-500 uppercase mb-2">Mall</p>
          <h2 class="font-display text-4xl md:text-5xl font-black text-stone-900 title-deco">商城管理</h2>
        </div>
        <button type="button" @click="showAddProductDialog = true" class="btn-ruby px-5 py-3 rounded-2xl font-medium flex items-center self-start">
          <i class="fa-solid fa-plus mr-2"></i> 添加商品
        </button>
      </div>

      <!-- 商品列表 -->
      <div class="glass-card rounded-3xl overflow-hidden mb-6 enter-up enter-delay-1">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead class="bg-stone-50/60">
              <tr>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">ID</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">商品名称</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">描述</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">积分</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">库存</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">状态</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-100">
              <tr v-for="product in pagedProducts" :key="product.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-bold text-stone-900">{{ product.id }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-medium text-stone-700">{{ product.name }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-500 max-w-xs">{{ product.description }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span class="font-display font-bold text-amber-600">{{ product.cost }}</span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ product.stock }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span :class="product.status === 1 ? 'badge bg-emerald-100 text-emerald-700' : 'badge bg-rose-100 text-rose-700'">
                    {{ product.status === 1 ? '上架' : '下架' }}
                  </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <button type="button" @click="openEditProductDialog(product)" class="text-sky-600 hover:text-sky-800 mr-3 font-medium hover:underline">
                    <i class="fa-solid fa-pen-to-square mr-1"></i> 编辑
                  </button>
                  <button type="button" @click="deleteProduct(product)" class="text-rose-600 hover:text-rose-800 font-medium hover:underline">
                    <i class="fa-solid fa-trash mr-1"></i> 删除
                  </button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
        <div class="px-6 py-4 border-t border-stone-100">
          <Pagination
            :current-page="currentMallPage"
            :total-pages="mallTotalPages"
            :total="products.length"
            :start-index="mallStartIndex"
            :end-index="mallEndIndex"
            @change="changeMallPage"
          />
        </div>
      </div>

      <!-- 兑换记录 -->
      <div class="glass-card rounded-3xl overflow-hidden enter-up enter-delay-2">
        <div class="p-6 border-b border-stone-100 flex items-center gap-3">
          <div class="w-9 h-9 rounded-xl bg-amber-100 text-amber-600 flex items-center justify-center">
            <i class="fa-solid fa-receipt"></i>
          </div>
          <h3 class="font-display text-xl font-bold text-stone-800">兑换记录</h3>
        </div>
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead class="bg-stone-50/60">
              <tr>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">ID</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">用户</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">商品</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">积分</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">时间</th>
                <th class="px-6 py-4 text-left text-[11px] font-bold text-stone-500 uppercase tracking-wider">状态</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-100">
              <tr v-for="record in exchangeRecords" :key="record.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-bold text-stone-900">{{ record.id }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ record.username }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-medium text-stone-700">{{ record.product_name }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span class="font-display font-bold text-amber-600">{{ record.cost }}</span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-500">{{ formatDateTime(record.created_at) }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span class="badge bg-emerald-100 text-emerald-700">
                    <i class="fa-solid fa-circle text-[6px] mr-1.5"></i>已完成
                  </span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ================= 对话框区域 ================= -->

    <!-- Modal 1: 添加用户 -->
    <BaseModal
      :show="showAddUserDialog"
      title="添加用户"
      icon="fa-user-plus"
      icon-from="#fb7185"
      icon-to="#be123c"
      @close="showAddUserDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">用户名</label>
          <input v-model="newUser.username" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">姓名</label>
          <input v-model="newUser.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">角色</label>
          <select v-model="newUser.role_id" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            <option :value="1">管理员</option>
            <option :value="2">教师</option>
            <option :value="3">学生</option>
            <option :value="4">家长</option>
          </select>
        </div>
        <div v-if="newUser.role_id === 3">
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">班级</label>
          <select v-model="newUser.className" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            <option value="高二(1)班">高二(1)班</option>
            <option value="高二(2)班">高二(2)班</option>
            <option value="高二(3)班">高二(3)班</option>
          </select>
        </div>
        <div v-if="newUser.role_id === 2">
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">绑定班级</label>
          <div class="space-y-2 max-h-40 overflow-y-auto p-3 bg-stone-50 rounded-xl">
            <label v-for="cls in classes" :key="cls.id" class="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" :value="cls.id" v-model="newUser.bound_class_ids" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300 rounded">
              <span class="text-sm text-stone-700">{{ cls.name }}</span>
            </label>
            <p v-if="classes.length === 0" class="text-xs text-stone-400 italic">暂无可选班级</p>
          </div>
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">密码</label>
          <input v-model="newUser.password" type="password" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div v-if="newUser.role_id === 4" class="pt-4 border-t border-stone-100">
          <p class="text-xs font-bold text-rose-500 uppercase tracking-wider mb-3"><i class="fa-solid fa-family mr-1"></i> 绑定子女</p>
          <div class="space-y-2 max-h-40 overflow-y-auto p-3 bg-stone-50 rounded-xl">
            <label v-for="stu in studentsList" :key="stu.id" class="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" :value="stu.id" v-model="newUser.bound_student_ids" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300 rounded">
              <span class="text-sm text-stone-700">{{ stu.name }}（{{ stu.username }}）</span>
            </label>
            <p v-if="studentsList.length === 0" class="text-xs text-stone-400 italic">暂无可选学生</p>
          </div>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showAddUserDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="addUser" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 2: 批量导入学生 -->
    <BaseModal
      :show="showBatchImportDialog"
      title="批量导入学生"
      icon="fa-file-import"
      icon-from="#7dd3fc"
      icon-to="#0369a1"
      max-width="max-w-2xl"
      @close="showBatchImportDialog = false"
    >
      <!-- Step 1: 选择文件 -->
      <div v-if="batchImportStep === 'select'">
        <div class="bg-sky-50 border border-sky-200 rounded-2xl p-4 mb-4 text-sm text-stone-600">
          <p class="font-medium text-sky-700 mb-1"><i class="fa-solid fa-info-circle mr-1"></i> 导入说明</p>
          <p>1. 先点击"下载模板"，填写学生姓名和班级名称（须与系统班级完全一致）</p>
          <p>2. 班级名称可参考：{{ classes.map(c => c.name).join('、') }}</p>
          <p>3. 用户名和密码由系统自动生成（姓名拼音首字母+班级编码+序号）</p>
        </div>
        <div class="flex gap-3 mb-4">
          <button type="button" @click="downloadStudentTemplate" class="btn-ghost px-4 py-2.5 rounded-2xl text-sm font-medium">
            <i class="fa-solid fa-download mr-2"></i>下载模板
          </button>
        </div>
        <div class="border-2 border-dashed border-stone-200 rounded-2xl p-6 text-center">
          <input type="file" id="batchImportFile" @change="handleBatchFileSelect" accept=".xlsx,.csv" class="hidden">
          <label for="batchImportFile" class="cursor-pointer">
            <i class="fa-solid fa-cloud-arrow-up text-4xl text-stone-300 mb-2 block"></i>
            <span class="text-stone-500">点击选择 .xlsx 或 .csv 文件</span>
          </label>
        </div>
      </div>

      <!-- Step 2: 预览 -->
      <div v-if="batchImportStep === 'preview'">
        <p class="text-sm text-stone-600 mb-3">共解析到 {{ batchImportRows.length }} 行，其中 {{ batchImportRows.filter(r => r.matched).length }} 行班级匹配成功，{{ batchImportRows.filter(r => !r.matched).length }} 行未匹配（将被跳过）。</p>
        <div class="overflow-x-auto max-h-[40vh] overflow-y-auto border border-stone-100 rounded-2xl">
          <table class="w-full text-sm">
            <thead class="bg-stone-50 sticky top-0">
              <tr>
                <th class="px-3 py-2 text-left">行号</th>
                <th class="px-3 py-2 text-left">姓名</th>
                <th class="px-3 py-2 text-left">班级名称</th>
                <th class="px-3 py-2 text-left">状态</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in batchImportRows" :key="row.row" :class="row.matched ? '' : 'bg-rose-50'">
                <td class="px-3 py-2">{{ row.row }}</td>
                <td class="px-3 py-2">{{ row.name }}</td>
                <td class="px-3 py-2">{{ row.className }}</td>
                <td class="px-3 py-2">
                  <span v-if="row.matched" class="text-emerald-600"><i class="fa-solid fa-check"></i> 匹配</span>
                  <span v-else class="text-rose-500"><i class="fa-solid fa-xmark"></i> 未匹配</span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Step 3: 结果 -->
      <div v-if="batchImportStep === 'result' && batchImportResult">
        <div class="grid grid-cols-3 gap-3 mb-4">
          <div class="bg-emerald-50 rounded-2xl p-3 text-center">
            <div class="text-2xl font-bold text-emerald-600">{{ batchImportResult.success }}</div>
            <div class="text-xs text-stone-500">成功</div>
          </div>
          <div class="bg-rose-50 rounded-2xl p-3 text-center">
            <div class="text-2xl font-bold text-rose-600">{{ batchImportResult.failed }}</div>
            <div class="text-xs text-stone-500">失败</div>
          </div>
          <div class="bg-stone-50 rounded-2xl p-3 text-center">
            <div class="text-2xl font-bold text-stone-700">{{ batchImportResult.success + batchImportResult.failed }}</div>
            <div class="text-xs text-stone-500">总计</div>
          </div>
        </div>
        <button type="button" @click="copyBatchAccounts" v-if="batchImportResult.successList.length" class="btn-ghost px-4 py-2 rounded-2xl text-sm font-medium mb-3">
          <i class="fa-solid fa-copy mr-2"></i>复制全部账号密码
        </button>
        <div v-if="batchImportResult.successList.length" class="mb-3">
          <p class="text-xs font-bold text-stone-500 uppercase mb-2">成功明细</p>
          <div class="overflow-x-auto max-h-[25vh] overflow-y-auto border border-emerald-100 rounded-2xl">
            <table class="w-full text-xs">
              <thead class="bg-emerald-50 sticky top-0">
                <tr><th class="px-2 py-1 text-left">姓名</th><th class="px-2 py-1 text-left">用户名</th><th class="px-2 py-1 text-left">密码</th><th class="px-2 py-1 text-left">学生ID</th></tr>
              </thead>
              <tbody>
                <tr v-for="r in batchImportResult.successList" :key="r.row">
                  <td class="px-2 py-1">{{ r.name }}</td>
                  <td class="px-2 py-1 font-mono">{{ r.username }}</td>
                  <td class="px-2 py-1 font-mono">{{ r.password }}</td>
                  <td class="px-2 py-1 font-mono text-stone-500">{{ r.student_id }}</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>
        <div v-if="batchImportResult.failedList.length">
          <p class="text-xs font-bold text-stone-500 uppercase mb-2">失败明细</p>
          <div class="overflow-x-auto max-h-[25vh] overflow-y-auto border border-rose-100 rounded-2xl">
            <table class="w-full text-xs">
              <thead class="bg-rose-50 sticky top-0">
                <tr><th class="px-2 py-1 text-left">行号</th><th class="px-2 py-1 text-left">姓名</th><th class="px-2 py-1 text-left">原因</th></tr>
              </thead>
              <tbody>
                <tr v-for="r in batchImportResult.failedList" :key="r.row">
                  <td class="px-2 py-1">{{ r.row }}</td>
                  <td class="px-2 py-1">{{ r.name }}</td>
                  <td class="px-2 py-1 text-rose-600">{{ r.reason }}</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>
      </div>

      <template #footer>
        <button type="button" @click="showBatchImportDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">
          {{ batchImportStep === 'result' ? '关闭' : '取消' }}
        </button>
        <div class="flex gap-2">
          <button v-if="batchImportStep === 'preview'" type="button" @click="resetBatchImport" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">重新选择</button>
          <button v-if="batchImportStep === 'result'" type="button" @click="resetBatchImport" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">继续导入</button>
          <button v-if="batchImportStep === 'preview'" type="button" @click="submitBatchImport" :disabled="batchImportLoading || batchImportRows.filter(r => r.matched).length === 0" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium disabled:opacity-50">
            <span v-if="batchImportLoading">导入中...</span>
            <span v-else>确认导入 ({{ batchImportRows.filter(r => r.matched).length }} 人)</span>
          </button>
        </div>
      </template>
    </BaseModal>

    <!-- Modal 3: 编辑用户 -->
    <BaseModal
      :show="showEditUserDialog"
      title="编辑用户"
      icon="fa-pen-to-square"
      icon-from="#7dd3fc"
      icon-to="#0369a1"
      @close="showEditUserDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">用户名</label>
          <input v-model="editUser.username" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">姓名</label>
          <input v-model="editUser.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">角色</label>
          <select v-model="editUser.role_id" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            <option :value="1">管理员</option>
            <option :value="2">教师</option>
            <option :value="3">学生</option>
            <option :value="4">家长</option>
          </select>
        </div>
        <div v-if="editUser.role_id === 3">
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">班级</label>
          <select v-model="editUser.className" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            <option value="高二(1)班">高二(1)班</option>
            <option value="高二(2)班">高二(2)班</option>
            <option value="高二(3)班">高二(3)班</option>
          </select>
        </div>
        <div v-if="editUser.role_id === 2">
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">绑定班级</label>
          <div class="space-y-2 max-h-40 overflow-y-auto p-3 bg-stone-50 rounded-xl">
            <label v-for="cls in classes" :key="cls.id" class="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" :value="cls.id" v-model="editUser.bound_class_ids" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300 rounded">
              <span class="text-sm text-stone-700">{{ cls.name }}</span>
            </label>
            <p v-if="classes.length === 0" class="text-xs text-stone-400 italic">暂无可选班级</p>
          </div>
        </div>
        <div v-if="editUser.role_id === 4" class="pt-4 border-t border-stone-100">
          <p class="text-xs font-bold text-rose-500 uppercase tracking-wider mb-3"><i class="fa-solid fa-family mr-1"></i> 绑定子女</p>
          <div class="space-y-2 max-h-40 overflow-y-auto p-3 bg-stone-50 rounded-xl">
            <label v-for="stu in studentsList" :key="stu.id" class="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" :value="stu.id" v-model="editUser.bound_student_ids" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300 rounded">
              <span class="text-sm text-stone-700">{{ stu.name }}（{{ stu.username }}）</span>
            </label>
            <p v-if="studentsList.length === 0" class="text-xs text-stone-400 italic">暂无可选学生</p>
          </div>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showEditUserDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="updateUser" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 4: 添加角色 -->
    <BaseModal
      :show="showAddRoleDialog"
      title="添加角色"
      icon="fa-user-gear"
      icon-from="#fb7185"
      icon-to="#be123c"
      @close="showAddRoleDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">角色名称</label>
          <input v-model="newRole.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">角色描述</label>
          <textarea v-model="newRole.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限</label>
          <div class="space-y-2 max-h-48 overflow-y-auto p-1">
            <label v-for="permission in permissions" :key="permission.id" class="flex items-center p-2.5 rounded-xl hover:bg-stone-50 cursor-pointer transition">
              <input v-model="newRole.permissions" :value="permission.id" type="checkbox" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300 rounded">
              <span class="ml-3 text-sm text-stone-700">{{ permission.name }}</span>
            </label>
          </div>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showAddRoleDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="addRole" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 5: 添加权限 -->
    <BaseModal
      :show="showAddPermissionDialog"
      title="添加权限"
      icon="fa-lock"
      icon-from="#fb7185"
      icon-to="#be123c"
      @close="showAddPermissionDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限名称</label>
          <input v-model="newPermission.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限代码</label>
          <input v-model="newPermission.code" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm font-mono">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限描述</label>
          <textarea v-model="newPermission.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showAddPermissionDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="addPermission" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 6: 编辑角色 -->
    <BaseModal
      :show="showEditRoleDialog"
      title="编辑角色"
      icon="fa-pen-to-square"
      icon-from="#7dd3fc"
      icon-to="#0369a1"
      @close="showEditRoleDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">角色名称</label>
          <input v-model="editRoleData.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">角色描述</label>
          <textarea v-model="editRoleData.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showEditRoleDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="updateRole" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 7: 编辑权限 -->
    <BaseModal
      :show="showEditPermissionDialog"
      title="编辑权限"
      icon="fa-pen-to-square"
      icon-from="#7dd3fc"
      icon-to="#0369a1"
      @close="showEditPermissionDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限名称</label>
          <input v-model="editPermissionData.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限代码</label>
          <input v-model="editPermissionData.code" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm font-mono">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">权限描述</label>
          <textarea v-model="editPermissionData.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showEditPermissionDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="updatePermission" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 8: 重置密码 -->
    <BaseModal
      :show="showResetPasswordDialog"
      title="重置密码"
      icon="fa-key"
      icon-from="#fcd34d"
      icon-to="#d97706"
      @close="showResetPasswordDialog = false"
    >
      <p class="text-sm text-stone-500 mb-4">为用户 <span class="font-bold text-rose-600">{{ resetPasswordUser.name }}</span> 重置密码</p>

      <div v-if="!resetPasswordResult" class="space-y-4">
        <div class="bg-sky-50 border border-sky-200 rounded-2xl p-4">
          <p class="text-sm text-sky-800 flex items-center">
            <i class="fa-solid fa-info-circle mr-2"></i>
            请选择重置密码的方式
          </p>
        </div>

        <div class="space-y-3">
          <label class="radio-card flex items-center p-4 border border-stone-200 rounded-2xl" :class="resetPasswordMode === 'auto' ? 'active' : ''">
            <input type="radio" v-model="resetPasswordMode" value="auto" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300">
            <div class="ml-3">
              <span class="text-sm font-bold text-stone-900">自动生成随机密码</span>
              <p class="text-xs text-stone-500 mt-0.5">系统将生成8-12位包含大小写字母和数字的安全密码</p>
            </div>
          </label>

          <label class="radio-card flex items-center p-4 border border-stone-200 rounded-2xl" :class="resetPasswordMode === 'manual' ? 'active' : ''">
            <input type="radio" v-model="resetPasswordMode" value="manual" class="h-4 w-4 text-rose-600 focus:ring-rose-500 border-stone-300">
            <div class="ml-3">
              <span class="text-sm font-bold text-stone-900">手动输入新密码</span>
              <p class="text-xs text-stone-500 mt-0.5">请输入至少6位的新密码</p>
            </div>
          </label>
        </div>

        <div v-if="resetPasswordMode === 'manual'" class="mt-4">
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">新密码</label>
          <input v-model="manualPassword" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm" placeholder="请输入至少6位密码">
        </div>
      </div>

      <div v-else class="space-y-4">
        <div class="bg-emerald-50 border border-emerald-200 rounded-2xl p-4">
          <div class="flex items-center">
            <i class="fa-solid fa-check-circle text-emerald-500 text-xl mr-3"></i>
            <span class="text-emerald-800 font-bold">密码重置成功！</span>
          </div>
        </div>

        <div class="bg-stone-50 rounded-2xl p-4">
          <p class="text-sm text-stone-600 mb-2">新密码：</p>
          <div class="flex items-center justify-between bg-white border border-stone-200 rounded-xl p-3">
            <code class="text-lg font-mono text-rose-600 font-bold">{{ resetPasswordResult }}</code>
            <button type="button" @click="copyPassword" class="ml-3 px-3 py-1.5 bg-rose-600 text-white text-sm rounded-lg hover:bg-rose-700 transition">
              <i class="fa-solid fa-copy mr-1"></i> 复制
            </button>
          </div>
        </div>

        <div class="bg-amber-50 border border-amber-200 rounded-2xl p-3">
          <p class="text-xs text-amber-800 flex items-start">
            <i class="fa-solid fa-exclamation-triangle mr-2 mt-0.5"></i>
            请妥善保管新密码，关闭此对话框后将无法再次查看
          </p>
        </div>
      </div>

      <template #footer>
        <button v-if="!resetPasswordResult" type="button" @click="showResetPasswordDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button v-if="!resetPasswordResult" type="button" @click="confirmResetPassword" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">确认重置</button>
        <button v-else type="button" @click="closeResetPasswordDialog" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">关闭</button>
      </template>
    </BaseModal>

    <!-- Modal 9: 添加班级 -->
    <BaseModal
      :show="showAddClassDialog"
      title="添加班级"
      icon="fa-school"
      icon-from="#fb7185"
      icon-to="#be123c"
      @close="showAddClassDialog = false"
    >
      <div class="space-y-4">
        <div class="grid grid-cols-2 gap-3">
          <div>
            <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">年级 <span class="text-rose-500">*</span></label>
            <select v-model="newClass.grade" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
              <option value="">请选择年级</option>
              <option value="高一">高一</option>
              <option value="高二">高二</option>
              <option value="高三">高三</option>
              <option value="初一">初一</option>
              <option value="初二">初二</option>
              <option value="初三">初三</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">班号 <span class="text-rose-500">*</span></label>
            <input v-model="newClass.classNo" type="number" min="1" placeholder="例如：1" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
          </div>
        </div>
        <div v-if="newClass.grade && newClass.classNo" class="bg-rose-50 border border-rose-200 rounded-2xl p-3 text-sm">
          <span class="text-stone-500">将创建班级：</span>
          <span class="font-bold text-stone-800">{{ buildClassName(newClass.grade, newClass.classNo) }}</span>
          <span class="text-stone-400 ml-2">| 编码 {{ gradeToCode(newClass.grade) }} / {{ classNoToCode(newClass.classNo) }}</span>
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">班主任</label>
          <input v-model="newClass.head_teacher" type="text" placeholder="请输入班主任姓名" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">描述</label>
          <textarea v-model="newClass.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3" placeholder="班级描述（可选）"></textarea>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showAddClassDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="addClass" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 10: 编辑班级 -->
    <BaseModal
      :show="showEditClassDialog"
      title="编辑班级"
      icon="fa-pen-to-square"
      icon-from="#7dd3fc"
      icon-to="#0369a1"
      @close="showEditClassDialog = false"
    >
      <div class="space-y-4">
        <div class="grid grid-cols-2 gap-3">
          <div>
            <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">年级 <span class="text-rose-500">*</span></label>
            <select v-model="editClassData.grade" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
              <option value="">请选择年级</option>
              <option value="高一">高一</option>
              <option value="高二">高二</option>
              <option value="高三">高三</option>
              <option value="初一">初一</option>
              <option value="初二">初二</option>
              <option value="初三">初三</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">班号 <span class="text-rose-500">*</span></label>
            <input v-model="editClassData.classNo" type="number" min="1" placeholder="例如：1" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
          </div>
        </div>
        <div v-if="editClassData.grade && editClassData.classNo" class="bg-sky-50 border border-sky-200 rounded-2xl p-3 text-sm">
          <span class="text-stone-500">将更新为：</span>
          <span class="font-bold text-stone-800">{{ buildClassName(editClassData.grade, editClassData.classNo) }}</span>
          <span class="text-stone-400 ml-2">| 编码 {{ gradeToCode(editClassData.grade) }} / {{ classNoToCode(editClassData.classNo) }}</span>
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">班主任</label>
          <input v-model="editClassData.head_teacher" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">描述</label>
          <textarea v-model="editClassData.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showEditClassDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="updateClass" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 11: 添加商品 -->
    <BaseModal
      :show="showAddProductDialog"
      title="添加商品"
      icon="fa-shopping-cart"
      icon-from="#fb7185"
      icon-to="#be123c"
      @close="showAddProductDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">商品名称</label>
          <input v-model="newProduct.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">商品描述</label>
          <textarea v-model="newProduct.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">所需积分</label>
          <input v-model="newProduct.cost" type="number" min="1" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">库存数量</label>
          <input v-model="newProduct.stock" type="number" min="0" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">商品状态</label>
          <select v-model="newProduct.status" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            <option :value="1">上架</option>
            <option :value="0">下架</option>
          </select>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showAddProductDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="addProduct" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- Modal 12: 编辑商品 -->
    <BaseModal
      :show="showEditProductDialog"
      title="编辑商品"
      icon="fa-pen-to-square"
      icon-from="#7dd3fc"
      icon-to="#0369a1"
      @close="showEditProductDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">商品名称</label>
          <input v-model="editProduct.name" type="text" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">商品描述</label>
          <textarea v-model="editProduct.description" class="field-input w-full px-4 py-3 rounded-2xl text-sm" rows="3"></textarea>
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">所需积分</label>
          <input v-model="editProduct.cost" type="number" min="1" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">库存数量</label>
          <input v-model="editProduct.stock" type="number" min="0" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
        </div>
        <div>
          <label class="block text-xs font-bold text-stone-500 uppercase tracking-wider mb-2">商品状态</label>
          <select v-model="editProduct.status" class="field-input w-full px-4 py-3 rounded-2xl text-sm">
            <option :value="1">上架</option>
            <option :value="0">下架</option>
          </select>
        </div>
      </div>
      <template #footer>
        <button type="button" @click="showEditProductDialog = false" class="btn-ghost px-5 py-2.5 rounded-2xl text-sm font-medium">取消</button>
        <button type="button" @click="updateProduct" class="btn-ruby px-5 py-2.5 rounded-2xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>
  </AppLayout>
</template>

<style scoped>
/* ===== 主按钮：rose 渐变（管理员主题） ===== */
.btn-ruby {
  background: linear-gradient(135deg, #e11d48 0%, #9f1239 100%);
  color: #fff;
  box-shadow:
    0 8px 20px -8px rgba(190, 18, 60, 0.55),
    inset 0 1px 0 rgba(255, 255, 255, 0.25);
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}
.btn-ruby:hover {
  transform: translateY(-2px);
  box-shadow:
    0 12px 28px -8px rgba(190, 18, 60, 0.65),
    inset 0 1px 0 rgba(255, 255, 255, 0.35);
}
.btn-ruby:active { transform: translateY(0); }
.btn-ruby:disabled { opacity: 0.5; cursor: not-allowed; transform: none; }

/* ===== 次按钮：ghost ===== */
.btn-ghost {
  background: rgba(255, 255, 255, 0.6);
  backdrop-filter: blur(8px);
  border: 1px solid rgba(120, 113, 108, 0.25);
  color: #44403c;
  transition: all 0.25s ease;
}
.btn-ghost:hover {
  background: rgba(255, 255, 255, 0.9);
  border-color: rgba(190, 18, 60, 0.4);
  color: #be123c;
}
.btn-ghost:disabled { opacity: 0.5; cursor: not-allowed; }

/* ===== 输入框 ===== */
.field-input {
  background: rgba(255, 255, 255, 0.7);
  backdrop-filter: blur(6px);
  border: 1px solid rgba(120, 113, 108, 0.22);
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  color: #44403c;
}
.field-input:focus {
  background: rgba(255, 255, 255, 0.98);
  border-color: #be123c;
  box-shadow: 0 0 0 4px rgba(190, 18, 60, 0.12);
  outline: none;
}

/* ===== 表格行 ===== */
.table-row { transition: background 0.25s ease; }
.table-row:hover {
  background: linear-gradient(90deg, rgba(190, 18, 60, 0.04) 0%, rgba(225, 29, 72, 0.06) 100%);
}

/* ===== 进度条 ===== */
.progress-track {
  background: rgba(120, 113, 108, 0.15);
  border-radius: 9999px;
  overflow: hidden;
}
.progress-fill {
  height: 100%;
  border-radius: 9999px;
  transition: width 0.6s cubic-bezier(0.16, 1, 0.3, 1);
}

/* ===== 角色卡片 ===== */
.role-card {
  background: rgba(255, 255, 255, 0.78);
  backdrop-filter: blur(14px);
  border: 1px solid rgba(255, 255, 255, 0.6);
  box-shadow: 0 10px 30px -12px rgba(28, 20, 16, 0.12);
  transition: all 0.4s cubic-bezier(0.16, 1, 0.3, 1);
}
.role-card:hover {
  transform: translateY(-6px);
  box-shadow: 0 22px 44px -14px rgba(190, 18, 60, 0.22);
}

/* ===== 单选卡片 ===== */
.radio-card {
  transition: all 0.25s ease;
  cursor: pointer;
}
.radio-card.active {
  border-color: #be123c;
  background: linear-gradient(135deg, rgba(190, 18, 60, 0.06), rgba(225, 29, 72, 0.04));
  box-shadow: 0 0 0 3px rgba(190, 18, 60, 0.1);
}

/* ===== 徽章 ===== */
.badge {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.7rem;
  border-radius: 9999px;
  font-size: 0.72rem;
  font-weight: 600;
  letter-spacing: 0.02em;
}

/* ===== 标题装饰下划线 ===== */
.title-deco {
  position: relative;
  display: inline-block;
}
.title-deco::after {
  content: '';
  position: absolute;
  left: 0;
  bottom: -8px;
  width: 40%;
  height: 3px;
  border-radius: 9999px;
  background: linear-gradient(90deg, #e11d48, #d97706);
}

/* ===== 入场动画延时（全局仅定义到 enter-delay-4） ===== */
.enter-delay-5 { animation-delay: 0.36s; }
.enter-delay-6 { animation-delay: 0.44s; }
.enter-delay-7 { animation-delay: 0.52s; }
.enter-delay-8 { animation-delay: 0.60s; }
</style>
