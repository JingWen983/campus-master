<script setup lang="ts">
/**
 * 能量大屏 —— 免登录实时排行榜（screen.html 入口）
 *
 * 三栏：学生总榜 / 年级班级榜 / 班级总榜，各自 10 秒轮询刷新；
 * 任一栏内容超出可视高度时自动匀速滚动，到底后反向回滚循环，不足一屏静止；
 * 深色大字号设计，适配 1920×1080 电视/LED 大屏。
 *
 * 数据源（后端 routes_public.cpp，均免登录）：
 *   GET /api/screen/leaderboard  学生总榜
 *   GET /api/screen/grade-rank   年级内班级榜
 *   GET /api/screen/class-rank   班级总榜
 */
import { ref, onMounted, onUnmounted } from 'vue'
import { api } from '../../lib/api'

interface StudentRow { rank: number; name: string; className: string; points: number }
interface ClassRow { rank: number; name: string; totalPoints: number; studentCount?: number }
interface GradeGroup { grade: string; classes: ClassRow[] }

const REFRESH_MS = 10_000   // 需求确认：大屏 10 秒轮询

const students = ref<StudentRow[]>([])
const grades = ref<GradeGroup[]>([])
const classes = ref<ClassRow[]>([])
const updatedAt = ref('')
const online = ref(true)

const listRefs = {
  students: ref<HTMLElement | null>(null),
  grades: ref<HTMLElement | null>(null),
  classes: ref<HTMLElement | null>(null),
}
let rafIds: number[] = []
let pollTimer: number | undefined

// ===== 自动滚动：匀速下滚 → 到底反向 → 回顶循环；不足一屏静止 =====
function setupAutoScroll(el: HTMLElement) {
  let offset = 0
  let direction = 1
  let paused = false
  el.addEventListener('mouseenter', () => (paused = true))
  el.addEventListener('mouseleave', () => (paused = false))

  function tick() {
    if (!paused && el.scrollHeight > el.clientHeight + 4) {
      offset += direction * 0.6   // 0.6px/帧 ≈ 36px/s，肉眼可跟读
      const max = el.scrollHeight - el.clientHeight
      if (offset >= max) { offset = max; direction = -1 }
      if (offset <= 0) { offset = 0; direction = 1 }
      el.scrollTop = offset
    }
    rafIds.push(requestAnimationFrame(tick))
  }
  rafIds.push(requestAnimationFrame(tick))
}

async function fetchAll() {
  const [a, b, c] = await Promise.all([
    api.get<{ students: StudentRow[]; updated_at: string }>('/api/screen/leaderboard'),
    api.get<GradeGroup[]>('/api/screen/grade-rank'),
    api.get<ClassRow[]>('/api/screen/class-rank'),
  ])
  if (a.code === 200 && a.data) {
    students.value = a.data.students || []
    updatedAt.value = a.data.updated_at || ''
    online.value = true
  } else {
    online.value = false
  }
  if (b.code === 200 && b.data) grades.value = b.data
  if (c.code === 200 && c.data) classes.value = c.data
}

function rankClass(rank: number): string {
  if (rank === 1) return 'rank-gold'
  if (rank === 2) return 'rank-silver'
  if (rank === 3) return 'rank-bronze'
  return ''
}

onMounted(async () => {
  await fetchAll()
  // 等 DOM 渲染出列表后再绑定自动滚动
  requestAnimationFrame(() => {
    if (listRefs.students.value) setupAutoScroll(listRefs.students.value)
    if (listRefs.grades.value) setupAutoScroll(listRefs.grades.value)
    if (listRefs.classes.value) setupAutoScroll(listRefs.classes.value)
  })
  pollTimer = window.setInterval(fetchAll, REFRESH_MS)
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
  rafIds.forEach(id => cancelAnimationFrame(id))
})
</script>

