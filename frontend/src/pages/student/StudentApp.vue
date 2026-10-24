<script setup lang="ts">
/**
 * 学生空间 —— 迁移自 student.html
 *
 * 6 个 tab：能量大厅 / 兑换商城 / 个人评价 / 兑换记录 / 风采榜 / 个人档案
 * 单 SFC + v-show 切换，保留原架构。API 走统一 api 封装，alert/confirm 替换为 toast/confirmDialog，
 * ECharts 雷达图用 BaseChart 组件 + computed option。
 */
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue'
import type { EChartsOption } from 'echarts'
import { api } from '../../lib/api'
import { checkAuth, logout } from '../../lib/auth'
import { formatDateTime } from '../../lib/format'
import { THEMES } from '../../lib/theme'
import { STUDENT_NAV } from '../../lib/navConfig'
import { makeQrDataUrl, scanVideoFrame } from '../../lib/qrcode'
import { toast } from '../../composables/useToast'
import { confirmDialog } from '../../composables/useConfirm'
import AppLayout from '../../components/AppLayout.vue'
import BaseChart from '../../components/BaseChart.vue'
import BaseModal from '../../components/BaseModal.vue'
import EmptyState from '../../components/EmptyState.vue'

// ===== Types =====
interface StudentInfo {
  id: string
  username: string
  name: string
  role_id: number
  className?: string
  points?: number
  rank?: number | string
}

interface PointsRecord {
  id: number | string
  points: number
  reason: string
  time: string
}

interface MallItem {
  id: number | string
  name: string
  description: string
  price: number
}

interface Evaluation {
  id: number | string
  dimension_id: number
  dimension_name: string
  score: number
  comment: string
  evaluator_name: string
  time: string
}

interface Redemption {
  id: number | string
  item_name: string
  item_description: string
  cost: number
  time: string
  /** 两阶段兑换：验证码与状态 */
  code?: string
  status?: string
  status_text?: string
}

interface LeaderboardEntry {
  id: number | string
  name: string
  totalPoints: number
}

interface HistoryItem {
  id: string
  points: number
  reason: string
  time: string
  sortTime: string
}

interface RedeemResponse {
  remain_points: number
  /** 两阶段兑换：8 位验证码（XXXX-XXXX） */
  redemption_code?: string
}

// ===== State =====
const currentTab = ref('home')
const userInfo = ref<StudentInfo>({
  id: '',
  username: '',
  name: '',
  role_id: 3,
  className: '',
})
const userRank = ref<number | string>('--')
const history = ref<HistoryItem[]>([])
const mallItems = ref<MallItem[]>([])
const evaluations = ref<Evaluation[]>([])
const redemptions = ref<Redemption[]>([])
const leaderboard = ref<LeaderboardEntry[]>([])
const radarChartRef = ref<InstanceType<typeof BaseChart>>()

// ===== 两阶段兑换：验证码弹窗状态 =====
const lastRedeemCode = ref('')
const lastRedeemItem = ref('')
const lastRedeemQr = ref('')

// ===== 积分卡（卡密充值） =====
interface MyCardRecord {
  id: number | string
  code: string
  face_value: number
  status: string
  status_text: string
  time: string
}

const cardCodeInput = ref('')
const cardRedeemLoading = ref(false)
const myCardRecords = ref<MyCardRecord[]>([])

// 摄像头扫码（复用后勤端同款 jsQR 逐帧识别）
const cardScanning = ref(false)
const cardScanError = ref('')
const cardVideoEl = ref<HTMLVideoElement | null>(null)
const cardCanvasEl = ref<HTMLCanvasElement | null>(null)
let cardMediaStream: MediaStream | null = null
let cardScanTimer: number | undefined
let cardLastScanned = ''
let cardLastScannedAt = 0

/** 输入时自动格式化：去非法字符 → 大写 → 每 4 位补横线 */
function formatCardInput() {
  const raw = cardCodeInput.value.toUpperCase().replace(/[^0-9A-Z]/g, '')
  const parts: string[] = []
  for (let i = 0; i < raw.length && i < 16; i += 4) {
    parts.push(raw.slice(i, i + 4))
  }
  cardCodeInput.value = parts.join('-')
}

async function loadMyCardRecords() {
  try {
    const res = await api.get<MyCardRecord[]>('/api/student/point-cards')
    if (res.code === 200 && res.data) myCardRecords.value = res.data
  } catch { /* api 层已提示 */ }
}

async function redeemCard() {
  const code = cardCodeInput.value.trim()
  if (code.replace(/-/g, '').length !== 16) {
    toast.warning('请输入完整的 16 位卡密')
    return
  }
  cardRedeemLoading.value = true
  try {
    const res = await api.post<{ result: string; face_value: number; points?: number }>(
      '/api/student/point-cards/redeem', { code })
    if (res.code === 200 && res.data) {
      if (res.data.result === 'credited') {
        toast.success(`充值成功！+${res.data.face_value} 积分`)
        if (res.data.points != null) userInfo.value.points = res.data.points
      } else {
        toast.info('已提交，等待老师确认后到账')
      }
      cardCodeInput.value = ''
      await Promise.all([loadMyCardRecords(), fetchHistory()])
    } else {
      toast.error(res.msg || '兑换失败')
    }
  } catch { /* api 层已提示 */ } finally {
    cardRedeemLoading.value = false
  }
}

