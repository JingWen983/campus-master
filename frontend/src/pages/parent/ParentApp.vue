<script setup lang="ts">
/**
 * 家长端 —— 迁移自 parent.html
 *
 * 单 SFC + v-show 切换 4 个 tab：积分记录 / 教师评价 / 兑换记录 / 家校留言
 * 侧边栏 #extra-section 渲染"我的子女"列表（点击切换 selectChild），
 * 上方为 HERO 子女信息卡。API 走统一 api 封装，alert→toast。
 */
import { ref, onMounted, nextTick } from 'vue'
import { api } from '../../lib/api'
import { checkAuth, logout, type UserInfo } from '../../lib/auth'
import { formatDateTime } from '../../lib/format'
import { THEMES } from '../../lib/theme'
import { PARENT_NAV } from '../../lib/navConfig'
import { toast } from '../../composables/useToast'
import AppLayout from '../../components/AppLayout.vue'
import EmptyState from '../../components/EmptyState.vue'

// ===== Types =====
interface ChildSummary {
  id: string
  name: string
  className?: string
  points?: number | null
  username?: string
}

interface ChildInfo {
  id: string
  name: string
  className?: string
  points?: number | null
  rank?: number | string | null
  username?: string
}

interface PointsRecord {
  id: number | string
  points: number
  type: string
  reason: string
  time: string
}

interface Evaluation {
  id: number | string
  dimension_name: string
  score: number
  comment: string
  evaluator_name: string
  time: string
}

interface Redemption {
  id: number | string
  item_name: string
  cost: number
  time: string
}

interface Message {
  id: number | string
  sender_type: 'parent' | 'teacher'
  content: string
  created_at: string
}

// ===== State =====
const currentTab = ref<'points' | 'evaluation' | 'redemptions' | 'messages'>('points')
const userInfo = ref<UserInfo>({
  id: '',
  username: '',
  name: '',
  role_id: 4,
})
const children = ref<ChildSummary[]>([])
const currentChildId = ref<string | null>(null)
const currentChildInfo = ref<ChildInfo | null>(null)
const pointsRecords = ref<PointsRecord[]>([])
const evaluations = ref<Evaluation[]>([])
const redemptions = ref<Redemption[]>([])
const messages = ref<Message[]>([])
const newMessage = ref('')
const messageContainer = ref<HTMLElement | null>(null)

// ===== API =====
async function loadChildren() {
  try {
    const res = await api.get<ChildSummary[]>('/api/parent/children')
    if (res.code === 200) {
      children.value = res.data || []
      if (children.value.length > 0 && !currentChildId.value) {
        await selectChild(children.value[0].id)
      }
    } else {
      toast.error(res.msg || '获取子女列表失败')
    }
  } catch (e) {
    console.error('加载子女列表失败:', e)
    toast.error('网络错误，无法获取子女列表')
  }
}

async function selectChild(id: string) {
  if (currentChildId.value === id && currentChildInfo.value) return
  currentChildId.value = id
  // 重置数据
  pointsRecords.value = []
  evaluations.value = []
  redemptions.value = []
  messages.value = []
  currentChildInfo.value = null
  await loadChildInfo()
  await loadActiveTabData()
}

async function loadChildInfo() {
  if (!currentChildId.value) return
  try {
    const res = await api.get<ChildInfo>(`/api/parent/student/${currentChildId.value}/info`)
    if (res.code === 200) {
      currentChildInfo.value = res.data || null
      // 同步子女列表中的积分显示
      const child = children.value.find(c => c.id === currentChildId.value)
      if (child && res.data) {
        child.points = res.data.points
      }
    } else {
      toast.error(res.msg || '获取子女信息失败')
    }
  } catch (e) {
    console.error('加载子女信息失败:', e)
    toast.error('网络错误，无法获取子女信息')
  }
}

async function loadPoints() {
  if (!currentChildId.value) return
  try {
    const res = await api.get<PointsRecord[]>(`/api/parent/student/${currentChildId.value}/points`)
    if (res.code === 200) {
      pointsRecords.value = res.data || []
    } else {
      toast.error(res.msg || '获取积分记录失败')
    }
  } catch (e) {
    console.error('加载积分记录失败:', e)
    toast.error('网络错误，无法获取积分记录')
  }
}