<template>
  <div class="screen-root">
    <!-- 顶部标题栏 -->
    <header class="screen-header">
      <div class="header-left">
        <i class="fa-solid fa-bolt header-icon"></i>
        <h1>校园文明能量站 · 实时排行榜</h1>
      </div>
      <div class="header-right">
        <span class="live-dot" :class="online ? 'on' : 'off'"></span>
        <span class="live-text">{{ online ? '实时' : '连接中断' }}</span>
        <span class="update-time" v-if="updatedAt">数据更新 {{ updatedAt }}</span>
      </div>
    </header>

    <!-- 三栏榜单 -->
    <main class="screen-body">
      <!-- 栏 1：学生总榜 -->
      <section class="panel">
        <h2 class="panel-title"><i class="fa-solid fa-ranking-star"></i> 学生总积分榜</h2>
        <div class="panel-list" ref="listRefs.students.value">
          <div v-if="students.length === 0" class="empty">暂无数据</div>
          <div v-for="s in students" :key="s.rank" class="row" :class="rankClass(s.rank)">
            <span class="row-rank">{{ s.rank }}</span>
            <span class="row-name">{{ s.name }}</span>
            <span class="row-class">{{ s.className }}</span>
            <span class="row-points">{{ s.points }}</span>
          </div>
        </div>
      </section>

      <!-- 栏 2：年级班级榜 -->
      <section class="panel">
        <h2 class="panel-title"><i class="fa-solid fa-layer-group"></i> 年级积分排名</h2>
        <div class="panel-list" ref="listRefs.grades.value">
          <div v-if="grades.length === 0" class="empty">暂无数据</div>
          <div v-for="g in grades" :key="g.grade" class="grade-group">
            <div class="grade-title">{{ g.grade }}年级</div>
            <div v-for="c in g.classes" :key="c.name" class="row" :class="rankClass(c.rank)">
              <span class="row-rank">{{ c.rank }}</span>
              <span class="row-name">{{ c.name }}</span>
              <span class="row-class">{{ c.studentCount }}人</span>
              <span class="row-points">{{ c.totalPoints }}</span>
            </div>
          </div>
        </div>
      </section>

      <!-- 栏 3：班级总榜 -->
      <section class="panel">
        <h2 class="panel-title"><i class="fa-solid fa-trophy"></i> 班级积分排名</h2>
        <div class="panel-list" ref="listRefs.classes.value">
          <div v-if="classes.length === 0" class="empty">暂无数据</div>
          <div v-for="c in classes" :key="c.name" class="row" :class="rankClass(c.rank)">
            <span class="row-rank">{{ c.rank }}</span>
            <span class="row-name">{{ c.name }}</span>
            <span class="row-points">{{ c.totalPoints }}</span>
          </div>
        </div>
      </section>
    </main>
  </div>
</template>

