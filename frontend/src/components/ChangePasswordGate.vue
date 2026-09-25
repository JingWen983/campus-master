<script setup lang="ts">
/**
 * 强制首登改密门禁（安全修复 V18 / B2）—— 全局挂载，见 main.ts
 *
 * 行为：当会话带 must_change_password=true 时，弹出**不可关闭**的阻断式改密表单，
 * 主界面在其后不可操作；改密成功后放行（刷新页面以全新状态继续）。
 *
 * 后端契约出处（全部为实测 file:line，见 docs/audit 与 t7 交付）：
 * - 登录响应下发标记：routes_public.cpp:85 / :88
 * - /api/auth/me 下发标记：routes_public.cpp:224
 * - 未改密会话访问业务接口 → 403 + must_change_password:true：auth.h:327-340
 * - 改密端点与全部校验规则：routes_public.cpp:438-536
 *   （原密码正确 :471、新旧不同 :478、≥8 位且同时含字母与数字 :487-489、
 *     弱口令黑名单 :496-507、不得与用户名相同 :509、成功清除标记 :518）
 */
import { ref, onMounted } from 'vue'
import BaseModal from './BaseModal.vue'
import { changePassword, type UserInfo } from '../lib/auth'
import {
  mustChangePassword,
  watchPasswordChangeRequirement,
  syncPasswordChangeRequirement,
  releasePasswordGate,
} from '../lib/passwordGate'

const oldPassword = ref('')
const newPassword = ref('')
const confirmPassword = ref('')
const submitting = ref(false)
const errorMsg = ref('')

/** 与后端 routes_public.cpp:496-507 同源的弱口令黑名单 */
const WEAK_DEFAULTS = [
  'admin123', 'teacher123', 'student123', 'parent123',
  '12345678', '123456789', 'password', 'password1', 'admin@123',
]

function currentUsername(): string {
  try {
    return (JSON.parse(localStorage.getItem('userInfo') || '{}') as UserInfo).username || ''
  } catch {
    return ''
  }
}

/** 前端预校验：与后端逐条对齐，减少无效往返；后端仍为最终判据 */
function validate(): string {
  if (!oldPassword.value || !newPassword.value) return '请填写原密码与新密码'
  if (newPassword.value !== confirmPassword.value) return '两次输入的新密码不一致'
  if (newPassword.value === oldPassword.value) return '新密码不能与原密码相同'
  if (
    newPassword.value.length < 8 ||
    !/[A-Za-z]/.test(newPassword.value) ||
    !/[0-9]/.test(newPassword.value)
  ) {
    return '新密码至少 8 位，且必须同时包含字母和数字'
  }
  if (WEAK_DEFAULTS.includes(newPassword.value)) return '新密码过于常见/可预测，请更换'
  if (newPassword.value === currentUsername()) return '新密码不能与用户名相同'
  return ''
}

async function submit() {
  const invalid = validate()
  if (invalid) {
    errorMsg.value = invalid
    return
  }
  submitting.value = true
  errorMsg.value = ''
  try {
    const res = await changePassword(oldPassword.value, newPassword.value)
    if (!res.ok) {
      errorMsg.value = res.msg
      return
    }
    // 放行主界面；刷新以获得改密后的干净会话状态
    releasePasswordGate()
    oldPassword.value = ''
    newPassword.value = ''
    confirmPassword.value = ''
    location.reload()
  } catch {
    errorMsg.value = '网络错误，请稍后重试'
  } finally {
    submitting.value = false
  }
}

onMounted(() => {
  watchPasswordChangeRequirement()
  syncPasswordChangeRequirement()
})
</script>

<template>
  <BaseModal
    :show="mustChangePassword"
    title="请先修改初始口令"
    icon="fa-key"
    icon-from="#f59e0b"
    icon-to="#b45309"
    max-width="max-w-lg"
    :close-on-backdrop="false"
  >
    <div class="space-y-4">
      <div class="rounded-2xl p-4 text-sm" style="background: linear-gradient(135deg, rgba(245, 158, 11, 0.10), rgba(20, 184, 166, 0.06)); border: 1px solid rgba(245, 158, 11, 0.25); color: #92400e;">
        <p class="font-semibold mb-1 flex items-center">
          <i class="fa-solid fa-shield-halved mr-1.5"></i>首次登录必须修改初始口令
        </p>
        <p class="text-stone-700 leading-relaxed">
          本账号仍在使用系统首次启动时随机生成的初始口令。为保护数据安全，
          修改成功前无法使用其他功能。初始口令见服务端启动时打印的控制台提示。
        </p>
      </div>

      <div>
        <label class="block text-xs font-semibold text-stone-600 mb-1.5">原密码（初始口令）</label>
        <input
          v-model="oldPassword"
          type="password"
          autocomplete="current-password"
          class="input-soft w-full px-4 py-2.5 text-sm"
          placeholder="请输入控制台提示的初始口令"
        >
      </div>

      <div>
        <label class="block text-xs font-semibold text-stone-600 mb-1.5">新密码</label>
        <input
          v-model="newPassword"
          type="password"
          autocomplete="new-password"
          class="input-soft w-full px-4 py-2.5 text-sm"
          placeholder="至少 8 位，需同时包含字母和数字"
        >
      </div>

      <div>
        <label class="block text-xs font-semibold text-stone-600 mb-1.5">确认新密码</label>
        <input
          v-model="confirmPassword"
          type="password"
          autocomplete="new-password"
          class="input-soft w-full px-4 py-2.5 text-sm"
          placeholder="请再次输入新密码"
          @keyup.enter="submit"
        >
      </div>

      <div v-if="errorMsg" class="p-3 bg-rose-50 border border-rose-200 rounded-2xl text-rose-700 text-sm flex items-center gap-2">
        <i class="fa-solid fa-circle-exclamation text-rose-500"></i>
        <span>{{ errorMsg }}</span>
      </div>
    </div>

    <template #footer>
      <button
        type="button"
        :disabled="submitting"
        class="btn-teal px-5 py-2 rounded-xl text-sm font-medium disabled:opacity-60 disabled:cursor-not-allowed flex items-center"
        @click="submit"
      >
        <i v-if="submitting" class="fa-solid fa-circle-notch fa-spin mr-2"></i>
        {{ submitting ? '正在提交...' : '确认修改' }}
      </button>
    </template>
  </BaseModal>
</template>