async function startCardScan() {
  cardScanError.value = ''
  try {
    cardMediaStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: 'environment', width: { ideal: 1280 }, height: { ideal: 720 } },
    })
    await nextTick()
    if (cardVideoEl.value) {
      cardVideoEl.value.srcObject = cardMediaStream
      await cardVideoEl.value.play()
    }
    cardScanning.value = true
    cardScanTimer = window.setInterval(() => {
      if (!cardVideoEl.value || !cardCanvasEl.value) return
      const text = scanVideoFrame(cardVideoEl.value, cardCanvasEl.value)
      if (!text) return
      const now = Date.now()
      if (text === cardLastScanned && now - cardLastScannedAt < 1500) return
      cardLastScanned = text
      cardLastScannedAt = now
      // 识别到卡号：填入输入框并自动提交
      stopCardScan()
      cardCodeInput.value = text
      formatCardInput()
      redeemCard()
    }, 220)
  } catch {
    cardScanError.value = '无法访问摄像头，请检查浏览器权限（需 HTTPS 或 localhost），或改用手动输入'
    cardScanning.value = false
  }
}

function stopCardScan() {
  if (cardScanTimer) { clearInterval(cardScanTimer); cardScanTimer = undefined }
  if (cardMediaStream) {
    cardMediaStream.getTracks().forEach(t => t.stop())
    cardMediaStream = null
  }
  cardScanning.value = false
}

// ===== Computed: 雷达图 option =====
const radarOption = computed<EChartsOption>(() => {
  const evalData: Record<number, number> = { 1: 0, 2: 0, 3: 0, 4: 0, 5: 0 }
  evaluations.value.forEach(e => {
    if (e.dimension_id && e.score) {
      evalData[e.dimension_id] = e.score
    }
  })
  return {
    tooltip: {},
    legend: { data: ['五育发展'], bottom: 0 },
    radar: {
      indicator: [
        { name: '德育', max: 100 },
        { name: '智育', max: 100 },
        { name: '体育', max: 100 },
        { name: '美育', max: 100 },
        { name: '劳育', max: 100 },
      ],
      shape: 'polygon',
    },
    series: [{
      type: 'radar',
      data: [{
        value: [evalData[1], evalData[2], evalData[3], evalData[4], evalData[5]],
        name: '五育发展',
        areaStyle: { color: 'rgba(59, 130, 246, 0.3)' },
        lineStyle: { color: '#3b82f6' },
        itemStyle: { color: '#3b82f6' },
      }],
    }],
  }
})

// ===== API Methods =====
async function fetchUserInfo() {
  try {
    const res = await api.get<StudentInfo>('/api/student/info')
    if (res.code === 200 && res.data) {
      userInfo.value = { ...userInfo.value, ...res.data }
      userRank.value = res.data.rank ?? '--'
    }
  } catch { /* api 层已提示 */ }
}

async function fetchHistory() {
  try {
    const [pointsRes, redemptionRes] = await Promise.all([
      api.get<PointsRecord[]>('/api/student/points/records'),
      api.get<Redemption[]>('/api/student/redemptions'),
    ])
    const list: HistoryItem[] = []
    if (pointsRes.code === 200 && pointsRes.data) {
      pointsRes.data.forEach(r => {
        list.push({
          id: 'p' + r.id,
          points: r.points,
          reason: r.reason,
          time: r.time,
          sortTime: r.time,
        })
      })
    }
    if (redemptionRes.code === 200 && redemptionRes.data) {
      redemptionRes.data.forEach(r => {
        list.push({
          id: 'r' + r.id,
          points: -r.cost,
          reason: '兑换 ' + r.item_name,
          time: r.time,
          sortTime: r.time,
        })
      })
    }
    list.sort((a, b) => (b.sortTime || '').localeCompare(a.sortTime || ''))
    history.value = list
  } catch { /* api 层已提示 */ }
}

async function fetchMallItems() {
  try {
    const res = await api.get<MallItem[]>('/api/student/mall')
    if (res.code === 200 && res.data) {
      mallItems.value = res.data
    }
  } catch { /* api 层已提示 */ }
}

async function fetchLeaderboard() {
  try {
    const res = await api.get<LeaderboardEntry[]>('/api/rank/class')
    if (res.code === 200 && res.data) {
      leaderboard.value = res.data
    }
  } catch { /* api 层已提示 */ }
}

async function fetchEvaluation() {
  try {
    const res = await api.get<Evaluation[]>('/api/student/evaluation')
    if (res.code === 200 && res.data) {
      evaluations.value = res.data
    }
  } catch { /* api 层已提示 */ }
}

async function fetchRedemptions() {
  try {
    const res = await api.get<Redemption[]>('/api/student/redemptions')
    if (res.code === 200 && res.data) {
      redemptions.value = res.data
    }
  } catch { /* api 层已提示 */ }
}

async function refreshHome() {
  await fetchUserInfo()
  await fetchHistory()
}

async function redeem(item: MallItem) {
  if ((userInfo.value.points ?? 0) < item.price) {
    toast.warning('积分不足哦！请继续努力获取积分。')
    return
  }
  const ok = await confirmDialog({
    message: `确定花费 ${item.price} 积分兑换【${item.name}】吗？兑换后请凭验证码到后勤处领取`,
    variant: 'primary',
    confirmText: '确认兑换',
  })
  if (!ok) return
  try {
    const res = await api.post<RedeemResponse>('/api/mall/redeem', { item_id: item.id })
    if (res.code === 200 && res.data) {
      userInfo.value.points = res.data.remain_points
      // 两阶段兑换：弹窗展示验证码 + 二维码（学生可截图保存去后勤核销）
      const code = res.data.redemption_code || ''
      if (code) {
        lastRedeemCode.value = code
        lastRedeemItem.value = item.name
        lastRedeemQr.value = await makeQrDataUrl(code)
      } else {
        toast.success('兑换成功！')
      }
      await Promise.all([fetchMallItems(), fetchRedemptions()])
    } else {
      toast.error(res.msg || '兑换失败')
    }
  } catch { /* api 层已提示 */ }
}