<style scoped>
.screen-root {
  position: fixed;
  inset: 0;
  display: flex;
  flex-direction: column;
  background: radial-gradient(ellipse at top, #0f2440 0%, #080f1f 60%, #050a14 100%);
  color: #e2e8f0;
  font-family: 'Noto Sans SC', sans-serif;
  overflow: hidden;
}

/* ===== 顶部 ===== */
.screen-header {
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 1.4rem 2.4rem;
  background: linear-gradient(90deg, rgba(56, 130, 246, 0.16), rgba(56, 130, 246, 0));
  border-bottom: 1px solid rgba(96, 165, 250, 0.22);
}
.header-left { display: flex; align-items: center; gap: 1rem; }
.header-icon {
  font-size: 2rem;
  color: #38bdf8;
  filter: drop-shadow(0 0 12px rgba(56, 189, 248, 0.8));
}
.screen-header h1 {
  font-size: 2rem;
  font-weight: 800;
  letter-spacing: 0.08em;
  background: linear-gradient(90deg, #7dd3fc, #38bdf8, #818cf8);
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
}
.header-right { display: flex; align-items: center; gap: 0.9rem; font-size: 1rem; color: #94a3b8; }
.live-dot { width: 0.7rem; height: 0.7rem; border-radius: 50%; }
.live-dot.on { background: #22c55e; box-shadow: 0 0 10px #22c55e; animation: pulse 1.6s infinite; }
.live-dot.off { background: #ef4444; box-shadow: 0 0 10px #ef4444; }
@keyframes pulse { 50% { opacity: 0.35; } }

/* ===== 三栏 ===== */
.screen-body {
  flex: 1;
  display: grid;
  grid-template-columns: 1fr 1fr 1fr;
  gap: 1.4rem;
  padding: 1.4rem 2.4rem 2rem;
  min-height: 0;   /* 让子面板可 overflow 滚动 */
}
.panel {
  display: flex;
  flex-direction: column;
  min-height: 0;
  background: rgba(15, 30, 55, 0.55);
  border: 1px solid rgba(96, 165, 250, 0.18);
  border-radius: 1.2rem;
  overflow: hidden;
}
.panel-title {
  flex-shrink: 0;
  font-size: 1.25rem;
  font-weight: 700;
  padding: 1rem 1.4rem;
  color: #93c5fd;
  border-bottom: 1px solid rgba(96, 165, 250, 0.18);
  background: rgba(30, 58, 138, 0.28);
}
.panel-title i { margin-right: 0.6rem; }
.panel-list { flex: 1; overflow-y: auto; padding: 0.7rem 1.1rem; scrollbar-width: none; }
.panel-list::-webkit-scrollbar { display: none; }

.empty { padding: 3rem 0; text-align: center; color: #64748b; font-size: 1.1rem; }

/* ===== 行 ===== */
.row {
  display: grid;
  grid-template-columns: 3.4rem 1fr 7.5rem 6.5rem;
  align-items: center;
  gap: 0.6rem;
  padding: 0.62rem 0.9rem;
  margin-bottom: 0.45rem;
  border-radius: 0.65rem;
  font-size: 1.12rem;
  background: rgba(30, 41, 59, 0.42);
}
.panel:last-child .row { grid-template-columns: 3.4rem 1fr 6.5rem; }
.row-rank {
  font-weight: 800;
  font-size: 1.2rem;
  color: #64748b;
  text-align: center;
}
.row-name { font-weight: 600; color: #f1f5f9; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.row-class { color: #94a3b8; font-size: 0.95rem; text-align: right; white-space: nowrap; }
.row-points {
  font-weight: 800;
  color: #fbbf24;
  font-size: 1.22rem;
  text-align: right;
  font-variant-numeric: tabular-nums;
}

/* 前三名高亮 */
.rank-gold { background: linear-gradient(90deg, rgba(251, 191, 36, 0.20), rgba(251, 191, 36, 0.05)); border: 1px solid rgba(251, 191, 36, 0.45); }
.rank-gold .row-rank { color: #fbbf24; }
.rank-silver { background: linear-gradient(90deg, rgba(203, 213, 225, 0.16), rgba(203, 213, 225, 0.04)); border: 1px solid rgba(203, 213, 225, 0.38); }
.rank-silver .row-rank { color: #cbd5e1; }
.rank-bronze { background: linear-gradient(90deg, rgba(217, 119, 6, 0.18), rgba(217, 119, 6, 0.05)); border: 1px solid rgba(217, 119, 6, 0.42); }
.rank-bronze .row-rank { color: #d97706; }

/* 年级分组标题 */
.grade-group { margin-bottom: 0.9rem; }
.grade-title {
  font-size: 1.05rem;
  font-weight: 700;
  color: #818cf8;
  padding: 0.45rem 0.3rem 0.55rem;
  letter-spacing: 0.12em;
}

/* 中屏适配（投影仪 1280×720 等） */
@media (max-width: 1600px) {
  .screen-header h1 { font-size: 1.5rem; }
  .header-icon { font-size: 1.5rem; }
  .row { font-size: 0.98rem; padding: 0.5rem 0.7rem; }
  .row-points { font-size: 1.05rem; }
  .screen-body { gap: 1rem; padding: 1rem 1.4rem 1.6rem; }
}
</style>
