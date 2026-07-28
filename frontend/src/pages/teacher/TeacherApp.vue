<script setup lang="ts">
/**
 * 教师工作台 —— 迁移自 teacher.html
 *
 * 单 SFC + v-show 切换 7 个 tab：工作台 / 学生管理 / 积分管理 / 学生评价 /
 * 数据统计 / 家长留言 / 兑换记录
 * 5 个 BaseModal：添加学生 / 编辑学生 / 积分操作 / 学生评价 / 批量导入
 * 3 个 BaseChart（统计 tab）：班级积分柱状图 / 积分趋势折线图 / 评价分布饼图
 * API 走统一 api 封装，alert/confirm → toast/confirmDialog
 */
import { ref, computed, watch, onMounted, nextTick } from 'vue'
import type { EChartsOption } from 'echarts'
import { api } from '../../lib/api'
import { checkAuth, logout, type UserInfo } from '../../lib/auth'
import { formatDateTime } from '../../lib/format'
import { THEMES } from '../../lib/theme'
import { TEACHER_NAV, type NavItem } from '../../lib/navConfig'
import { toast } from '../../composables/useToast'
import { confirmDialog } from '../../composables/useConfirm'
import AppLayout from '../../components/AppLayout.vue'
import BaseChart from '../../components/BaseChart.vue'
import BaseModal from '../../components/BaseModal.vue'
import EmptyState from '../../components/EmptyState.vue'
import Pagination from '../../components/Pagination.vue'
import StatCard from '../../components/StatCard.vue'

// ===== Types =====
interface Student {
  id: number | string
  studentId: string
  name: string
  className: string
  points: number
}

interface MyClass {
  id: number | string
  name: string
}

interface PointsRecord {
  id: number | string
  studentId: number | string
  studentName: string
  className: string
  points: number
  reason: string
  time: string
  operatorName: string
}

interface EvalDimension {
  id: number
  name: string
  description: string
  scoreMax: number
}

interface Evaluation {
  id: number
  studentId: number
  dimensionId: number
  score: number
  comment: string
}

interface EvaluationForm {
  studentId: number | string
  scores: Record<number, string | number>
  comment: string
}

interface ParentMessage {
  id: number | string
  student_name: string
  class_name: string
  content: string
  created_at: string
  read_status: number
  reply_to: number | string | null
  sender_type: 'parent' | 'teacher'
}

interface Redemption {
  id: number | string
  student_name: string
  className: string
  item_name: string
  cost: number
  time: string
}

interface DashboardStats {
  todayPoints: number
  pendingEvaluations: number
}

interface RecentActivity {
  id: string
  type: 'add' | 'deduct' | 'eval'
  description: string
  points: number | string
  time: string
  sortTime: string
}

interface ClassPointItem { className?: string; name?: string; points?: number; value?: number }
interface PointsTrendItem { month?: string; name?: string; points?: number; value?: number }
interface EvalDistItem { level?: string; name?: string; count?: number; value?: number }

interface StatisticsData {
  classPoints?: ClassPointItem[]
  pointsTrend?: PointsTrendItem[]
  evaluationDistribution?: EvalDistItem[]
}

interface ImportResult {
  success?: number
  failed?: number
  errors?: string[]
}

// ===== State =====
const currentTab = ref<string>('dashboard')
const userInfo = ref<UserInfo>({
  id: '',
  username: '',
  name: '',
  role_id: 2,
})

// Dialog visibility
const showAddStudentDialog = ref(false)
const showEditStudentDialog = ref(false)
const showAddPointsDialog = ref(false)
const showAddEvaluationDialog = ref(false)
const showImportDialog = ref(false)

// Student list
const studentSearch = ref('')
const studentClassFilter = ref('')
const currentPage = ref(1)
const pageSize = 10
const students = ref<Student[]>([])
const myClasses = ref<MyClass[]>([])

const newStudent = ref<{ studentId: string; name: string; className: string; points: number | string }>({
  studentId: '', name: '', className: '', points: 0,
})
const editStudentData = ref<{ id: number | string; studentId: string; name: string; className: string; points: number | string }>({
  id: 0, studentId: '', name: '', className: '', points: 0,
})

// Points
const pointsRecords = ref<PointsRecord[]>([])
const pointsOperation = ref<{ studentId: number | string; type: 'add' | 'deduct'; points: number | string; reason: string }>({
  studentId: '', type: 'add', points: 0, reason: '',
})

// Evaluation
const evaluationDimensions: EvalDimension[] = [
  { id: 1, name: '德育', description: '思想品德和道德素养', scoreMax: 100 },
  { id: 2, name: '智育', description: '学习成绩和学习能力', scoreMax: 100 },
  { id: 3, name: '体育', description: '体育锻炼和健康状况', scoreMax: 100 },
  { id: 4, name: '美育', description: '艺术素养和审美能力', scoreMax: 100 },
  { id: 5, name: '劳育', description: '劳动技能和实践能力', scoreMax: 100 },
]
const evaluations = ref<Evaluation[]>([
  { id: 1, studentId: 1, dimensionId: 1, score: 85, comment: '表现良好' },
  { id: 2, studentId: 1, dimensionId: 2, score: 90, comment: '学习认真' },
])
const evaluation = ref<EvaluationForm>({ studentId: '', scores: {}, comment: '' })

// Dashboard
const dashboardStats = ref<DashboardStats>({ todayPoints: 0, pendingEvaluations: 0 })
const recentActivities = ref<RecentActivity[]>([])

// Statistics
const statisticsData = ref<StatisticsData>({})
const classPointsChartRef = ref<InstanceType<typeof BaseChart>>()
const pointsTrendChartRef = ref<InstanceType<typeof BaseChart>>()
const evaluationChartRef = ref<InstanceType<typeof BaseChart>>()

// Parent messages
const parentMessages = ref<ParentMessage[]>([])
const unreadCount = ref(0)
const replyingTo = ref<number | string | null>(null)
const replyContent = ref('')

// Redemptions
const redemptions = ref<Redemption[]>([])

// CSV import file input
const csvFileInput = ref<HTMLInputElement>()

// ===== Computed =====
const navItems = computed<NavItem[]>(() =>
  TEACHER_NAV.map(item =>
    item.key === 'parent-messages'
      ? { ...item, badge: unreadCount.value }
      : item
  )
)

const filteredStudentsList = computed(() =>
  students.value.filter(student => {
    const matchesSearch =
      student.name.toLowerCase().includes(studentSearch.value.toLowerCase()) ||
      student.studentId.toLowerCase().includes(studentSearch.value.toLowerCase())
    const matchesClass = !studentClassFilter.value || student.className === studentClassFilter.value
    return matchesSearch && matchesClass
  })
)

const filteredStudents = computed(() => {
  const start = (currentPage.value - 1) * pageSize
  return filteredStudentsList.value.slice(start, start + pageSize)
})