// 从兑换记录重新打开验证码弹窗（仅未兑换的单子可领取）
async function showRedeemQr(record: Redemption) {
  if (!record.code) {
    toast.info('该记录为旧版兑换（无验证码），已直接完成')
    return
  }
  lastRedeemCode.value = record.code
  lastRedeemItem.value = record.item_name
  lastRedeemQr.value = await makeQrDataUrl(record.code)
}

function statusBadgeClass(status?: string): string {
  if (status === 'pending') return 'bg-amber-100 text-amber-700'
  if (status === 'expired') return 'bg-stone-200 text-stone-500'
  return 'bg-emerald-100 text-emerald-700'   // redeemed / 旧记录默认
}

// ===== Tab Switching =====
function switchTab(tab: string) {
  if (tab === '__logout__') {
    logout()
    return
  }
  currentTab.value = tab
  if (tab === 'home') {
    fetchUserInfo()
    fetchHistory()
  } else if (tab === 'mall') {
    fetchMallItems()
  } else if (tab === 'cards') {
    loadMyCardRecords()
  } else if (tab === 'evaluation') {
    fetchEvaluation()
  } else if (tab === 'redemptions') {
    fetchRedemptions()
  } else if (tab === 'rank') {
    fetchLeaderboard()
  } else if (tab === 'profile') {
    fetchHistory()
    nextTick(() => {
      radarChartRef.value?.resize()
    })
  }
}

// ===== Lifecycle =====
onMounted(async () => {
  const user = await checkAuth(3)
  if (!user) return
  userInfo.value = { ...user }
  await Promise.all([
    fetchUserInfo(),
    fetchHistory(),
    fetchMallItems(),
    fetchLeaderboard(),
    fetchEvaluation(),
    fetchRedemptions(),
    loadMyCardRecords(),
  ])
})

onUnmounted(() => {
  stopCardScan()
})
</script>

