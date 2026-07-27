<script setup lang="ts">
/**
 * 登录页 —— 迁移自 index.html
 *
 * 功能：
 * - 已登录自动跳转（mounted 调 /api/auth/me）
 * - 用户名 / 密码表单，密码显隐切换
 * - 登录：auth.login() 内部先 /api/auth/login 再回退 /api/parent/login，CSRF 自动存储
 * - 演示账号一键填充（admin/teacher/student/parent）
 * - 错误提示带 shake 动画（替换原 alert）
 */
import { ref, onMounted } from 'vue'
import { login } from '../lib/auth'
import { apiRequest } from '../lib/api'
import { getRoleHome } from '../lib/auth'

const username = ref('')
const password = ref('')
const showPassword = ref(false)
const loading = ref(false)
const errorMsg = ref('')

onMounted(async () => {
  // 已登录则自动跳转
  try {
    const res = await apiRequest<{ role_id: number }>('GET', '/api/auth/me')
    if (res.code === 200 && res.data?.role_id) {
      location.href = getRoleHome(res.data.role_id)
    }
  } catch {
    // 未登录，正常显示
  }
})

async function doLogin() {
  if (!username.value || !password.value) {
    errorMsg.value = '请输入用户名和密码'
    return
  }
  loading.value = true
  errorMsg.value = ''
  try {
    const ok = await login(username.value, password.value)
    if (!ok) {
      errorMsg.value = '用户名或密码错误'
    }
  } catch {
    errorMsg.value = '网络错误，请稍后重试'
  } finally {
    loading.value = false
  }
}

function fillTestAccount(u: string, p: string) {
  username.value = u
  password.value = p
  errorMsg.value = ''
}
</script>