async function loadEvaluations() {
  if (!currentChildId.value) return
  try {
    const res = await api.get<Evaluation[]>(`/api/parent/student/${currentChildId.value}/evaluation`)
    if (res.code === 200) {
      evaluations.value = res.data || []
    } else {
      toast.error(res.msg || '获取评价记录失败')
    }
  } catch (e) {
    console.error('加载评价记录失败:', e)
    toast.error('网络错误，无法获取评价记录')
  }
}

async function loadRedemptions() {
  if (!currentChildId.value) return
  try {
    const res = await api.get<Redemption[]>(`/api/parent/student/${currentChildId.value}/redemptions`)
    if (res.code === 200) {
      redemptions.value = res.data || []
    } else {
      toast.error(res.msg || '获取兑换记录失败')
    }
  } catch (e) {
    console.error('加载兑换记录失败:', e)
    toast.error('网络错误，无法获取兑换记录')
  }
}

async function loadMessages() {
  if (!currentChildId.value) return
  try {
    const res = await api.get<Message[]>(`/api/parent/student/${currentChildId.value}/messages`)
    if (res.code === 200) {
      messages.value = res.data || []
      await nextTick()
      scrollToBottom()
    } else {
      toast.error(res.msg || '获取留言失败')
    }
  } catch (e) {
    console.error('加载留言失败:', e)
    toast.error('网络错误，无法获取留言')
  }
}

async function sendMessage() {
  const content = newMessage.value.trim()
  if (!content) return
  if (!currentChildId.value) {
    toast.warning('请先选择子女')
    return
  }
  try {
    const res = await api.post(`/api/parent/student/${currentChildId.value}/messages`, { content })
    if (res.code === 200) {
      newMessage.value = ''
      await loadMessages()
    } else {
      toast.error(res.msg || '发送失败')
    }
  } catch (e) {
    console.error('发送留言失败:', e)
    toast.error('网络错误，发送失败')
  }
}

// ===== Tab Switching =====
function switchTab(tab: string) {
  if (tab === '__logout__') {
    logout()
    return
  }
  currentTab.value = tab as typeof currentTab.value
  loadActiveTabData()
}

function loadActiveTabData() {
  if (!currentChildId.value) return
  if (currentTab.value === 'points') {
    loadPoints()
  } else if (currentTab.value === 'evaluation') {
    loadEvaluations()
  } else if (currentTab.value === 'redemptions') {
    loadRedemptions()
  } else if (currentTab.value === 'messages') {
    loadMessages()
  }
}

function scrollToBottom() {
  const el = messageContainer.value
  if (el) {
    el.scrollTop = el.scrollHeight
  }
}

// ===== Lifecycle =====
onMounted(async () => {
  const user = await checkAuth(4)
  if (!user) return
  userInfo.value = { ...user }
  await loadChildren()
})
</script>