<template>
  <AppLayout
    :theme="THEMES.emerald"
    brand="文明能量站"
    brand-en="Energy Station"
    brand-icon="fa-leaf"
    :nav-items="STUDENT_NAV"
    :current-tab="currentTab"
    :user-info="userInfo"
    sidebar-width="w-72"
    @switch-tab="switchTab"
  >
    <!-- ====== Tab 1: 能量大厅 ====== -->
    <div v-show="currentTab === 'home'" class="grid grid-cols-1 md:grid-cols-3 gap-5 md:gap-7">
      <!-- HERO 积分卡片 -->
      <div class="md:col-span-1 anim-fade-up">
        <div class="hero-points rounded-[2rem] p-7 text-white shadow-2xl relative" style="box-shadow: 0 25px 50px -12px rgba(5, 150, 105, 0.4);">
          <i class="fa-solid fa-bolt deco-icon" style="right: -10px; bottom: -20px; font-size: 9rem; color: white;"></i>
          <i class="fa-solid fa-leaf deco-icon" style="right: 30px; top: -10px; font-size: 3rem; color: white; opacity: 0.18; transform: rotate(25deg);"></i>

          <div class="flex items-center gap-2 mb-2">
            <span class="inline-block w-2 h-2 rounded-full bg-emerald-200 animate-pulse"></span>
            <p class="text-xs font-medium tracking-wider uppercase opacity-90">当前能量积分</p>
          </div>

          <div class="mb-7 mt-3">
            <div class="points-number text-7xl md:text-[5.5rem]">
              {{ userInfo.points != null ? userInfo.points : '--' }}
            </div>
            <p class="font-display text-2xl font-light italic mt-1 opacity-90">energy points</p>
          </div>

          <div class="flex items-center justify-between gap-3 bg-white/15 backdrop-blur-md p-3 rounded-2xl border border-white/20">
            <div class="flex items-center gap-2 min-w-0">
              <i class="fa-solid fa-user-circle text-lg opacity-80"></i>
              <span class="text-sm font-medium truncate">{{ userInfo.name || '同学' }}</span>
            </div>
            <div class="h-4 w-px bg-white/30"></div>
            <span class="text-xs opacity-90 truncate">{{ userInfo.className || '--' }}</span>
          </div>
        </div>
      </div>

      <!-- 行为记录明细 -->
      <div class="md:col-span-2 card-base p-6 md:p-7 anim-fade-up delay-2">
        <div class="flex justify-between items-center mb-5 pb-4 border-b border-stone-200/60">
          <div>
            <h3 class="font-display text-2xl font-bold text-stone-800" style="letter-spacing: -0.02em;">最近能量动态</h3>
            <p class="text-xs text-stone-500 mt-0.5">每一次行动都在塑造更好的你</p>
          </div>
          <button class="btn-emerald text-xs px-4 py-2 rounded-full font-medium flex items-center gap-1.5" @click="refreshHome">
            <i class="fa-solid fa-rotate-right"></i>刷新
          </button>
        </div>

        <EmptyState v-if="history.length === 0" icon="fa-folder-open" text="暂无记录" variant="inline" />

        <div class="space-y-2">
          <div
            v-for="(record, idx) in history"
            :key="record.id"
            :class="'anim-fade-up delay-' + ((idx % 5) + 1)"
            class="history-row flex justify-between items-center group p-3"
          >
            <div class="flex items-center gap-4 min-w-0">
              <div :class="record.points > 0 ? 'bg-emerald-100 text-emerald-600' : 'bg-orange-100 text-orange-600'" class="w-11 h-11 rounded-2xl flex items-center justify-center shrink-0 shadow-sm">
                <i :class="record.points > 0 ? 'fa-solid fa-arrow-trend-up' : 'fa-solid fa-arrow-trend-down'"></i>
              </div>
              <div class="min-w-0">
                <p class="text-base font-bold text-stone-800 truncate">{{ record.reason }}</p>
                <p class="text-xs text-stone-400 mt-0.5"><i class="fa-solid fa-clock mr-1"></i>{{ formatDateTime(record.time) }}</p>
              </div>
            </div>
            <span :class="record.points > 0 ? 'text-emerald-600 bg-emerald-50' : 'text-orange-600 bg-orange-50'" class="font-display font-bold text-xl px-3 py-1.5 rounded-xl whitespace-nowrap">
              {{ record.points > 0 ? '+' : '' }}{{ record.points }}
            </span>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 2: 兑换商城 ====== -->
    <div v-show="currentTab === 'mall'">
      <div class="flex flex-wrap justify-between items-end gap-4 mb-7 anim-fade-up">
        <div>
          <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800" style="letter-spacing: -0.03em;">福利兑换</h2>
          <p class="text-sm text-stone-500 mt-1">用积累的能量，换取心仪的奖励</p>
        </div>
        <span class="badge-emerald px-4 py-2 rounded-full text-sm font-bold flex items-center gap-2">
          <i class="fa-solid fa-coins text-amber-500"></i>我的积分: <span class="font-display text-base">{{ userInfo.points }}</span>
        </span>
      </div>

      <div class="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4 md:gap-6">
        <div
          v-for="(item, idx) in mallItems"
          :key="item.id"
          :class="'card-base card-hover anim-fade-up delay-' + ((idx % 6) + 1)"
          class="overflow-hidden flex flex-col"
        >
          <div class="mall-icon-wrap h-32 flex items-center justify-center relative">
            <i class="fa-solid fa-gift text-5xl relative z-10" style="color: #059669; filter: drop-shadow(0 4px 8px rgba(5, 150, 105, 0.25));"></i>
          </div>
          <div class="p-4 flex flex-col flex-1">
            <h3 class="text-base font-bold text-stone-800 mb-1 leading-snug">{{ item.name }}</h3>
            <p class="text-xs text-stone-500 mb-4 flex-1 line-clamp-2 leading-relaxed">{{ item.description }}</p>
            <div class="flex items-center justify-between mt-auto pt-3 border-t border-stone-100">
              <span class="font-display font-bold text-base text-amber-600 flex items-center gap-1">
                <i class="fa-solid fa-coins text-xs"></i>{{ item.price }}
              </span>
              <button class="btn-emerald text-xs px-4 py-2 rounded-full font-medium" @click="redeem(item)">兑换</button>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 2.5: 积分卡充值 ====== -->
    <div v-show="currentTab === 'cards'">
      <div class="mb-7 anim-fade-up">
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800" style="letter-spacing: -0.03em;">积分卡充值</h2>
        <p class="text-sm text-stone-500 mt-1">输入卡密或扫描卡片二维码，充值你的能量积分</p>
      </div>

      <div class="grid grid-cols-1 lg:grid-cols-5 gap-5">
        <!-- 充值区 -->
        <div class="lg:col-span-3 card-base p-6 anim-fade-up">
          <h3 class="font-bold text-stone-800 mb-4 flex items-center gap-2">
            <i class="fa-solid fa-ticket text-amber-500"></i>卡密充值
          </h3>
          <input
            v-model="cardCodeInput"
            type="text"
            placeholder="XXXX-XXXX-XXXX-XXXX"
            maxlength="19"
            class="w-full px-5 py-4 text-center font-mono text-xl tracking-[0.15em] uppercase rounded-2xl border-2 border-stone-200 focus:border-amber-400 focus:outline-none bg-amber-50/50"
            @input="formatCardInput"
            @keyup.enter="redeemCard"
          >
          <p class="text-xs text-stone-400 mt-2 text-center">16 位卡密，见实物卡或老师发放的图片</p>

          <div class="flex gap-3 mt-5">
            <button
              class="btn-emerald flex-1 py-3.5 rounded-2xl font-bold"
              :disabled="cardRedeemLoading"
              @click="redeemCard"
            >
              <i class="fa-solid fa-bolt mr-2"></i>{{ cardRedeemLoading ? '提交中…' : '立即充值' }}
            </button>
            <button
              v-if="!cardScanning"
              class="px-5 py-3.5 rounded-2xl text-sm font-medium bg-stone-100 text-stone-600 hover:bg-stone-200"
              @click="startCardScan"
            >
              <i class="fa-solid fa-qrcode mr-2"></i>扫码
            </button>
            <button
              v-else
              class="px-5 py-3.5 rounded-2xl text-sm font-medium bg-rose-100 text-rose-600 hover:bg-rose-200"
              @click="stopCardScan"
            >
              <i class="fa-solid fa-stop mr-2"></i>停止
            </button>
          </div>

          <!-- 扫码取景 -->
          <div v-if="cardScanning" class="mt-4 relative rounded-2xl overflow-hidden bg-stone-900 aspect-video">
            <video ref="cardVideoEl" class="w-full h-full object-cover" muted playsinline></video>
            <canvas ref="cardCanvasEl" class="hidden"></canvas>
            <div class="absolute inset-[22%] border-2 border-emerald-300/90 rounded-xl pointer-events-none" style="box-shadow: 0 0 0 9999px rgba(5,10,20,0.35);"></div>
          </div>
          <p v-if="cardScanError" class="text-xs text-rose-500 mt-2">{{ cardScanError }}</p>

          <div class="mt-5 p-4 rounded-2xl bg-stone-50 text-xs text-stone-500 leading-relaxed">
            <p><i class="fa-solid fa-circle-info mr-1.5 text-amber-500"></i><b>定向卡</b>（卡面印有你的名字）充值立即到账</p>
            <p class="mt-1"><i class="fa-solid fa-circle-info mr-1.5 text-amber-500"></i><b>通用卡</b>提交后需老师确认，到账会有记录</p>
          </div>
        </div>

        <!-- 记录区 -->
        <div class="lg:col-span-2 card-base p-6 anim-fade-up delay-2">
          <h3 class="font-bold text-stone-800 mb-4 flex items-center gap-2">
            <i class="fa-solid fa-clock-rotate-left text-emerald-600"></i>我的充值记录
          </h3>
          <EmptyState v-if="myCardRecords.length === 0" icon="fa-ticket" text="还没有充值记录" variant="inline" />
          <div v-else class="space-y-3 max-h-[420px] overflow-y-auto">
            <div v-for="r in myCardRecords" :key="r.id" class="p-3.5 rounded-2xl bg-stone-50 flex items-center gap-3">
              <div class="w-10 h-10 rounded-xl flex items-center justify-center shrink-0"
                :class="r.status === 'used' ? 'bg-emerald-100 text-emerald-600' : r.status === 'pending' ? 'bg-amber-100 text-amber-600' : 'bg-stone-200 text-stone-500'">
                <i class="fa-solid" :class="r.status === 'used' ? 'fa-check' : r.status === 'pending' ? 'fa-hourglass-half' : 'fa-ticket'"></i>
              </div>
              <div class="flex-1 min-w-0">
                <div class="flex justify-between items-center gap-2">
                  <span class="font-bold text-stone-800">+{{ r.face_value }} 积分</span>
                  <span class="text-xs font-bold shrink-0"
                    :class="r.status === 'used' ? 'text-emerald-600' : r.status === 'pending' ? 'text-amber-600' : 'text-stone-500'">
                    {{ r.status_text }}
                  </span>
                </div>
                <p class="text-[11px] text-stone-400 font-mono mt-0.5">{{ r.code }}</p>
                <p class="text-[11px] text-stone-400">{{ r.time }}</p>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 3: 个人评价 ====== -->
    <div v-show="currentTab === 'evaluation'">
      <div class="mb-7 anim-fade-up">
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800" style="letter-spacing: -0.03em;">个人评价</h2>
        <p class="text-sm text-stone-500 mt-1">老师眼中的你，多维度的成长画像</p>
      </div>

      <EmptyState v-if="evaluations.length === 0" icon="fa-star" text="暂无评价记录" variant="card" />

      <div class="grid grid-cols-1 md:grid-cols-2 gap-5">
        <div
          v-for="(evalItem, idx) in evaluations"
          :key="evalItem.id"
          :class="'card-base card-hover anim-fade-up delay-' + ((idx % 5) + 1)"
          class="p-5"
        >
          <div class="flex justify-between items-start mb-3">
            <div class="flex items-center gap-3">
              <div class="w-10 h-10 rounded-xl flex items-center justify-center text-white shrink-0" style="background: linear-gradient(135deg, #10b981, #059669);">
                <i class="fa-solid fa-star text-sm"></i>
              </div>
              <span class="font-display font-bold text-lg text-stone-800">{{ evalItem.dimension_name }}</span>
            </div>
            <span class="font-display font-bold text-2xl text-emerald-600">{{ evalItem.score }}<span class="text-sm text-stone-400 font-body font-normal">/100</span></span>
          </div>
          <div class="score-bar mb-4">
            <div class="score-fill" :style="{ width: Math.min(evalItem.score, 100) + '%' }"></div>
          </div>
          <p class="text-sm text-stone-600 mb-3 leading-relaxed">{{ evalItem.comment }}</p>
          <p class="text-xs text-stone-400 flex items-center gap-2">
            <i class="fa-solid fa-chalkboard-user"></i>{{ evalItem.evaluator_name }}
            <span class="text-stone-300">·</span>
            <i class="fa-solid fa-clock"></i>{{ formatDateTime(evalItem.time) }}
          </p>
        </div>
      </div>
    </div>

    <!-- ====== Tab 4: 兑换记录 ====== -->
    <div v-show="currentTab === 'redemptions'">
      <div class="mb-7 anim-fade-up">
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800" style="letter-spacing: -0.03em;">兑换记录</h2>
        <p class="text-sm text-stone-500 mt-1">回顾你用心血换来的每一份奖励</p>
      </div>

      <EmptyState v-if="redemptions.length === 0" icon="fa-folder-open" text="暂无兑换记录" variant="card" />

      <div class="space-y-4">
        <div
          v-for="(record, idx) in redemptions"
          :key="record.id"
          :class="'card-base card-hover anim-fade-up delay-' + ((idx % 5) + 1)"
          class="p-5 flex items-center gap-4"
        >
          <div class="w-14 h-14 rounded-2xl flex items-center justify-center shrink-0 mall-icon-wrap">
            <i class="fa-solid fa-gift text-2xl relative z-10" style="color: #059669;"></i>
          </div>
          <div class="flex-1 min-w-0">
            <div class="flex justify-between items-center mb-1 gap-3">
              <span class="font-bold text-stone-800 truncate">{{ record.item_name }}</span>
              <span class="font-display font-bold text-base text-orange-600 whitespace-nowrap">-{{ record.cost }}<span class="text-xs font-body font-normal text-stone-400"> 积分</span></span>
            </div>
            <p class="text-sm text-stone-500 mb-1 line-clamp-2">{{ record.item_description }}</p>
            <div class="flex items-center justify-between gap-3">
              <p class="text-xs text-stone-400"><i class="fa-solid fa-clock mr-1"></i>{{ formatDateTime(record.time) }}</p>
              <div class="flex items-center gap-2 shrink-0">
                <span class="px-2.5 py-0.5 rounded-full text-xs font-bold" :class="statusBadgeClass(record.status)">
                  {{ record.status_text || '已兑换' }}
                </span>
                <button
                  v-if="record.status === 'pending' && record.code"
                  class="text-xs font-medium text-emerald-700 bg-emerald-50 hover:bg-emerald-100 px-3 py-1 rounded-full transition"
                  @click="showRedeemQr(record)"
                >
                  <i class="fa-solid fa-qrcode mr-1"></i>查看二维码
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== 两阶段兑换：验证码 + 二维码弹窗 ====== -->
    <BaseModal
      :show="!!lastRedeemCode"
      title="兑换成功"
      icon="fa-circle-check"
      icon-from="#6ee7b7"
      icon-to="#059669"
      max-width="max-w-md"
      @close="lastRedeemCode = ''"
    >
      <div class="text-center py-2">
        <p class="text-sm text-stone-500 mb-3">{{ lastRedeemItem }}</p>
        <img
          v-if="lastRedeemQr"
          :src="lastRedeemQr"
          alt="兑换二维码"
          class="w-56 h-56 mx-auto rounded-2xl border-4 border-emerald-100 shadow-lg"
        >
        <p class="font-display text-3xl font-black tracking-[0.18em] text-stone-800 mt-5 select-all">
          {{ lastRedeemCode }}
        </p>
        <p class="text-sm text-stone-500 mt-3 leading-relaxed">
          请截图保存，凭<b>二维码</b>或<b>验证码</b>到后勤处领取
        </p>
        <p class="text-xs text-amber-600 mt-1.5"><i class="fa-solid fa-clock mr-1"></i>7 天内未领取将自动退回积分</p>
      </div>
      <template #footer>
        <button class="btn-emerald px-8 py-2.5 rounded-2xl text-sm font-medium" @click="lastRedeemCode = ''">
          我已保存
        </button>
      </template>
    </BaseModal>

    <!-- ====== Tab 5: 班级风采榜 ====== -->
    <div v-show="currentTab === 'rank'">
      <div class="text-center mb-8 anim-fade-up">
        <div class="inline-flex items-center justify-center w-16 h-16 rounded-2xl mb-3 shadow-lg" style="background: linear-gradient(135deg, #fbbf24, #f59e0b); box-shadow: 0 12px 24px -8px rgba(245, 158, 11, 0.5);">
          <i class="fa-solid fa-crown text-3xl text-white"></i>
        </div>
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800" style="letter-spacing: -0.03em;">年级积分争霸榜</h2>
        <p class="text-sm text-stone-500 mt-1">为班级荣誉而战，每一分都闪耀</p>
      </div>

      <!-- 颁奖台 Top 3 -->
      <div v-if="leaderboard.length >= 3" class="grid grid-cols-3 gap-3 md:gap-5 mb-6 max-w-3xl mx-auto">
        <!-- 第2名 -->
        <div class="podium-card podium-2 p-4 md:p-5 text-center anim-scale-in delay-2 flex flex-col justify-end">
          <div class="medal medal-2 mx-auto mb-3">2</div>
          <p class="font-bold text-stone-800 text-sm md:text-base truncate">{{ leaderboard[1].name }}</p>
          <p class="font-display font-bold text-xl md:text-2xl text-stone-700 mt-1">{{ leaderboard[1].totalPoints }}</p>
          <p class="text-[10px] text-stone-500 uppercase tracking-wider">points</p>
        </div>
        <!-- 第1名 -->
        <div class="podium-card podium-1 p-4 md:p-6 text-center anim-scale-in delay-1 flex flex-col justify-end">
          <i class="fa-solid fa-crown text-2xl text-amber-500 mb-1"></i>
          <div class="medal medal-1 mx-auto mb-3" style="width: 64px; height: 64px; font-size: 1.75rem;">1</div>
          <p class="font-bold text-stone-800 text-sm md:text-base truncate">{{ leaderboard[0].name }}</p>
          <p class="font-display font-bold text-2xl md:text-3xl text-amber-600 mt-1">{{ leaderboard[0].totalPoints }}</p>
          <p class="text-[10px] text-amber-600 uppercase tracking-wider font-semibold">champion</p>
        </div>
        <!-- 第3名 -->
        <div class="podium-card podium-3 p-4 md:p-5 text-center anim-scale-in delay-3 flex flex-col justify-end">
          <div class="medal medal-3 mx-auto mb-3">3</div>
          <p class="font-bold text-stone-800 text-sm md:text-base truncate">{{ leaderboard[2].name }}</p>
          <p class="font-display font-bold text-xl md:text-2xl text-stone-700 mt-1">{{ leaderboard[2].totalPoints }}</p>
          <p class="text-[10px] text-stone-500 uppercase tracking-wider">points</p>
        </div>
      </div>

      <!-- 排名列表 (第4名及以后) -->
      <div class="max-w-3xl mx-auto card-base p-4 md:p-6 anim-fade-up delay-4">
        <div v-if="leaderboard.length > 3" class="space-y-2">
          <div v-for="(cls, index) in leaderboard.slice(3)" :key="cls.id" class="flex items-center p-3 md:p-4 rounded-2xl transition-all duration-300 hover:bg-emerald-50/50">
            <div class="w-10 h-10 flex items-center justify-center rounded-xl font-display font-bold text-base text-stone-500 bg-stone-100 mr-4">
              {{ index + 4 }}
            </div>
            <div class="flex-1 min-w-0">
              <div class="font-bold text-stone-800 text-base truncate">{{ cls.name }}</div>
            </div>
            <div class="font-display font-bold text-xl text-emerald-600 whitespace-nowrap">
              {{ cls.totalPoints }} <span class="text-xs font-body font-normal text-stone-400">分</span>
            </div>
          </div>
        </div>

        <!-- 如果不足3个，显示全部列表 -->
        <div v-if="leaderboard.length < 3" class="space-y-2">
          <div v-for="(cls, index) in leaderboard" :key="cls.id" class="flex items-center p-3 md:p-4 rounded-2xl transition-all duration-300 hover:bg-emerald-50/50">
            <div class="w-10 h-10 flex items-center justify-center rounded-xl font-display font-bold text-base text-stone-500 bg-stone-100 mr-4">
              {{ index + 1 }}
            </div>
            <div class="flex-1 min-w-0">
              <div class="font-bold text-stone-800 text-base truncate">{{ cls.name }}</div>
            </div>
            <div class="font-display font-bold text-xl text-emerald-600 whitespace-nowrap">
              {{ cls.totalPoints }} <span class="text-xs font-body font-normal text-stone-400">分</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- ====== Tab 6: 个人档案 ====== -->
    <div v-show="currentTab === 'profile'">
      <div class="mb-7 anim-fade-up">
        <h2 class="font-display text-3xl md:text-4xl font-bold text-stone-800" style="letter-spacing: -0.03em;">个人发展档案</h2>
        <p class="text-sm text-stone-500 mt-1">五育并举，全面发展的成长轨迹</p>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <!-- 雷达图 -->
        <div class="card-base p-6 anim-fade-up delay-1">
          <div class="flex items-center gap-3 mb-4">
            <div class="w-10 h-10 rounded-xl flex items-center justify-center text-white" style="background: linear-gradient(135deg, #10b981, #059669);">
              <i class="fa-solid fa-chart-pie text-sm"></i>
            </div>
            <div>
              <h3 class="font-display font-bold text-lg text-stone-800">五育发展雷达图</h3>
              <p class="text-xs text-stone-500">德 · 智 · 体 · 美 · 劳</p>
            </div>
          </div>
          <div class="radar-chart-wrap">
            <BaseChart ref="radarChartRef" :option="radarOption" height="320px" />
          </div>
        </div>

        <!-- 基本信息 -->
        <div class="card-base p-6 anim-fade-up delay-2">
          <div class="flex items-center gap-3 mb-5">
            <div class="w-10 h-10 rounded-xl flex items-center justify-center text-white" style="background: linear-gradient(135deg, #10b981, #059669);">
              <i class="fa-solid fa-id-card text-sm"></i>
            </div>
            <div>
              <h3 class="font-display font-bold text-lg text-stone-800">基本信息</h3>
              <p class="text-xs text-stone-500">个人档案概览</p>
            </div>
          </div>
          <div class="space-y-3">
            <div class="flex justify-between items-center p-4 rounded-2xl bg-stone-50/80 hover:bg-emerald-50/50 transition">
              <span class="text-stone-500 text-sm flex items-center gap-2"><i class="fa-solid fa-user text-stone-400"></i>姓名</span>
              <span class="font-display font-bold text-stone-800">{{ userInfo.name }}</span>
            </div>
            <div class="flex justify-between items-center p-4 rounded-2xl bg-stone-50/80 hover:bg-emerald-50/50 transition">
              <span class="text-stone-500 text-sm flex items-center gap-2"><i class="fa-solid fa-school text-stone-400"></i>班级</span>
              <span class="font-display font-bold text-stone-800">{{ userInfo.className }}</span>
            </div>
            <div class="flex justify-between items-center p-4 rounded-2xl bg-stone-50/80 hover:bg-emerald-50/50 transition">
              <span class="text-stone-500 text-sm flex items-center gap-2"><i class="fa-solid fa-bolt text-stone-400"></i>当前积分</span>
              <span class="font-display font-bold text-emerald-600 text-lg">{{ userInfo.points }}</span>
            </div>
            <div class="flex justify-between items-center p-4 rounded-2xl bg-stone-50/80 hover:bg-emerald-50/50 transition">
              <span class="text-stone-500 text-sm flex items-center gap-2"><i class="fa-solid fa-trophy text-stone-400"></i>积分排名</span>
              <span class="font-display font-bold text-amber-600 text-lg">#{{ userRank }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </AppLayout>