<template>
  <div class="login-page min-h-screen flex items-center justify-center p-4 md:p-8 relative overflow-hidden grain">
    <!-- 背景浮动光斑 -->
    <div class="blob bg-amber-300/30 w-72 h-72 top-10 left-10"></div>
    <div class="blob bg-emerald-300/20 w-96 h-96 bottom-10 right-10" style="animation-delay: -4s;"></div>
    <div class="blob bg-rose-300/20 w-64 h-64 top-1/2 left-1/2" style="animation-delay: -8s;"></div>

    <div class="relative z-10 w-full max-w-5xl grid lg:grid-cols-2 gap-8 lg:gap-16 items-center">
      <!-- 左侧：品牌叙事区（大屏可见） -->
      <div class="hidden lg:block enter-up">
        <div class="flex items-center gap-3 mb-8">
          <div class="w-12 h-12 rounded-2xl bg-gradient-to-br from-amber-500 to-amber-700 flex items-center justify-center shadow-lg shadow-amber-500/30">
            <i class="fa-solid fa-leaf text-white text-xl"></i>
          </div>
          <span class="font-display text-2xl font-semibold text-stone-800">文明能量站</span>
        </div>

        <h1 class="font-display text-6xl xl:text-7xl font-black leading-[0.95] text-stone-900 mb-6 tracking-tight">
          点燃<span class="italic text-amber-600">每一份</span><br>
          文明微光
        </h1>

        <p class="text-lg text-stone-600 leading-relaxed mb-10 max-w-md">
          以积分激励行为，以数据见证成长。<br>
          让校园里的每一次善举，都被看见、被记录、被嘉奖。
        </p>

        <div class="flex gap-8">
          <div>
            <div class="font-display text-3xl font-bold text-stone-900">3<span class="text-amber-600">+</span></div>
            <div class="text-sm text-stone-500 mt-1">角色协同</div>
          </div>
          <div class="w-px bg-stone-300"></div>
          <div>
            <div class="font-display text-3xl font-bold text-stone-900">5<span class="text-emerald-600">维</span></div>
            <div class="text-sm text-stone-500 mt-1">五育评价</div>
          </div>
          <div class="w-px bg-stone-300"></div>
          <div>
            <div class="font-display text-3xl font-bold text-stone-900">∞</div>
            <div class="text-sm text-stone-500 mt-1">成长可能</div>
          </div>
        </div>
      </div>

      <!-- 右侧：登录卡片 -->
      <div class="enter-up enter-delay-2">
        <div class="deco-border bg-white/70 backdrop-blur-xl rounded-3xl shadow-2xl shadow-stone-900/10 overflow-hidden">
          <div class="px-8 pt-10 pb-2 text-center">
            <div class="lg:hidden w-16 h-16 rounded-2xl bg-gradient-to-br from-amber-500 to-amber-700 flex items-center justify-center mx-auto mb-5 shadow-lg shadow-amber-500/30">
              <i class="fa-solid fa-leaf text-white text-2xl"></i>
            </div>
            <h2 class="font-display text-3xl font-bold text-stone-900">校园管理系统</h2>
            <p class="text-stone-500 mt-2 text-sm">登录以进入你的能量空间</p>
          </div>

          <div class="p-8 pt-6">
            <form @submit.prevent="doLogin">
              <div class="mb-5 enter-up enter-delay-3">
                <label class="block text-xs font-semibold text-stone-500 uppercase tracking-wider mb-2 ml-1">用户名 / 子女学号</label>
                <div class="field-wrap">
                  <input
                    v-model.trim="username"
                    type="text"
                    autocomplete="username"
                    placeholder="请输入用户名 / 子女学号"
                    class="field-input w-full px-5 py-4 border border-stone-200 rounded-2xl text-stone-800 placeholder-stone-400 focus:outline-none"
                    required
                  >
                </div>
              </div>

              <div class="mb-6 enter-up enter-delay-4">
                <label class="block text-xs font-semibold text-stone-500 uppercase tracking-wider mb-2 ml-1">密码 / 家长密码</label>
                <div class="field-wrap relative">
                  <input
                    v-model="password"
                    :type="showPassword ? 'text' : 'password'"
                    autocomplete="current-password"
                    placeholder="请输入密码 / 家长密码"
                    class="field-input w-full px-5 py-4 pr-14 border border-stone-200 rounded-2xl text-stone-800 placeholder-stone-400 focus:outline-none"
                    required
                  >
                  <button
                    type="button"
                    @click="showPassword = !showPassword"
                    class="absolute right-4 top-1/2 -translate-y-1/2 w-9 h-9 rounded-xl flex items-center justify-center text-stone-400 hover:text-amber-600 hover:bg-amber-50 transition"
                  >
                    <i :class="showPassword ? 'fa-solid fa-eye-slash' : 'fa-solid fa-eye'"></i>
                  </button>
                </div>
              </div>

              <Transition name="shake">
                <div v-if="errorMsg" class="mb-5 p-4 bg-rose-50 border border-rose-200 rounded-2xl text-rose-700 text-sm flex items-center gap-3">
                  <i class="fa-solid fa-circle-exclamation text-rose-500"></i>
                  <span>{{ errorMsg }}</span>
                </div>
              </Transition>

              <button
                type="submit"
                :disabled="loading"
                class="btn-primary w-full text-white py-4 rounded-2xl font-semibold text-base flex items-center justify-center gap-2 disabled:opacity-60 disabled:cursor-not-allowed disabled:transform-none"
              >
                <template v-if="loading">
                  <i class="fa-solid fa-circle-notch fa-spin"></i>
                  <span>正在进入...</span>
                </template>
                <template v-else>
                  <span>登 录</span>
                  <i class="fa-solid fa-arrow-right-long"></i>
                </template>
              </button>
            </form>

            <!-- 演示账号 -->
            <div class="mt-8 pt-6 border-t border-stone-200/60">
              <p class="text-center text-xs text-stone-400 uppercase tracking-wider mb-4">演示账号 · 一键填充</p>
              <div class="grid grid-cols-4 gap-3">
                <button type="button" @click="fillTestAccount('admin', 'admin123')"
                  class="role-chip group p-4 bg-rose-50/80 border border-rose-200/60 rounded-2xl text-center hover:border-rose-400">
                  <div class="w-10 h-10 mx-auto rounded-xl bg-rose-500 flex items-center justify-center mb-2 group-hover:scale-110 transition">
                    <i class="fa-solid fa-user-shield text-white"></i>
                  </div>
                  <div class="text-xs font-bold text-rose-700">管理员</div>
                </button>
                <button type="button" @click="fillTestAccount('teacher', 'teacher123')"
                  class="role-chip group p-4 bg-teal-50/80 border border-teal-200/60 rounded-2xl text-center hover:border-teal-400">
                  <div class="w-10 h-10 mx-auto rounded-xl bg-teal-600 flex items-center justify-center mb-2 group-hover:scale-110 transition">
                    <i class="fa-solid fa-chalkboard-user text-white"></i>
                  </div>
                  <div class="text-xs font-bold text-teal-700">教师</div>
                </button>
                <button type="button" @click="fillTestAccount('student', 'student123')"
                  class="role-chip group p-4 bg-emerald-50/80 border border-emerald-200/60 rounded-2xl text-center hover:border-emerald-400">
                  <div class="w-10 h-10 mx-auto rounded-xl bg-emerald-600 flex items-center justify-center mb-2 group-hover:scale-110 transition">
                    <i class="fa-solid fa-user-graduate text-white"></i>
                  </div>
                  <div class="text-xs font-bold text-emerald-700">学生</div>
                </button>
                <button type="button" @click="fillTestAccount('parent', 'parent123')"
                  class="role-chip group p-4 bg-amber-50/80 border border-amber-200/60 rounded-2xl text-center hover:border-amber-400">
                  <div class="w-10 h-10 mx-auto rounded-xl bg-amber-600 flex items-center justify-center mb-2 group-hover:scale-110 transition">
                    <i class="fa-solid fa-family text-white"></i>
                  </div>
                  <div class="text-xs font-bold text-amber-700">家长</div>
                </button>
              </div>
            </div>
          </div>
        </div>

        <p class="text-center text-xs text-stone-400 mt-6 font-display italic">— 让每一份善意都有回响 —</p>
      </div>
    </div>
  </div>