<template>
  <AppLayout
    :theme="THEMES.indigo"
    brand="家校同心"
    brand-en="Parent Portal"
    brand-icon="fa-heart"
    :nav-items="PARENT_NAV"
    :current-tab="currentTab"
    :user-info="userInfo"
    sidebar-width="w-72"
    @switch-tab="switchTab"
  >
    <!-- ====== 侧边栏：我的子女列表（置于功能导航之上） ====== -->
    <template #extra-section>
      <div class="px-3 pb-2">
        <p class="px-3 pt-2 pb-3 text-[10px] font-bold tracking-[0.2em] text-stone-400 uppercase">我的子女</p>
        <div v-if="children.length === 0" class="px-3 py-2 text-xs text-stone-400">
          <i class="fa-solid fa-folder-open mr-2"></i>暂无子女信息
        </div>
        <div
          v-for="child in children"
          :key="child.id"
          class="nav-item rounded-2xl px-3 py-3 mb-1 flex items-center gap-3 cursor-pointer"
          :class="currentChildId === child.id ? 'active' : 'text-stone-600 hover:bg-white/60'"
          @click="selectChild(child.id)"
        >
          <div
            class="child-avatar w-9 h-9 rounded-xl flex items-center justify-center text-white shrink-0 shadow-md"
            style="background: linear-gradient(135deg, #a8a29e, #78716c);"
          >
            <span class="font-display font-bold text-sm">{{ (child.name || '?').charAt(0) }}</span>
          </div>
          <div class="min-w-0 flex-1">
            <p class="text-sm font-bold truncate" :class="currentChildId === child.id ? 'text-indigo-800' : 'text-stone-800'">{{ child.name }}</p>
            <p class="text-[10px] text-stone-500 truncate">{{ child.className }}</p>
          </div>
          <div class="text-right shrink-0">
            <span class="font-display font-bold text-sm" :class="currentChildId === child.id ? 'text-indigo-700' : 'text-stone-700'">{{ child.points || '--' }}</span>
            <p class="text-[9px] text-stone-400 uppercase tracking-wider">points</p>
          </div>
        </div>
      </div>
    </template>

    <!-- ====== 移动端子女切换栏（PC 端在侧边栏） ====== -->
    <section class="md:hidden mb-5 fade-up">
      <div class="flex items-center justify-between mb-3">
        <div class="flex items-center gap-2">
          <i class="fa-solid fa-people-roof text-indigo-500"></i>
          <h2 class="font-display text-base font-bold text-stone-800">我的子女</h2>
        </div>
        <span class="text-xs text-stone-500">共 {{ children.length }} 名</span>
      </div>

      <div v-if="children.length === 0" class="glass rounded-2xl p-6 text-center text-stone-400">
        <i class="fa-solid fa-folder-open text-2xl mb-2 opacity-40"></i>
        <p class="text-sm">暂无子女信息</p>
      </div>

      <div v-else class="flex gap-3 overflow-x-auto no-scrollbar pb-1 -mx-1 px-1">
        <div
          v-for="(child, idx) in children"
          :key="child.id"
          class="child-card rounded-2xl p-3 min-w-[160px] fade-up"
          :class="['delay-' + ((idx % 5) + 1), currentChildId === child.id ? 'active' : '']"
          @click="selectChild(child.id)"
        >
          <div class="flex items-center gap-2.5 mb-2">
            <div
              class="child-avatar w-8 h-8 rounded-xl flex items-center justify-center text-white shrink-0 shadow-sm"
              style="background: linear-gradient(135deg, #a8a29e, #78716c);"
            >
              <span class="font-display font-bold text-xs">{{ (child.name || '?').charAt(0) }}</span>
            </div>
            <div class="min-w-0 flex-1">
              <p class="font-bold text-stone-800 text-sm truncate">{{ child.name }}</p>
              <p class="text-[10px] text-stone-500 truncate">{{ child.className }}</p>
            </div>
          </div>
          <div class="flex items-end justify-between pt-2 border-t border-stone-200/50">
            <span class="text-[9px] text-stone-500 uppercase tracking-wider">积分</span>
            <span class="font-display font-bold text-lg" :style="currentChildId === child.id ? 'color: #4f46e5;' : 'color: #44403c;'">{{ child.points || '--' }}</span>
          </div>
        </div>
      </div>
    </section>

    <!-- ====== 子女信息卡 (HERO) ====== -->
    <section v-if="currentChildInfo" class="mb-6 fade-up delay-2">
      <div
        class="hero-info rounded-[2rem] p-6 md:p-8 text-white shadow-2xl relative"
        style="box-shadow: 0 25px 50px -12px rgba(79, 70, 229, 0.4);"
      >
        <i class="fa-solid fa-graduation-cap deco-icon" style="right: -10px; bottom: -20px; font-size: 9rem; color: white;"></i>
        <i class="fa-solid fa-star deco-icon" style="right: 30px; top: -10px; font-size: 3rem; color: white; opacity: 0.18; transform: rotate(25deg);"></i>

        <div class="flex flex-col md:flex-row md:items-center md:justify-between gap-6">
          <div class="flex items-center gap-4">
            <div
              class="w-16 h-16 md:w-20 md:h-20 rounded-3xl flex items-center justify-center text-white shrink-0 shadow-lg"
              style="background: rgba(255, 255, 255, 0.18); backdrop-filter: blur(10px); border: 1px solid rgba(255, 255, 255, 0.3);"
            >
              <span class="font-display font-bold text-3xl md:text-4xl">{{ (currentChildInfo.name || '?').charAt(0) }}</span>
            </div>
            <div>
              <p class="text-xs font-medium tracking-wider uppercase opacity-80 mb-1">子女档案</p>
              <h3 class="font-display text-2xl md:text-3xl font-bold leading-tight">{{ currentChildInfo.name }}</h3>
              <div class="flex items-center gap-2 mt-2 flex-wrap">
                <span
                  class="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium"
                  style="background: rgba(255, 255, 255, 0.18); border: 1px solid rgba(255, 255, 255, 0.25);"
                >
                  <i class="fa-solid fa-school text-[10px]"></i>{{ currentChildInfo.className }}
                </span>
                <span
                  v-if="currentChildInfo.username"
                  class="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium"
                  style="background: rgba(255, 255, 255, 0.18); border: 1px solid rgba(255, 255, 255, 0.25);"
                >
                  <i class="fa-solid fa-id-badge text-[10px]"></i>{{ currentChildInfo.username }}
                </span>
              </div>
            </div>
          </div>

          <div class="grid grid-cols-2 gap-4 md:gap-6 md:flex md:items-center">
            <div class="text-center md:text-left">
              <p class="text-[10px] font-medium tracking-wider uppercase opacity-80 mb-1">当前积分</p>
              <div class="points-number text-5xl md:text-6xl">{{ currentChildInfo.points !== null && currentChildInfo.points !== undefined ? currentChildInfo.points : '--' }}</div>
              <p class="font-display text-xs italic mt-1 opacity-80">energy points</p>
            </div>
            <div class="h-12 w-px bg-white/20 hidden md:block"></div>
            <div class="text-center md:text-left">
              <p class="text-[10px] font-medium tracking-wider uppercase opacity-80 mb-1">班级排名</p>
              <div class="flex items-baseline justify-center md:justify-start gap-1">
                <span class="font-display font-bold text-5xl md:text-6xl" style="color: #fde68a; text-shadow: 0 0 30px rgba(253, 230, 138, 0.4);">#</span>
                <span class="font-display font-bold text-5xl md:text-6xl" style="color: #fde68a; text-shadow: 0 0 30px rgba(253, 230, 138, 0.4);">{{ currentChildInfo.rank || '--' }}</span>
              </div>
              <p class="font-display text-xs italic mt-1 opacity-80">class rank</p>
            </div>
          </div>
        </div>
      </div>
    </section>

    <!-- 未选择子女提示 -->
    <section v-if="!currentChildInfo && children.length > 0" class="mb-6 fade-up">
      <div class="glass rounded-2xl p-8 text-center">
        <div class="w-16 h-16 rounded-2xl mx-auto mb-4 flex items-center justify-center" style="background: linear-gradient(135deg, #e0e7ff, #c7d2fe);">
          <i class="fa-solid fa-hand-pointer text-2xl text-indigo-700"></i>
        </div>
        <h3 class="font-display text-xl font-bold text-stone-800 mb-2">请选择一位子女</h3>
        <p class="text-sm text-stone-500">从左侧列表（或上方卡片）中选择子女，查看其详细信息</p>
      </div>
    </section>

    <section v-if="children.length === 0" class="mb-6 fade-up">
      <div class="glass rounded-2xl p-8 text-center">
        <i class="fa-solid fa-circle-info text-3xl text-indigo-500 mb-3"></i>
        <h3 class="font-display text-xl font-bold text-stone-800 mb-2">数据加载中...</h3>
        <p class="text-sm text-stone-500">正在获取您的子女信息</p>
      </div>
    </section>

    <!-- ====== Tab 1: 积分记录 ====== -->
    <div v-show="currentTab === 'points' && currentChildInfo" class="fade-up">
      <div class="glass rounded-3xl overflow-hidden">
        <div
          class="px-6 py-4 border-b border-stone-200/60 flex items-center justify-between"
          style="background: linear-gradient(90deg, rgba(99, 102, 241, 0.05), rgba(124, 58, 237, 0.03));"
        >
          <div class="flex items-center gap-3">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #e0e7ff, #c7d2fe); color: #4338ca;">
              <i class="fa-solid fa-coins text-sm"></i>
            </div>
            <div>
              <h3 class="font-display text-lg font-bold text-stone-800">积分记录</h3>
              <p class="text-xs text-stone-500">每一次能量变动都在这里</p>
            </div>
          </div>
          <button class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-1.5" @click="loadPoints">
            <i class="fa-solid fa-rotate-right"></i>刷新
          </button>
        </div>
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead>
              <tr class="border-b border-stone-200/60">
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">时间</th>
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">积分</th>
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">类型</th>
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">原因</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-200/50">
              <EmptyState
                v-if="pointsRecords.length === 0"
                icon="fa-folder-open"
                text="暂无积分记录"
                variant="table-row"
                :colspan="4"
              />
              <tr v-for="record in pointsRecords" :key="record.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-xs text-stone-500">{{ formatDateTime(record.time) }}</td>
                <td class="px-6 py-4 whitespace-nowrap">
                  <span
                    class="font-display font-bold text-base"
                    :style="record.points > 0 ? 'color: #059669;' : (record.points < 0 ? 'color: #be123c;' : 'color: #78716c;')"
                  >
                    {{ record.points > 0 ? '+' : '' }}{{ record.points }}
                  </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap">
                  <span class="chip" :class="record.points >= 0 ? 'chip-award' : 'chip-deduct'">
                    <i :class="record.points >= 0 ? 'fa-solid fa-arrow-up' : 'fa-solid fa-arrow-down'"></i>
                    {{ record.type }}
                  </span>
                </td>
                <td class="px-6 py-4 text-sm text-stone-700">{{ record.reason }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Tab 2: 教师评价 ====== -->
    <div v-show="currentTab === 'evaluation' && currentChildInfo" class="fade-up">
      <div class="mb-4 flex items-center justify-between">
        <div>
          <h3 class="font-display text-xl font-bold text-stone-800">教师评价</h3>
          <p class="text-xs text-stone-500 mt-0.5">五育并举 · 全面发展的成长画像</p>
        </div>
        <button class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-1.5" @click="loadEvaluations">
          <i class="fa-solid fa-rotate-right"></i>刷新
        </button>
      </div>

      <EmptyState
        v-if="evaluations.length === 0"
        icon="fa-star"
        text="暂无评价记录"
        variant="card"
      />

      <div v-else class="grid grid-cols-1 md:grid-cols-2 gap-4 md:gap-5">
        <div
          v-for="(evalItem, idx) in evaluations"
          :key="evalItem.id"
          class="glass lift-card rounded-3xl p-5 fade-up"
          :class="'delay-' + ((idx % 5) + 1)"
          style="box-shadow: 0 4px 20px -8px rgba(79, 70, 229, 0.12), 0 2px 6px -2px rgba(0, 0, 0, 0.04);"
        >
          <div class="flex justify-between items-start mb-3">
            <div class="flex items-center gap-3">
              <div class="w-10 h-10 rounded-xl flex items-center justify-center text-white shrink-0 shadow-md" style="background: linear-gradient(135deg, #6366f1, #7c3aed);">
                <i class="fa-solid fa-star text-sm"></i>
              </div>
              <span class="font-display font-bold text-lg text-stone-800">{{ evalItem.dimension_name }}</span>
            </div>
            <span class="font-display font-bold text-3xl" style="color: #4f46e5;">{{ evalItem.score }}<span class="text-sm text-stone-400 font-body font-normal">/100</span></span>
          </div>
          <div class="eval-bar mb-4">
            <span :style="{ width: Math.min(evalItem.score, 100) + '%', background: 'linear-gradient(90deg, #6366f1, #7c3aed)' }"></span>
          </div>
          <p class="text-sm text-stone-600 mb-3 leading-relaxed">{{ evalItem.comment || '暂无评语' }}</p>
          <p class="text-xs text-stone-400 flex items-center gap-2 flex-wrap">
            <span class="flex items-center gap-1"><i class="fa-solid fa-chalkboard-user"></i>{{ evalItem.evaluator_name || '未知' }}</span>
            <span class="text-stone-300">·</span>
            <span class="flex items-center gap-1"><i class="fa-solid fa-clock"></i>{{ formatDateTime(evalItem.time) }}</span>
          </p>
        </div>
      </div>
    </div>

    <!-- ====== Tab 3: 兑换记录 ====== -->
    <div v-show="currentTab === 'redemptions' && currentChildInfo" class="fade-up">
      <div class="glass rounded-3xl overflow-hidden">
        <div
          class="px-6 py-4 border-b border-stone-200/60 flex items-center justify-between"
          style="background: linear-gradient(90deg, rgba(99, 102, 241, 0.05), rgba(124, 58, 237, 0.03));"
        >
          <div class="flex items-center gap-3">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #fef3c7, #fcd34d); color: #b45309;">
              <i class="fa-solid fa-gift text-sm"></i>
            </div>
            <div>
              <h3 class="font-display text-lg font-bold text-stone-800">兑换记录</h3>
              <p class="text-xs text-stone-500">用心血换来的每一份奖励</p>
            </div>
          </div>
          <button class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-1.5" @click="loadRedemptions">
            <i class="fa-solid fa-rotate-right"></i>刷新
          </button>
        </div>
        <div class="overflow-x-auto">
          <table class="w-full">
            <thead>
              <tr class="border-b border-stone-200/60">
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">时间</th>
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">商品</th>
                <th class="px-6 py-3.5 text-left text-xs font-semibold text-stone-500 uppercase tracking-wider">消耗积分</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-stone-200/50">
              <EmptyState
                v-if="redemptions.length === 0"
                icon="fa-folder-open"
                text="暂无兑换记录"
                variant="table-row"
                :colspan="3"
              />
              <tr v-for="record in redemptions" :key="record.id" class="table-row">
                <td class="px-6 py-4 whitespace-nowrap text-xs text-stone-500">{{ formatDateTime(record.time) }}</td>
                <td class="px-6 py-4 whitespace-nowrap">
                  <div class="flex items-center gap-2.5">
                    <div class="w-8 h-8 rounded-lg flex items-center justify-center text-white shrink-0" style="background: linear-gradient(135deg, #fbbf24, #f59e0b);">
                      <i class="fa-solid fa-gift text-xs"></i>
                    </div>
                    <span class="text-sm font-semibold text-stone-800">{{ record.item_name || '未知商品' }}</span>
                  </div>
                </td>
                <td class="px-6 py-4 whitespace-nowrap">
                  <span class="chip chip-deduct font-display font-bold text-sm">
                    <i class="fa-solid fa-coins"></i>-{{ record.cost }}
                  </span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- ====== Tab 4: 家校留言 ====== -->
    <div v-show="currentTab === 'messages' && currentChildInfo" class="fade-up">
      <div class="glass rounded-3xl overflow-hidden flex flex-col" style="height: calc(100vh - 380px); min-height: 420px;">
        <div
          class="px-6 py-4 border-b border-stone-200/60 flex items-center justify-between shrink-0"
          style="background: linear-gradient(90deg, rgba(99, 102, 241, 0.05), rgba(124, 58, 237, 0.03));"
        >
          <div class="flex items-center gap-3">
            <div class="w-9 h-9 rounded-xl flex items-center justify-center" style="background: linear-gradient(135deg, #e0e7ff, #c7d2fe); color: #4338ca;">
              <i class="fa-solid fa-comments text-sm"></i>
            </div>
            <div>
              <h3 class="font-display text-lg font-bold text-stone-800">家校留言</h3>
              <p class="text-xs text-stone-500">与老师保持沟通，共同关注成长</p>
            </div>
          </div>
          <button class="btn-ghost px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-1.5" @click="loadMessages">
            <i class="fa-solid fa-rotate-right"></i>刷新
          </button>
        </div>

        <div ref="messageContainer" class="flex-1 overflow-y-auto p-4 md:p-6 space-y-4 no-scrollbar">
          <div v-if="messages.length === 0" class="h-full flex flex-col items-center justify-center">
            <EmptyState icon="fa-comments" text="还没有留言，发起第一条沟通吧" variant="inline" />
          </div>

          <div
            v-for="msg in messages"
            :key="msg.id"
            class="flex"
            :class="msg.sender_type === 'parent' ? 'justify-end' : 'justify-start'"
          >
            <div class="flex items-end gap-2 max-w-[80%]" :class="msg.sender_type === 'parent' ? 'flex-row-reverse' : 'flex-row'">
              <div
                class="w-9 h-9 rounded-full flex items-center justify-center text-white shrink-0 shadow-md"
                :style="msg.sender_type === 'parent' ? 'background: linear-gradient(135deg, #6366f1, #4f46e5);' : 'background: linear-gradient(135deg, #a8a29e, #78716c);'"
              >
                <i :class="msg.sender_type === 'parent' ? 'fa-solid fa-user' : 'fa-solid fa-chalkboard-user'"></i>
              </div>
              <div :class="msg.sender_type === 'parent' ? 'items-end' : 'items-start'" class="flex flex-col gap-1 min-w-0">
                <div class="msg-bubble" :class="msg.sender_type === 'parent' ? 'msg-parent' : 'msg-teacher'">
                  {{ msg.content }}
                </div>
                <p class="text-[10px] text-stone-400 px-2" :class="msg.sender_type === 'parent' ? 'text-right' : 'text-left'">
                  {{ formatDateTime(msg.created_at) }}
                </p>
              </div>
            </div>
          </div>
        </div>

        <div class="px-4 md:px-6 py-3 md:py-4 border-t border-stone-200/60 shrink-0" style="background: rgba(250, 246, 239, 0.5);">
          <div class="flex items-end gap-2 md:gap-3">
            <textarea
              v-model="newMessage"
              rows="1"
              placeholder="输入留言内容，按 Enter 发送（Shift+Enter 换行）..."
              class="flex-1 px-4 py-2.5 text-sm resize-none max-h-24"
              style="min-height: 44px; background: rgba(255, 255, 255, 0.7); border: 1px solid #e7e5e4; border-radius: 0.85rem; transition: all 0.2s ease; color: #292524; outline: none;"
              @keydown.enter.exact.prevent="sendMessage"
            ></textarea>
            <button
              class="btn-indigo px-4 md:px-5 py-2.5 rounded-xl text-sm font-medium flex items-center gap-1.5 shrink-0"
              :disabled="!newMessage.trim()"
              @click="sendMessage"
            >
              <i class="fa-solid fa-paper-plane"></i>
              <span class="hidden md:inline">发送</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  </AppLayout>
</template>

<style scoped>
/* ===== 侧边栏子女项 / 导航项（复用原 .nav-item 视觉） ===== */
.nav-item {
  position: relative;
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}
.nav-item:hover { transform: translateX(3px); }
.nav-item.active {
  background: linear-gradient(135deg, rgba(99, 102, 241, 0.16) 0%, rgba(124, 58, 237, 0.08) 100%);
  color: #4338ca;
  box-shadow: inset 0 0 0 1px rgba(99, 102, 241, 0.18);
}
.nav-item.active::before {
  content: '';
  position: absolute;
  left: 0;
  top: 22%;
  bottom: 22%;
  width: 3px;
  border-radius: 0 4px 4px 0;
  background: linear-gradient(180deg, #6366f1, #7c3aed);
}

/* ===== 移动端子女切换卡片 ===== */
.child-card {
  transition: all 0.35s cubic-bezier(0.22, 1, 0.36, 1);
  background: rgba(255, 255, 255, 0.7);
  backdrop-filter: blur(14px) saturate(140%);
  -webkit-backdrop-filter: blur(14px) saturate(140%);
  border: 1px solid rgba(255, 255, 255, 0.6);
  cursor: pointer;
}
.child-card:hover {
  transform: translateY(-3px);
  border-color: rgba(99, 102, 241, 0.35);
  box-shadow: 0 12px 28px -10px rgba(79, 70, 229, 0.25);
}
.child-card.active {
  background: linear-gradient(135deg, rgba(99, 102, 241, 0.16), rgba(124, 58, 237, 0.10));
  border: 1.5px solid rgba(99, 102, 241, 0.55);
  box-shadow: 0 12px 28px -10px rgba(79, 70, 229, 0.35), inset 0 0 0 1px rgba(255, 255, 255, 0.4);
}
.child-card.active .child-avatar {
  background: linear-gradient(135deg, #6366f1, #7c3aed) !important;
  box-shadow: 0 6px 14px -4px rgba(79, 70, 229, 0.5) !important;
}

/* ===== HERO 信息卡 ===== */
.hero-info {
  background:
    radial-gradient(circle at 20% 20%, rgba(255, 255, 255, 0.28), transparent 50%),
    linear-gradient(135deg, #4338ca 0%, #4f46e5 45%, #7c3aed 100%);
  position: relative;
  overflow: hidden;
}
.hero-info::before {
  content: '';
  position: absolute;
  inset: 0;
  background: url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n2'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='2'/%3E%3CfeColorMatrix values='0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0.15 0'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n2)'/%3E%3C/svg%3E");
  opacity: 0.55;
  mix-blend-mode: overlay;
}
.hero-info > * { position: relative; z-index: 2; }

.points-number {
  font-family: 'Fraunces', serif;
  font-weight: 800;
  font-variation-settings: "opsz" 144;
  line-height: 0.9;
  letter-spacing: -0.04em;
  background: linear-gradient(180deg, #ffffff 0%, #e0e7ff 100%);
  -webkit-background-clip: text;
  background-clip: text;
  -webkit-text-fill-color: transparent;
  text-shadow: 0 0 40px rgba(199, 210, 254, 0.4);
}

/* 装饰图标 */
.deco-icon {
  position: absolute;
  opacity: 0.14;
  pointer-events: none;
}

/* ===== 主按钮：indigo→violet 渐变 ===== */
.btn-indigo {
  background: linear-gradient(135deg, #6366f1 0%, #4f46e5 50%, #7c3aed 100%);
  color: #fff;
  box-shadow: 0 6px 18px -6px rgba(79, 70, 229, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.25);
  transition: transform 0.25s ease, box-shadow 0.25s ease, filter 0.25s ease;
}
.btn-indigo:hover { transform: translateY(-1px); filter: brightness(1.05); box-shadow: 0 10px 24px -6px rgba(79, 70, 229, 0.6); }
.btn-indigo:active { transform: translateY(0); }
.btn-indigo:disabled { opacity: 0.5; cursor: not-allowed; transform: none; }

/* ===== 次按钮：ghost ===== */
.btn-ghost {
  background: rgba(255, 255, 255, 0.6);
  color: #44403c;
  border: 1px solid #e7e5e4;
  transition: all 0.25s ease;
}
.btn-ghost:hover { background: rgba(255, 255, 255, 0.9); border-color: #6366f1; color: #4f46e5; }

/* ===== 表格行 ===== */
.table-row { transition: background 0.2s ease; }
.table-row:hover {
  background: linear-gradient(90deg, rgba(99, 102, 241, 0.05), rgba(124, 58, 237, 0.03));
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
.chip-award { background: rgba(16, 185, 129, 0.12); color: #047857; }
.chip-deduct { background: rgba(244, 63, 94, 0.12); color: #be123c; }

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

/* ===== 留言气泡 ===== */
.msg-bubble {
  max-width: 75%;
  padding: 12px 16px;
  border-radius: 1.25rem;
  font-size: 14px;
  line-height: 1.55;
  word-break: break-word;
  box-shadow: 0 4px 12px -4px rgba(0, 0, 0, 0.08);
}
.msg-parent {
  background: linear-gradient(135deg, #6366f1, #4f46e5);
  color: #fff;
  border-bottom-right-radius: 0.4rem;
}
.msg-teacher {
  background: rgba(255, 255, 255, 0.85);
  color: #292524;
  border: 1px solid #e7e5e4;
  border-bottom-left-radius: 0.4rem;
}

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
.delay-6 { animation-delay: 0.40s; }
</style>