const totalPages = computed(() => Math.ceil(filteredStudentsList.value.length / pageSize) || 1)
const startIndex = computed(() => filteredStudentsList.value.length === 0 ? 0 : (currentPage.value - 1) * pageSize + 1)
const endIndex = computed(() => Math.min(currentPage.value * pageSize, filteredStudentsList.value.length))

// ===== Chart options =====
const classPointsOption = computed<EChartsOption>(() => {
  const classPoints = statisticsData.value.classPoints || []
  const names = classPoints.map(item => item.className || item.name || '')
  const values = classPoints.map(item => item.points != null ? item.points : (item.value || 0))
  return {
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
    grid: { left: '3%', right: '4%', bottom: '3%', containLabel: true },
    xAxis: { type: 'category', data: names, axisLabel: { interval: 0, rotate: classPoints.length > 4 ? 30 : 0 } },
    yAxis: { type: 'value' },
    series: [{ name: '总积分', type: 'bar', data: values, itemStyle: { color: '#6366f1', borderRadius: [6, 6, 0, 0] } }],
  }
})

const pointsTrendOption = computed<EChartsOption>(() => {
  const pointsTrend = statisticsData.value.pointsTrend || []
  const names = pointsTrend.map(item => item.month || item.name || '')
  const values = pointsTrend.map(item => item.points != null ? item.points : (item.value || 0))
  return {
    tooltip: { trigger: 'axis' },
    grid: { left: '3%', right: '4%', bottom: '3%', containLabel: true },
    xAxis: { type: 'category', boundaryGap: false, data: names },
    yAxis: { type: 'value' },
    series: [{ name: '积分', type: 'line', data: values, smooth: true, itemStyle: { color: '#10b981' }, areaStyle: { color: 'rgba(16, 185, 129, 0.15)' } }],
  }
})