</template>

<style scoped>
.login-page {
  background-image:
    radial-gradient(at 15% 20%, rgba(217, 119, 6, 0.08) 0px, transparent 50%),
    radial-gradient(at 85% 80%, rgba(5, 150, 105, 0.06) 0px, transparent 50%),
    radial-gradient(at 50% 50%, rgba(190, 18, 60, 0.04) 0px, transparent 50%);
}

.field-wrap {
  position: relative;
  transition: transform 0.3s ease;
}
.field-wrap:focus-within {
  transform: translateX(4px);
}
.field-input {
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  background: rgba(255, 255, 255, 0.6);
  backdrop-filter: blur(8px);
}
.field-input:focus {
  background: rgba(255, 255, 255, 0.95);
  box-shadow: 0 0 0 4px rgba(217, 119, 6, 0.12);
  border-color: #d97706;
}

.btn-primary {
  background: linear-gradient(135deg, #d97706 0%, #92400e 100%);
  box-shadow: 0 8px 24px -8px rgba(217, 119, 6, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.2);
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}
.btn-primary:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 12px 32px -8px rgba(217, 119, 6, 0.6), inset 0 1px 0 rgba(255, 255, 255, 0.3);
}
.btn-primary:active:not(:disabled) {
  transform: translateY(0);
}

.role-chip {
  transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
  backdrop-filter: blur(8px);
}
.role-chip:hover {
  transform: translateY(-3px) scale(1.03);
}

.deco-border {
  position: relative;
}
.deco-border::before {
  content: '';
  position: absolute;
  inset: -1px;
  border-radius: inherit;
  padding: 1px;
  background: linear-gradient(135deg, rgba(217, 119, 6, 0.3), rgba(5, 150, 105, 0.2), rgba(190, 18, 60, 0.2));
  -webkit-mask: linear-gradient(#fff 0 0) content-box, linear-gradient(#fff 0 0);
  -webkit-mask-composite: xor;
  mask-composite: exclude;
  pointer-events: none;
}

.shake-enter-active {
  animation: shake 0.4s;
}
@keyframes shake {
  0%, 100% { transform: translateX(0); }
  25% { transform: translateX(-8px); }
  75% { transform: translateX(8px); }
}
</style>