</template>

<style scoped>
/* ===== HERO 积分卡片 (emerald 渐变) ===== */
.hero-points {
    background:
        radial-gradient(circle at 20% 20%, rgba(255, 255, 255, 0.25), transparent 50%),
        linear-gradient(135deg, #047857 0%, #059669 45%, #10b981 100%);
    position: relative;
    overflow: hidden;
}
.hero-points::before {
    content: '';
    position: absolute;
    inset: 0;
    background: url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n2'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='2'/%3E%3CfeColorMatrix values='0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0.15 0'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n2)'/%3E%3C/svg%3E");
    opacity: 0.6;
    mix-blend-mode: overlay;
}
.hero-points > * { position: relative; z-index: 2; }

.points-number {
    font-family: 'Fraunces', serif;
    font-weight: 800;
    font-variation-settings: "opsz" 144;
    line-height: 0.9;
    letter-spacing: -0.04em;
    background: linear-gradient(180deg, #ffffff 0%, #d1fae5 100%);
    -webkit-background-clip: text;
    background-clip: text;
    -webkit-text-fill-color: transparent;
    text-shadow: 0 0 40px rgba(167, 243, 208, 0.4);
}

/* ===== 主按钮 (emerald 渐变) ===== */
.btn-emerald {
    background: linear-gradient(135deg, #10b981, #059669);
    color: white;
    box-shadow: 0 4px 12px -2px rgba(5, 150, 105, 0.4), inset 0 1px 0 rgba(255, 255, 255, 0.25);
    transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}
.btn-emerald:hover {
    transform: translateY(-1px);
    box-shadow: 0 8px 20px -4px rgba(5, 150, 105, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.3);
}
.btn-emerald:active { transform: translateY(0); }

/* ===== 卡片基础 ===== */
.card-base {
    background: rgba(255, 255, 255, 0.8);
    backdrop-filter: blur(16px) saturate(140%);
    -webkit-backdrop-filter: blur(16px) saturate(140%);
    border: 1px solid rgba(255, 255, 255, 0.7);
    border-radius: 1.5rem;
    box-shadow: 0 1px 2px rgba(28, 25, 23, 0.04), 0 8px 24px -8px rgba(28, 25, 23, 0.08);
    transition: transform 0.4s cubic-bezier(0.16, 1, 0.3, 1), box-shadow 0.4s ease;
}
.card-hover:hover {
    transform: translateY(-6px);
    box-shadow: 0 1px 2px rgba(28, 25, 23, 0.04), 0 20px 40px -12px rgba(5, 150, 105, 0.18);
}

/* ===== 颁奖台 (Podium) ===== */
.podium-card {
    position: relative;
    border-radius: 1.5rem;
    transition: transform 0.4s cubic-bezier(0.16, 1, 0.3, 1);
}
.podium-1 {
    background: linear-gradient(180deg, rgba(251, 191, 36, 0.18), rgba(245, 158, 11, 0.06));
    border: 1px solid rgba(251, 191, 36, 0.35);
    transform: translateY(-12px);
    box-shadow: 0 20px 40px -16px rgba(245, 158, 11, 0.35);
}
.podium-2 {
    background: linear-gradient(180deg, rgba(203, 213, 225, 0.18), rgba(148, 163, 184, 0.06));
    border: 1px solid rgba(148, 163, 184, 0.3);
}
.podium-3 {
    background: linear-gradient(180deg, rgba(251, 146, 60, 0.15), rgba(234, 88, 12, 0.05));
    border: 1px solid rgba(251, 146, 60, 0.3);
}

.medal {
    width: 56px; height: 56px;
    border-radius: 50%;
    display: flex; align-items: center; justify-content: center;
    font-family: 'Fraunces', serif;
    font-weight: 800;
    font-size: 1.5rem;
    color: white;
    box-shadow: 0 8px 16px -4px rgba(0, 0, 0, 0.2), inset 0 2px 4px rgba(255, 255, 255, 0.4);
}
.medal-1 { background: linear-gradient(135deg, #fbbf24, #f59e0b); }
.medal-2 { background: linear-gradient(135deg, #cbd5e1, #94a3b8); }
.medal-3 { background: linear-gradient(135deg, #fb923c, #ea580c); }

/* ===== 评价进度条 ===== */
.score-bar {
    height: 8px;
    background: rgba(120, 113, 108, 0.12);
    border-radius: 999px;
    overflow: hidden;
}
.score-fill {
    height: 100%;
    background: linear-gradient(90deg, #10b981, #059669);
    border-radius: 999px;
    transition: width 0.8s cubic-bezier(0.16, 1, 0.3, 1);
}

/* ===== 商城卡片图标区 ===== */
.mall-icon-wrap {
    background:
        radial-gradient(circle at 30% 30%, rgba(16, 185, 129, 0.18), transparent 60%),
        linear-gradient(135deg, #ecfdf5, #d1fae5);
    position: relative;
    overflow: hidden;
}
.mall-icon-wrap::after {
    content: '';
    position: absolute;
    inset: 0;
    background: url("data:image/svg+xml,%3Csvg viewBox='0 0 100 100' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n3'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.8' numOctaves='2'/%3E%3CfeColorMatrix values='0 0 0 0 0.04 0 0 0 0 0.3 0 0 0 0 0.2 0 0 0 0.08 0'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n3)'/%3E%3C/svg%3E");
    opacity: 0.5;
}

/* ===== 历史记录条目 ===== */
.history-row {
    transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
    border-radius: 1rem;
}
.history-row:hover {
    background: rgba(16, 185, 129, 0.06);
    transform: translateX(4px);
}

/* ===== 装饰性图标 ===== */
.deco-icon {
    position: absolute;
    opacity: 0.12;
    pointer-events: none;
}

/* ===== 标签徽章 ===== */
.badge-emerald {
    background: linear-gradient(135deg, rgba(16, 185, 129, 0.15), rgba(5, 150, 105, 0.08));
    color: #047857;
    border: 1px solid rgba(16, 185, 129, 0.25);
}

/* ===== 雷达图容器 ===== */
.radar-chart-wrap {
    border-radius: 1.25rem;
    background: rgba(236, 253, 245, 0.4);
    overflow: hidden;
}

/* ===== line-clamp 兼容 ===== */
.line-clamp-2 {
    display: -webkit-box;
    -webkit-line-clamp: 2;
    -webkit-box-orient: vertical;
    overflow: hidden;
}

/* ===== body 字体覆盖 (font-display 内嵌时) ===== */
.font-body { font-family: 'Noto Sans SC', system-ui, sans-serif; }

/* ===== 入场动画 ===== */
@keyframes fadeUp {
    from { opacity: 0; transform: translateY(20px); }
    to { opacity: 1; transform: translateY(0); }
}
@keyframes fadeIn {
    from { opacity: 0; }
    to { opacity: 1; }
}
@keyframes scaleIn {
    from { opacity: 0; transform: scale(0.92); }
    to { opacity: 1; transform: scale(1); }
}
.anim-fade-up { animation: fadeUp 0.7s cubic-bezier(0.16, 1, 0.3, 1) both; }
.anim-fade-in { animation: fadeIn 0.8s ease both; }
.anim-scale-in { animation: scaleIn 0.6s cubic-bezier(0.16, 1, 0.3, 1) both; }
.delay-1 { animation-delay: 0.08s; }
.delay-2 { animation-delay: 0.16s; }
.delay-3 { animation-delay: 0.24s; }
.delay-4 { animation-delay: 0.32s; }
.delay-5 { animation-delay: 0.40s; }
.delay-6 { animation-delay: 0.48s; }
</style>