const evaluationChartOption = computed<EChartsOption>(() => {
  const evaluationDistribution = statisticsData.value.evaluationDistribution || []
  const data = evaluationDistribution.map(item => ({
    name: item.level || item.name || '',
    value: item.count != null ? item.count : (item.value || 0),
  }))
  return {
    tooltip: { trigger: 'item', formatter: '{b}: {c} ({d}%)' },
    legend: { top: '5%', left: 'center' },
    color: ['#10b981', '#34d399', '#fbbf24', '#f59e0b'],
    series: [{
      name: '评价分布',
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
watch(studentSearch, () => { currentPage.value = 1 })
watch(studentClassFilter, () => { currentPage.value = 1 })

// ===== API Methods =====
async function loadStudents() {
  try {
    const res = await api.get<Student[]>('/api/teacher/students')
    if (res.code === 200) {
      students.value = res.data || []
    }
  } catch (e) {
    console.error('Load students error:', e)
  }
}

async function loadMyClasses() {
  try {
    const res = await api.get<MyClass[]>('/api/teacher/my-classes')
    if (res.code === 200) {
      myClasses.value = res.data || []
    }
  } catch (e) {
    console.error('Load my classes error:', e)
  }
}

async function loadPointsRecords() {
  try {
    const res = await api.get<PointsRecord[]>('/api/teacher/points/records')
    if (res.code === 200) {
      pointsRecords.value = res.data || []
    }
  } catch (e) {
    console.error('Load points records error:', e)
  }
}

async function loadEvaluations() {
  // 原始代码调用 /api/teacher/evaluation/dimensions 但赋值到不存在的 this.dimensions，
  // 实际未使用返回值。此处保留 API 调用以维持网络行为。
  try {
    await api.get('/api/teacher/evaluation/dimensions')
  } catch (e) {
    console.error('Load evaluations error:', e)
  }
}

async function loadDashboard() {
  try {
    const res = await api.get<DashboardStats>('/api/teacher/dashboard')
    if (res.code === 200) {
      dashboardStats.value = res.data || { todayPoints: 0, pendingEvaluations: 0 }
    }
  } catch (e) {
    console.error('Load dashboard error:', e)
  }
}

async function loadRecentActivities() {
  try {
    const [pointsRes, redemptionRes] = await Promise.all([
      api.get<PointsRecord[]>('/api/teacher/points/records'),
      api.get<Redemption[]>('/api/teacher/redemptions'),
    ])
    const activities: RecentActivity[] = []
    if (pointsRes.code === 200 && pointsRes.data) {
      pointsRes.data.forEach(record => {
        activities.push({
          id: 'p' + record.id,
          type: record.points > 0 ? 'add' : 'deduct',
          description: `${record.studentName} - ${record.reason}`,
          points: Math.abs(record.points),
          time: record.time,
          sortTime: record.time,
        })
      })
    }
    if (redemptionRes.code === 200 && redemptionRes.data) {
      redemptionRes.data.forEach(record => {
        activities.push({
          id: 'r' + record.id,
          type: 'deduct',
          description: `${record.student_name} - 兑换 ${record.item_name}`,
          points: record.cost,
          time: record.time,
          sortTime: record.time,
        })
      })
    }
    activities.sort((a, b) => (b.sortTime || '').localeCompare(a.sortTime || ''))
    recentActivities.value = activities.slice(0, 10)
  } catch (e) {
    console.error('Load recent activities error:', e)
  }
}

async function loadStatistics() {
  try {
    const res = await api.get<StatisticsData>('/api/teacher/statistics')
    if (res.code === 200) {
      statisticsData.value = res.data || {}
    }
  } catch (e) {
    console.error('Load statistics error:', e)
  }
}

async function loadParentMessages() {
  try {
    const res = await api.get<ParentMessage[]>('/api/teacher/parent-messages')
    if (res.code === 200) {
      parentMessages.value = res.data || []
      unreadCount.value = parentMessages.value.filter(m => m.read_status === 0).length
    }
  } catch (e) {
    console.error('Load parent messages error:', e)
  }
}

async function loadRedemptions() {
  try {
    const res = await api.get<Redemption[]>('/api/teacher/redemptions')
    if (res.code === 200) {
      redemptions.value = res.data || []
    }
  } catch (e) {
    console.error('Load redemptions error:', e)
  }
}

// ===== Parent message actions =====
function getReplies(messageId: number | string): ParentMessage[] {
  return parentMessages.value
    .filter(m => m.reply_to === messageId && m.sender_type === 'teacher')
    .sort((a, b) => new Date(a.created_at).getTime() - new Date(b.created_at).getTime())
}

function replyMessage(id: number | string) {
  replyingTo.value = id
  replyContent.value = ''
}

function cancelReply() {
  replyingTo.value = null
  replyContent.value = ''
}

async function submitReply() {
  if (!replyContent.value.trim()) {
    toast.warning('请输入回复内容')
    return
  }
  try {
    const res = await api.post(`/api/teacher/parent-messages/${replyingTo.value}/reply`, {
      content: replyContent.value,
    })
    if (res.code === 200) {
      replyingTo.value = null
      replyContent.value = ''
      await loadParentMessages()
      toast.success('回复成功！')
    } else {
      toast.error(res.msg || '回复失败')
    }
  } catch (e) {
    console.error('Reply message error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function markAsRead(message: ParentMessage) {
  if (message.read_status !== 0) return
  if (replyingTo.value === message.id) return
  try {
    const res = await api.put(`/api/teacher/parent-messages/${message.id}/read`)
    if (res.code === 200) {
      message.read_status = 1
      unreadCount.value = parentMessages.value.filter(m => m.read_status === 0).length
    }
  } catch (e) {
    console.error('Mark as read error:', e)
  }
}

// ===== Student actions =====
async function addStudent() {
  if (!newStudent.value.studentId || !newStudent.value.name) {
    toast.warning('请填写完整信息')
    return
  }
  try {
    const res = await api.post('/api/teacher/students', {
      studentId: newStudent.value.studentId,
      name: newStudent.value.name,
      className: newStudent.value.className,
      points: parseInt(String(newStudent.value.points)) || 0,
    })
    if (res.code === 200) {
      await loadStudents()
      showAddStudentDialog.value = false
      newStudent.value = { studentId: '', name: '', className: '', points: 0 }
      toast.success('学生添加成功！')
    } else {
      toast.error(res.msg || '添加失败')
    }
  } catch (e) {
    console.error('Add student error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function editStudent(student: Student) {
  editStudentData.value = {
    id: student.id,
    studentId: student.studentId,
    name: student.name,
    className: student.className,
    points: student.points,
  }
  showEditStudentDialog.value = true
}

async function updateStudent() {
  if (!editStudentData.value.studentId || !editStudentData.value.name) {
    toast.warning('学号和姓名不能为空')
    return
  }
  try {
    const res = await api.put(`/api/teacher/students/${editStudentData.value.id}`, {
      studentId: editStudentData.value.studentId,
      name: editStudentData.value.name,
      className: editStudentData.value.className,
      points: parseInt(String(editStudentData.value.points)) || 0,
    })
    if (res.code === 200) {
      await loadStudents()
      showEditStudentDialog.value = false
      toast.success('学生信息更新成功！')
    } else {
      toast.error(res.msg || '更新失败')
    }
  } catch (e) {
    console.error('Update student error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function deleteStudent(student: Student) {
  const ok = await confirmDialog({
    message: `确定要删除学生 ${student.name} 吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    const res = await api.delete('/api/teacher/students', { id: student.id })
    if (res.code === 200) {
      await loadStudents()
      toast.success('删除成功！')
    } else {
      toast.error(res.msg || '删除失败')
    }
  } catch (e) {
    console.error('Delete student error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

async function importStudentCsv() {
  const fileInput = csvFileInput.value
  if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
    toast.warning('请选择 CSV 文件')
    return
  }
  const file = fileInput.files[0]
  try {
    const csv_data = await file.text()
    const res = await api.post<ImportResult>('/api/teacher/students/import', { csv_data })
    if (res.code === 200) {
      const result = res.data || {}
      const success = result.success || 0
      const failed = result.failed || 0
      const errors = result.errors || []
      let msg = `导入完成：成功 ${success} 条，失败 ${failed} 条`
      if (errors.length > 0) {
        msg += '\n错误详情：\n' + errors.join('\n')
      }
      toast.info(msg)
      if (success > 0) {
        await loadStudents()
        showImportDialog.value = false
        fileInput.value = ''
      }
    } else {
      toast.error(res.msg || '导入失败')
    }
  } catch (e) {
    console.error('Import students error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function downloadCsvTemplate() {
  const csvContent = '学号,姓名,班级,初始密码\n2024001,测试学生,高二(1)班,123456\n'
  const blob = new Blob(['\ufeff' + csvContent], { type: 'text/csv;charset=utf-8;' })
  const link = document.createElement('a')
  link.href = URL.createObjectURL(blob)
  link.download = '学生导入模板.csv'
  document.body.appendChild(link)
  link.click()
  document.body.removeChild(link)
}

// ===== Points actions =====
function manageStudentPoints(student: Student) {
  pointsOperation.value.studentId = student.id
  showAddPointsDialog.value = true
}

function searchStudents() {
  currentPage.value = 1
}

async function operatePoints() {
  if (!pointsOperation.value.studentId || !pointsOperation.value.points || !pointsOperation.value.reason) {
    toast.warning('请填写完整信息')
    return
  }
  try {
    const res = await api.post('/api/teacher/points', {
      studentId: pointsOperation.value.studentId,
      points: parseInt(String(pointsOperation.value.points)),
      type: pointsOperation.value.type,
      reason: pointsOperation.value.reason,
    })
    if (res.code === 200) {
      await loadStudents()
      await loadPointsRecords()
      showAddPointsDialog.value = false
      pointsOperation.value = { studentId: '', type: 'add', points: 0, reason: '' }
      toast.success('积分操作成功！')
    } else {
      toast.error(res.msg || '操作失败')
    }
  } catch (e) {
    console.error('Points operation error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

// ===== Evaluation actions =====
function evaluateStudent(student: Student) {
  evaluation.value.studentId = student.id
  evaluation.value.scores = {}
  evaluation.value.comment = ''
  showAddEvaluationDialog.value = true
}

function getStudentEvaluation(studentId: number | string, dimensionId: number): number | null {
  const ev = evaluations.value.find(e => e.studentId === studentId && e.dimensionId === dimensionId)
  return ev ? ev.score : null
}

async function submitEvaluation() {
  if (!evaluation.value.studentId) {
    toast.warning('请选择学生')
    return
  }
  try {
    const res = await api.post('/api/teacher/evaluation', {
      studentId: evaluation.value.studentId,
      scores: evaluation.value.scores,
      comment: evaluation.value.comment,
    })
    if (res.code === 200) {
      await loadEvaluations()
      showAddEvaluationDialog.value = false
      evaluation.value = { studentId: '', scores: {}, comment: '' }
      toast.success('评价提交成功！')
    } else {
      toast.error(res.msg || '提交失败')
    }
  } catch (e) {
    console.error('Evaluation submit error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

function editEvaluation(student: Student) {
  toast.info('编辑评价功能开发中...')
}

async function deleteEvaluation(student: Student) {
  const ok = await confirmDialog({
    message: `确定要删除学生 ${student.name} 的评价吗？`,
    variant: 'danger',
    confirmText: '确认删除',
  })
  if (!ok) return
  try {
    // 这里需要根据实际的评价ID来调用删除API
    // 暂时使用模拟数据
    toast.info('删除评价功能开发中...')
  } catch (e) {
    console.error('Delete evaluation error:', e)
    toast.error('网络错误，请稍后重试')
  }
}

// ===== Pagination =====
function changePage(page: number) {
  currentPage.value = page
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
    loadRecentActivities()
  } else if (tab === 'students') {
    loadStudents()
  } else if (tab === 'points') {
    loadPointsRecords()
  } else if (tab === 'evaluation') {
    loadEvaluations()
  } else if (tab === 'statistics') {
    loadStatistics().then(() => {
      nextTick(() => {
        classPointsChartRef.value?.resize()
        pointsTrendChartRef.value?.resize()
        evaluationChartRef.value?.resize()
      })
    })
  } else if (tab === 'parent-messages') {
    loadParentMessages()
  } else if (tab === 'redemptions') {
    loadRedemptions()
  }
}

// ===== Lifecycle =====
onMounted(async () => {
  const user = await checkAuth(2)
  if (!user) return
  userInfo.value = { ...user }
  await loadMyClasses()
  await loadStudents()
  await loadPointsRecords()
  await loadEvaluations()
  await loadDashboard()
  await loadRecentActivities()
  await loadParentMessages()
})
</script>

<template>
  <AppLayout
    :theme="THEMES.teal"
    brand="教师工作台"
    brand-en="Teacher"
    brand-icon="fa-chalkboard-teacher"
    :nav-items="navItems"
    :current-tab="currentTab"
    :user-info="userInfo"
    sidebar-width="w-64"
    @switch-tab="switchTab"
  >
    <!-- ====== Tab 1: 工作台 ====== -->
    <div v-show="currentTab === 'dashboard'">
      <div class="mb-6 fade-up">
        <p class="text-sm text-stone-500 mb-1">欢迎回来，{{ userInfo.name || '老师' }} 👋</p>
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">今日校园概览</h2>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6">
        <StatCard
          label="学生总数"
          :value="students.length"
          icon="fa-users"
          gradient-from="#ccfbf1"
          gradient-to="#5eead4"
          sublabel="在读学生"
          :delay="1"
          variant="lift-card"
        />
        <StatCard
          label="今日积分变动"
          :value="dashboardStats.todayPoints"
          icon="fa-coins"
          gradient-from="#d1fae5"
          gradient-to="#6ee7b7"
          sublabel="实时统计"
          :delay="2"
          variant="lift-card"
        />
        <StatCard
          label="待评价学生"
          :value="dashboardStats.pendingEvaluations"
          icon="fa-star"
          gradient-from="#fef3c7"
          gradient-to="#fcd34d"
          sublabel="待处理事项"
          :delay="3"
          variant="lift-card"
        />
      </div>

      <div class="glass rounded-3xl p-6 fade-up delay-4" style="box-shadow: 0 4px 20px -8px rgba(13, 148, 136, 0.12);">
        <div class="flex items-center justify-between mb-5">
          <div class="flex items-center gap-3">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #ccfbf1, #99f6e4); color: #0d9488;">
              <i class="fa-solid fa-clock-rotate-left text-sm"></i>
            </div>
            <h3 class="font-display text-xl font-bold text-stone-800">最近操作记录</h3>
          </div>
          <span class="chip" style="background: rgba(20, 184, 166, 0.1); color: #0d9488;">实时</span>
        </div>
        <EmptyState v-if="recentActivities.length === 0" icon="fa-inbox" text="暂无记录" variant="inline" />
        <div v-else class="space-y-2">
          <div
            v-for="activity in recentActivities"
            :key="activity.id"
            class="flex items-center group hover:bg-white/60 p-3 -mx-1 rounded-2xl smooth-trans"
          >
            <div
              :class="activity.type === 'add' ? 'text-emerald-600' : (activity.type === 'deduct' ? 'text-rose-500' : 'text-teal-600')"
              class="w-10 h-10 rounded-xl flex items-center justify-center shrink-0"
              :style="activity.type === 'add' ? 'background: linear-gradient(135deg, #d1fae5, #a7f3d0);' : (activity.type === 'deduct' ? 'background: linear-gradient(135deg, #fee2e2, #fecaca);' : 'background: linear-gradient(135deg, #ccfbf1, #99f6e4);')"
            >
              <i :class="activity.type === 'add' ? 'fa-solid fa-plus' : (activity.type === 'deduct' ? 'fa-solid fa-minus' : 'fa-solid fa-star')"></i>
            </div>
            <div class="ml-4 flex-1 min-w-0">
              <p class="text-sm font-semibold text-stone-800 truncate">{{ activity.description }}</p>
              <p class="text-xs text-stone-400 mt-0.5">{{ activity.time }}</p>
            </div>
            <span
              :class="activity.type === 'add' ? 'text-emerald-600' : (activity.type === 'deduct' ? 'text-rose-500' : 'text-teal-600')"
              class="font-display font-bold text-lg"
            >
              {{ activity.type === 'add' ? '+' : (activity.type === 'deduct' ? '-' : '') }}{{ activity.points || '' }}
            </span>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 2: 学生管理 ====== -->
    <div v-show="currentTab === 'students'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-center gap-4 mb-6 fade-up">
        <div>
          <p class="text-sm text-stone-500 mb-1">管理班级学生信息</p>
          <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">学生管理</h2>
        </div>
        <div class="flex space-x-2">
          <button @click="showImportDialog = true" class="btn-ghost px-4 py-2.5 rounded-xl text-sm font-medium flex items-center">
            <i class="fa-solid fa-file-import mr-2"></i> 批量导入
          </button>
          <button @click="showAddStudentDialog = true" class="btn-teal px-4 py-2.5 rounded-xl text-sm font-medium flex items-center">
            <i class="fa-solid fa-plus mr-2"></i> 添加学生
          </button>
        </div>
      </div>

      <div class="glass rounded-3xl p-4 mb-6 fade-up delay-1">
        <div class="flex flex-col md:flex-row gap-3">
          <div class="flex-1 relative">
            <i class="fa-solid fa-magnifying-glass absolute left-4 top-1/2 -translate-y-1/2 text-stone-400 text-sm"></i>
            <input v-model="studentSearch" type="text" placeholder="搜索学生姓名或学号" class="input-soft w-full pl-11 pr-4 py-2.5 text-sm">
          </div>
          <div class="md:w-48">
            <select v-model="studentClassFilter" class="input-soft w-full px-4 py-2.5 text-sm">
              <option value="">所有班级</option>
              <option v-for="cls in myClasses" :key="cls.id" :value="cls.name">{{ cls.name }}</option>
            </select>
          </div>
          <button @click="searchStudents" class="btn-teal px-5 py-2.5 rounded-xl text-sm font-medium flex items-center justify-center">
            <i class="fa-solid fa-search mr-2"></i> 搜索
          </button>
        </div>
      </div>

      <div class="glass rounded-3xl overflow-hidden fade-up delay-2">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead>
              <tr class="border-b border-stone-200/60" style="background: linear-gradient(90deg, rgba(20, 184, 166, 0.05), rgba(245, 158, 11, 0.03));">
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">学号</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">姓名</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">班级</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">积分</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-200/50">
              <EmptyState
                v-if="filteredStudents.length === 0"
                icon="fa-folder-open"
                text="未找到匹配的学生记录"
                variant="table-row"
                :colspan="5"
              />
              <tr v-for="student in filteredStudents" :key="student.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-semibold text-stone-800 font-display">{{ student.studentId }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-700">
                  <div class="flex items-center gap-2.5">
                    <div class="w-8 h-8 rounded-lg flex items-center justify-center text-xs font-bold text-white" style="background: linear-gradient(135deg, #2dd4bf, #0d9488);">{{ (student.name || '?').charAt(0) }}</div>
                    {{ student.name }}
                  </div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">
                  <span class="chip" style="background: rgba(245, 158, 11, 0.12); color: #b45309;">{{ student.className }}</span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-bold font-display" style="color: #0d9488;">{{ student.points }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <div class="flex items-center gap-1">
                    <button @click="editStudent(student)" class="w-8 h-8 rounded-lg flex items-center justify-center text-stone-500 hover:text-teal-600 hover:bg-teal-50 smooth-trans" title="编辑">
                      <i class="fa-solid fa-pen-to-square text-xs"></i>
                    </button>
                    <button @click="deleteStudent(student)" class="w-8 h-8 rounded-lg flex items-center justify-center text-stone-500 hover:text-rose-500 hover:bg-rose-50 smooth-trans" title="删除">
                      <i class="fa-solid fa-trash text-xs"></i>
                    </button>
                    <button @click="manageStudentPoints(student)" class="px-3 h-8 rounded-lg flex items-center text-xs font-medium text-white smooth-trans" style="background: linear-gradient(135deg, #14b8a6, #0d9488);" title="积分操作">
                      <i class="fa-solid fa-coins mr-1"></i> 积分
                    </button>
                  </div>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
        <div class="px-6 py-4 border-t border-stone-200/60" style="background: rgba(250, 246, 239, 0.5);">
          <Pagination
            :current-page="currentPage"
            :total-pages="totalPages"
            :total="filteredStudentsList.length"
            :start-index="startIndex"
            :end-index="endIndex"
            @change="changePage"
          />
        </div>
      </div>
    </div>

    <!-- ====== Tab 3: 积分管理 ====== -->
    <div v-show="currentTab === 'points'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-center gap-4 mb-6 fade-up">
        <div>
          <p class="text-sm text-stone-500 mb-1">查看与操作学生积分</p>
          <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">积分管理</h2>
        </div>
        <button @click="showAddPointsDialog = true" class="btn-teal px-4 py-2.5 rounded-xl text-sm font-medium flex items-center">
          <i class="fa-solid fa-plus mr-2"></i> 操作积分
        </button>
      </div>
      <div class="glass rounded-3xl overflow-hidden fade-up delay-1">
        <div class="px-6 py-4 border-b border-stone-200/60 flex items-center justify-between">
          <div class="flex items-center gap-3">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #e0e7ff, #c7d2fe); color: #4338ca;">
              <i class="fa-solid fa-coins text-sm"></i>
            </div>
            <div>
              <h3 class="font-display text-lg font-bold text-stone-800">积分记录</h3>
              <p class="text-xs text-stone-500">每一次能量变动都在这里</p>
            </div>
          </div>
          <button @click="loadPointsRecords" class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-1.5">
            <i class="fa-solid fa-rotate-right"></i>刷新
          </button>
        </div>
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead>
              <tr class="border-b border-stone-200/60" style="background: linear-gradient(90deg, rgba(20, 184, 166, 0.05), rgba(245, 158, 11, 0.03));">
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">学生</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">积分变动</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">原因</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">操作时间</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">操作人</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-200/50">
              <EmptyState
                v-if="pointsRecords.length === 0"
                icon="fa-folder-open"
                text="暂无积分记录"
                variant="table-row"
                :colspan="5"
              />
              <tr v-for="record in pointsRecords" :key="record.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-800">
                  <div class="flex items-center gap-2.5">
                    <div class="w-8 h-8 rounded-lg flex items-center justify-center text-xs font-bold text-white" style="background: linear-gradient(135deg, #2dd4bf, #0d9488);">{{ (record.studentName || '?').charAt(0) }}</div>
                    <div>
                      <div class="font-semibold">{{ record.studentName }}</div>
                      <div class="text-xs text-stone-500">{{ record.className }}</div>
                    </div>
                  </div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <span class="chip font-display font-bold text-sm" :style="record.points > 0 ? 'background: rgba(16, 185, 129, 0.12); color: #047857;' : 'background: rgba(244, 63, 94, 0.12); color: #be123c;'">
                    {{ record.points > 0 ? '+' : '' }}{{ record.points }}
                  </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ record.reason }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-xs text-stone-500">{{ record.time }}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">{{ record.operatorName }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Tab 4: 学生评价 ====== -->
    <div v-show="currentTab === 'evaluation'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-center gap-4 mb-6 fade-up">
        <div>
          <p class="text-sm text-stone-500 mb-1">五育并举 · 全面发展评价</p>
          <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">学生评价</h2>
        </div>
        <button @click="showAddEvaluationDialog = true" class="btn-teal px-4 py-2.5 rounded-xl text-sm font-medium flex items-center">
          <i class="fa-solid fa-plus mr-2"></i> 添加评价
        </button>
      </div>
      <div class="grid grid-cols-2 md:grid-cols-5 gap-3 mb-6">
        <div v-for="(dimension, idx) in evaluationDimensions" :key="dimension.id" class="glass lift-card rounded-3xl p-4 text-center fade-up" :class="'delay-' + (idx + 1)">
          <div class="w-10 h-10 mx-auto rounded-xl flex items-center justify-center mb-2 font-display font-bold text-white" style="background: linear-gradient(135deg, #14b8a6, #0d9488);">{{ dimension.name.charAt(0) }}</div>
          <div class="font-display text-xl font-bold text-stone-800">{{ dimension.name }}</div>
          <div class="text-[11px] text-stone-500 mt-1 leading-tight">{{ dimension.description }}</div>
        </div>
      </div>
      <div class="glass rounded-3xl overflow-hidden fade-up delay-2">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead>
              <tr class="border-b border-stone-200/60" style="background: linear-gradient(90deg, rgba(20, 184, 166, 0.05), rgba(245, 158, 11, 0.03));">
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">学生</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">德育</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">智育</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">体育</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">美育</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">劳育</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-200/50">
              <tr v-for="student in students" :key="student.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-800">
                  <div class="flex items-center gap-2.5">
                    <div class="w-8 h-8 rounded-lg flex items-center justify-center text-xs font-bold text-white" style="background: linear-gradient(135deg, #2dd4bf, #0d9488);">{{ (student.name || '?').charAt(0) }}</div>
                    <div>
                      <div class="font-semibold">{{ student.name }}</div>
                      <div class="text-xs text-stone-500">{{ student.className }}</div>
                    </div>
                  </div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">
                  <div v-if="getStudentEvaluation(student.id, 1)" class="flex items-center">
                    <div class="eval-bar w-16">
                      <span :style="{ width: (getStudentEvaluation(student.id, 1) as number * 100 / 100) + '%', background: 'linear-gradient(90deg, #14b8a6, #2dd4bf)' }"></span>
                    </div>
                    <span class="ml-2 font-display font-semibold text-stone-700">{{ getStudentEvaluation(student.id, 1) }}</span>
                  </div>
                  <div v-else class="text-stone-400 text-xs">未评价</div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">
                  <div v-if="getStudentEvaluation(student.id, 2)" class="flex items-center">
                    <div class="eval-bar w-16">
                      <span :style="{ width: (getStudentEvaluation(student.id, 2) as number * 100 / 100) + '%', background: 'linear-gradient(90deg, #10b981, #34d399)' }"></span>
                    </div>
                    <span class="ml-2 font-display font-semibold text-stone-700">{{ getStudentEvaluation(student.id, 2) }}</span>
                  </div>
                  <div v-else class="text-stone-400 text-xs">未评价</div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">
                  <div v-if="getStudentEvaluation(student.id, 3)" class="flex items-center">
                    <div class="eval-bar w-16">
                      <span :style="{ width: (getStudentEvaluation(student.id, 3) as number * 100 / 100) + '%', background: 'linear-gradient(90deg, #f59e0b, #fbbf24)' }"></span>
                    </div>
                    <span class="ml-2 font-display font-semibold text-stone-700">{{ getStudentEvaluation(student.id, 3) }}</span>
                  </div>
                  <div v-else class="text-stone-400 text-xs">未评价</div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">
                  <div v-if="getStudentEvaluation(student.id, 4)" class="flex items-center">
                    <div class="eval-bar w-16">
                      <span :style="{ width: (getStudentEvaluation(student.id, 4) as number * 100 / 100) + '%', background: 'linear-gradient(90deg, #8b5cf6, #a78bfa)' }"></span>
                    </div>
                    <span class="ml-2 font-display font-semibold text-stone-700">{{ getStudentEvaluation(student.id, 4) }}</span>
                  </div>
                  <div v-else class="text-stone-400 text-xs">未评价</div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-600">
                  <div v-if="getStudentEvaluation(student.id, 5)" class="flex items-center">
                    <div class="eval-bar w-16">
                      <span :style="{ width: (getStudentEvaluation(student.id, 5) as number * 100 / 100) + '%', background: 'linear-gradient(90deg, #f97316, #fb923c)' }"></span>
                    </div>
                    <span class="ml-2 font-display font-semibold text-stone-700">{{ getStudentEvaluation(student.id, 5) }}</span>
                  </div>
                  <div v-else class="text-stone-400 text-xs">未评价</div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                  <div class="flex items-center gap-1">
                    <button @click="evaluateStudent(student)" class="w-8 h-8 rounded-lg flex items-center justify-center text-stone-500 hover:text-teal-600 hover:bg-teal-50 smooth-trans" title="评价">
                      <i class="fa-solid fa-star text-xs"></i>
                    </button>
                    <button @click="editEvaluation(student)" class="w-8 h-8 rounded-lg flex items-center justify-center text-stone-500 hover:text-amber-600 hover:bg-amber-50 smooth-trans" title="编辑">
                      <i class="fa-solid fa-pen-to-square text-xs"></i>
                    </button>
                    <button @click="deleteEvaluation(student)" class="w-8 h-8 rounded-lg flex items-center justify-center text-stone-500 hover:text-rose-500 hover:bg-rose-50 smooth-trans" title="删除">
                      <i class="fa-solid fa-trash text-xs"></i>
                    </button>
                  </div>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Tab 5: 数据统计 ====== -->
    <div v-show="currentTab === 'statistics'">
      <div class="mb-6 fade-up">
        <p class="text-sm text-stone-500 mb-1">数据驱动 · 洞察校园动态</p>
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">数据统计</h2>
      </div>
      <div class="grid grid-cols-1 md:grid-cols-2 gap-5 mb-5">
        <div class="glass lift-card rounded-3xl p-6 fade-up delay-1">
          <div class="flex items-center gap-3 mb-4">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #ccfbf1, #99f6e4); color: #0d9488;">
              <i class="fa-solid fa-trophy text-sm"></i>
            </div>
            <h3 class="font-display text-lg font-bold text-stone-800">班级积分统计</h3>
          </div>
          <BaseChart ref="classPointsChartRef" :option="classPointsOption" height="320px" />
        </div>
        <div class="glass lift-card rounded-3xl p-6 fade-up delay-2">
          <div class="flex items-center gap-3 mb-4">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #d1fae5, #a7f3d0); color: #047857;">
              <i class="fa-solid fa-chart-line text-sm"></i>
            </div>
            <h3 class="font-display text-lg font-bold text-stone-800">积分趋势</h3>
          </div>
          <BaseChart ref="pointsTrendChartRef" :option="pointsTrendOption" height="320px" />
        </div>
      </div>
      <div class="glass lift-card rounded-3xl p-6 fade-up delay-3">
        <div class="flex items-center gap-3 mb-4">
          <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #fef3c7, #fcd34d); color: #b45309;">
            <i class="fa-solid fa-chart-pie text-sm"></i>
          </div>
          <h3 class="font-display text-lg font-bold text-stone-800">学生评价统计</h3>
        </div>
        <BaseChart ref="evaluationChartRef" :option="evaluationChartOption" height="320px" />
      </div>
    </div>

    <!-- ====== Tab 6: 家长留言 ====== -->
    <div v-show="currentTab === 'parent-messages'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-center gap-4 mb-6 fade-up">
        <div>
          <p class="text-sm text-stone-500 mb-1">家校沟通 · 共育成长</p>
          <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">家长留言</h2>
        </div>
        <button @click="loadParentMessages" class="btn-ghost px-4 py-2.5 rounded-xl text-sm font-medium flex items-center">
          <i class="fa-solid fa-rotate mr-2"></i> 刷新列表
        </button>
      </div>

      <EmptyState v-if="parentMessages.length === 0" icon="fa-comments" text="暂无家长留言" variant="card" />

      <div v-else class="space-y-4">
        <div
          v-for="message in parentMessages"
          :key="message.id"
          @click="markAsRead(message)"
          :class="message.read_status === 0 ? 'border-l-4 border-rose-400' : 'border-l-4 border-transparent'"
          class="glass lift-card rounded-3xl p-5 fade-up cursor-pointer smooth-trans"
        >
          <div class="flex items-start gap-3">
            <div class="w-10 h-10 rounded-xl flex items-center justify-center text-white shrink-0" style="background: linear-gradient(135deg, #f59e0b, #d97706);">
              <i class="fa-solid fa-user"></i>
            </div>
            <div class="flex-1 min-w-0">
              <div class="flex items-center justify-between gap-2 mb-1.5">
                <div class="flex items-center gap-2 flex-wrap">
                  <span class="font-semibold text-stone-800 text-sm">{{ message.student_name }}</span>
                  <span class="chip" style="background: rgba(245, 158, 11, 0.12); color: #b45309;">{{ message.class_name }}</span>
                  <span class="chip" style="background: rgba(20, 184, 166, 0.1); color: #0d9488;">家长</span>
                  <span v-if="message.read_status === 0" class="chip" style="background: #ef4444; color: #fff;">未读</span>
                </div>
                <span class="text-xs text-stone-400 shrink-0">{{ formatDateTime(message.created_at) }}</span>
              </div>
              <p class="text-sm text-stone-700 leading-relaxed bg-amber-50/60 rounded-xl px-3 py-2 border border-amber-100/60">{{ message.content }}</p>

              <div v-if="getReplies(message.id).length > 0" class="mt-3 space-y-2 pl-4 border-l-2 border-teal-200">
                <div v-for="reply in getReplies(message.id)" :key="reply.id" class="flex items-start gap-2">
                  <div class="w-7 h-7 rounded-lg flex items-center justify-center text-white shrink-0" style="background: linear-gradient(135deg, #14b8a6, #0d9488);">
                    <i class="fa-solid fa-user-tie text-xs"></i>
                  </div>
                  <div class="flex-1 min-w-0">
                    <div class="flex items-center gap-2 mb-0.5">
                      <span class="text-xs font-semibold text-teal-700">教师回复</span>
                      <span class="text-[11px] text-stone-400">{{ formatDateTime(reply.created_at) }}</span>
                    </div>
                    <p class="text-sm text-stone-700 leading-relaxed bg-teal-50/60 rounded-lg px-3 py-1.5 border border-teal-100/60">{{ reply.content }}</p>
                  </div>
                </div>
              </div>

              <div class="mt-3 flex items-center gap-2">
                <button v-if="replyingTo !== message.id" @click.stop="replyMessage(message.id)" class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center">
                  <i class="fa-solid fa-reply mr-1.5"></i> 回复
                </button>
                <div v-if="replyingTo === message.id" class="w-full flex flex-col gap-2" @click.stop>
                  <textarea v-model="replyContent" rows="3" placeholder="请输入回复内容..." class="input-soft w-full px-3 py-2 text-sm"></textarea>
                  <div class="flex justify-end gap-2">
                    <button @click="cancelReply" class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium">取消</button>
                    <button @click="submitReply" class="btn-teal px-3 py-1.5 rounded-lg text-xs font-medium">发送回复</button>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 7: 兑换记录 ====== -->
    <div v-show="currentTab === 'redemptions'">
      <div class="flex flex-col md:flex-row md:justify-between md:items-center gap-4 mb-6 fade-up">
        <div>
          <p class="text-sm text-stone-500 mb-1">学生兑换记录</p>
          <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800 tracking-tight">兑换记录</h2>
        </div>
        <button @click="loadRedemptions" class="btn-ghost px-4 py-2.5 rounded-xl text-sm font-medium flex items-center">
          <i class="fa-solid fa-rotate mr-2"></i> 刷新列表
        </button>
      </div>

      <EmptyState v-if="redemptions.length === 0" icon="fa-receipt" text="暂无兑换记录" variant="card" />

      <div v-else class="glass rounded-3xl overflow-hidden fade-up delay-1">
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead>
              <tr class="border-b border-stone-200/60" style="background: linear-gradient(90deg, rgba(20, 184, 166, 0.05), rgba(245, 158, 11, 0.03));">
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">学生</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">兑换商品</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">消耗积分</th>
                <th class="px-6 py-4 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">兑换时间</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-200/50">
              <tr v-for="record in redemptions" :key="record.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-800">
                  <div class="font-semibold">{{ record.student_name }}</div>
                  <div class="text-xs text-stone-500">{{ record.className }}</div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-stone-700">{{ record.item_name }}</td>
                <td class="px-6 py-4 whitespace-nowrap">
                  <span class="font-display font-bold text-base text-amber-600">-{{ record.cost }}</span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-xs text-stone-500">{{ record.time }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Modal 1: 添加学生 ====== -->
    <BaseModal
      :show="showAddStudentDialog"
      title="添加学生"
      icon="fa-user-plus"
      icon-from="#14b8a6"
      icon-to="#0d9488"
      @close="showAddStudentDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">学号</label>
          <input v-model="newStudent.studentId" type="text" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">姓名</label>
          <input v-model="newStudent.name" type="text" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">班级</label>
          <select v-model="newStudent.className" class="input-soft w-full px-4 py-2.5 text-sm">
            <option value="">请选择班级</option>
            <option v-for="cls in myClasses" :key="cls.id" :value="cls.name">{{ cls.name }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">初始积分</label>
          <input v-model="newStudent.points" type="number" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
      </div>
      <template #footer>
        <button @click="showAddStudentDialog = false" class="btn-ghost px-5 py-2 rounded-xl text-sm font-medium">取消</button>
        <button @click="addStudent" class="btn-teal px-5 py-2 rounded-xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- ====== Modal 2: 编辑学生 ====== -->
    <BaseModal
      :show="showEditStudentDialog"
      title="编辑学生"
      icon="fa-pen-to-square"
      icon-from="#f59e0b"
      icon-to="#d97706"
      @close="showEditStudentDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">学号</label>
          <input v-model="editStudentData.studentId" type="text" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">姓名</label>
          <input v-model="editStudentData.name" type="text" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">班级</label>
          <select v-model="editStudentData.className" class="input-soft w-full px-4 py-2.5 text-sm">
            <option value="">请选择班级</option>
            <option v-for="cls in myClasses" :key="cls.id" :value="cls.name">{{ cls.name }}</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">积分</label>
          <input v-model="editStudentData.points" type="number" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
      </div>
      <template #footer>
        <button @click="showEditStudentDialog = false" class="btn-ghost px-5 py-2 rounded-xl text-sm font-medium">取消</button>
        <button @click="updateStudent" class="btn-teal px-5 py-2 rounded-xl text-sm font-medium">保存</button>
      </template>
    </BaseModal>

    <!-- ====== Modal 3: 积分操作 ====== -->
    <BaseModal
      :show="showAddPointsDialog"
      title="积分操作"
      icon="fa-coins"
      icon-from="#14b8a6"
      icon-to="#0d9488"
      @close="showAddPointsDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">选择学生</label>
          <select v-model="pointsOperation.studentId" class="input-soft w-full px-4 py-2.5 text-sm">
            <option value="">请选择学生</option>
            <option v-for="student in students" :key="student.id" :value="student.id">{{ student.name }} ({{ student.className }})</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">操作类型</label>
          <select v-model="pointsOperation.type" class="input-soft w-full px-4 py-2.5 text-sm">
            <option value="add">增加积分</option>
            <option value="deduct">扣除积分</option>
          </select>
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">积分数量</label>
          <input v-model="pointsOperation.points" type="number" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">操作原因</label>
          <textarea v-model="pointsOperation.reason" class="input-soft w-full px-4 py-2.5 text-sm" rows="3"></textarea>
        </div>
      </div>
      <template #footer>
        <button @click="showAddPointsDialog = false" class="btn-ghost px-5 py-2 rounded-xl text-sm font-medium">取消</button>
        <button @click="operatePoints" class="btn-teal px-5 py-2 rounded-xl text-sm font-medium">确认操作</button>
      </template>
    </BaseModal>

    <!-- ====== Modal 4: 学生评价 ====== -->
    <BaseModal
      :show="showAddEvaluationDialog"
      title="学生评价"
      icon="fa-star"
      icon-from="#f59e0b"
      icon-to="#fbbf24"
      max-width="max-w-2xl"
      @close="showAddEvaluationDialog = false"
    >
      <div class="space-y-4">
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">选择学生</label>
          <select v-model="evaluation.studentId" class="input-soft w-full px-4 py-2.5 text-sm">
            <option value="">请选择学生</option>
            <option v-for="student in students" :key="student.id" :value="student.id">{{ student.name }} ({{ student.className }})</option>
          </select>
        </div>
        <div v-for="dimension in evaluationDimensions" :key="dimension.id" class="space-y-1.5">
          <label class="block text-xs font-semibold text-stone-600">{{ dimension.name }} (满分{{ dimension.scoreMax }}分)</label>
          <input v-model="evaluation.scores[dimension.id]" type="number" min="0" :max="dimension.scoreMax" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">评价内容</label>
          <textarea v-model="evaluation.comment" class="input-soft w-full px-4 py-2.5 text-sm" rows="3"></textarea>
        </div>
      </div>
      <template #footer>
        <button @click="showAddEvaluationDialog = false" class="btn-ghost px-5 py-2 rounded-xl text-sm font-medium">取消</button>
        <button @click="submitEvaluation" class="btn-teal px-5 py-2 rounded-xl text-sm font-medium">提交评价</button>
      </template>
    </BaseModal>

    <!-- ====== Modal 5: 批量导入学生 ====== -->
    <BaseModal
      :show="showImportDialog"
      title="批量导入学生"
      icon="fa-file-import"
      icon-from="#14b8a6"
      icon-to="#0d9488"
      @close="showImportDialog = false"
    >
      <div class="space-y-4">
        <div class="rounded-2xl p-3 text-sm" style="background: linear-gradient(135deg, rgba(20, 184, 166, 0.08), rgba(245, 158, 11, 0.05)); border: 1px solid rgba(20, 184, 166, 0.18); color: #0f766e;">
          <p class="font-semibold mb-1 flex items-center"><i class="fa-solid fa-circle-info mr-1.5"></i>CSV 文件格式说明：</p>
          <p class="text-stone-700">列顺序：学号,姓名,班级,初始密码</p>
          <p class="text-stone-500 text-xs mt-1">说明：密码可选，默认为 123456</p>
        </div>
        <div>
          <label class="block text-xs font-semibold text-stone-600 mb-1.5">选择 CSV 文件</label>
          <input ref="csvFileInput" type="file" accept=".csv" class="input-soft w-full px-4 py-2.5 text-sm">
        </div>
        <button @click="downloadCsvTemplate" class="text-teal-600 hover:text-teal-700 text-sm flex items-center font-medium">
          <i class="fa-solid fa-download mr-1.5"></i> 下载模板
        </button>
      </div>
      <template #footer>
        <button @click="showImportDialog = false" class="btn-ghost px-5 py-2 rounded-xl text-sm font-medium">取消</button>
        <button @click="importStudentCsv" class="btn-teal px-5 py-2 rounded-xl text-sm font-medium">导入</button>
      </template>
    </BaseModal>
  </AppLayout>
</template>

<style scoped>
/* ===== 主按钮：teal 渐变 ===== */
.btn-teal {
  background: linear-gradient(135deg, #14b8a6 0%, #0d9488 100%);
  color: #fff;
  box-shadow: 0 6px 18px -6px rgba(13, 148, 136, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.25);
  transition: transform 0.25s ease, box-shadow 0.25s ease, filter 0.25s ease;
}
.btn-teal:hover { transform: translateY(-1px); filter: brightness(1.05); box-shadow: 0 10px 24px -6px rgba(13, 148, 136, 0.6); }
.btn-teal:active { transform: translateY(0); }

/* ===== 次按钮：ghost ===== */
.btn-ghost {
  background: rgba(255, 255, 255, 0.6);
  color: #44403c;
  border: 1px solid #e7e5e4;
  transition: all 0.25s ease;
}
.btn-ghost:hover { background: rgba(255, 255, 255, 0.9); border-color: #2dd4bf; color: #0d9488; }

/* ===== 输入框 ===== */
.input-soft {
  background: rgba(255, 255, 255, 0.7);
  border: 1px solid #e7e5e4;
  border-radius: 0.85rem;
  transition: all 0.2s ease;
  color: #292524;
}
.input-soft:focus {
  outline: none;
  border-color: #14b8a6;
  background: #fff;
  box-shadow: 0 0 0 4px rgba(20, 184, 166, 0.12);
}

/* ===== 卡片 hover 抬升 ===== */
.lift-card {
  transition: transform 0.35s cubic-bezier(0.22, 1, 0.36, 1), box-shadow 0.35s ease;
  box-shadow: 0 4px 20px -8px rgba(13, 148, 136, 0.12), 0 2px 6px -2px rgba(0, 0, 0, 0.04);
}
.lift-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 18px 40px -12px rgba(13, 148, 136, 0.22), 0 6px 14px -4px rgba(0, 0, 0, 0.06);
}

/* ===== 表格行 ===== */
.table-row { transition: background 0.2s ease; }
.table-row:hover {
  background: linear-gradient(90deg, rgba(20, 184, 166, 0.05), rgba(245, 158, 11, 0.03));
}

/* ===== 评价进度条 ===== */
.eval-bar {
  height: 8px;
  border-radius: 999px;
  background: rgba(120, 113, 108, 0.12);
  overflow: hidden;
  position: relative;
}
.eval-bar > span {
  display: block;
  height: 100%;
  border-radius: 999px;
  transition: width 0.6s cubic-bezier(0.22, 1, 0.36, 1);
}

/* ===== 标签徽章 ===== */
.chip {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 2px 10px;
  border-radius: 999px;
  font-size: 11px;
  font-weight: 600;
}

/* ===== 平滑过渡 ===== */
.smooth-trans { transition: all 0.3s cubic-bezier(0.22, 1, 0.36, 1); }

/* ===== 入场动画 ===== */
@keyframes fadeUp {
  from { opacity: 0; transform: translateY(16px); }
  to { opacity: 1; transform: translateY(0); }
}
.fade-up { animation: fadeUp 0.6s cubic-bezier(0.22, 1, 0.36, 1) both; }
.delay-1 { animation-delay: 0.05s; }
.delay-2 { animation-delay: 0.12s; }
.delay-3 { animation-delay: 0.19s; }
.delay-4 { animation-delay: 0.26s; }
.delay-5 { animation-delay: 0.33s; }
</style>
