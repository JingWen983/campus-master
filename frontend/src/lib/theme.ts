/**
 * 主题色配置 —— 统一 4 角色主题色，避免散落硬编码
 *
 * 角色 → 主题色映射：
 *   admin   → rose
 *   teacher → teal
 *   student → emerald
 *   parent  → indigo
 *   login   → amber
 */
export type ThemeName = 'rose' | 'teal' | 'emerald' | 'indigo' | 'amber'

export interface ThemeConfig {
  /** 主品牌渐变 from */
  from: string
  /** 主品牌渐变 to */
  to: string
  /** 主品牌实色（用于图标、强调） */
  solid: string
  /** 浅底色（徽章、hover） */
  soft: string
  /** 文字色 */
  text: string
  /** 阴影色（带透明度） */
  shadow: string
  /** logo 渐变 class */
  logoGradient: string
  /** 主题色按钮 class */
  btnPrimary: string
}

export const THEMES: Record<ThemeName, ThemeConfig> = {
  rose: {
    from: 'from-rose-500',
    to: 'to-rose-800',
    solid: '#e11d48',
    soft: 'bg-rose-50',
    text: 'text-rose-700',
    shadow: 'shadow-rose-900/20',
    logoGradient: 'bg-gradient-to-br from-rose-500 to-rose-800',
    btnPrimary: 'bg-rose-600 hover:bg-rose-700',
  },
  teal: {
    from: 'from-teal-400',
    to: 'to-teal-700',
    solid: '#0d9488',
    soft: 'bg-teal-50',
    text: 'text-teal-700',
    shadow: 'shadow-teal-900/20',
    logoGradient: 'bg-gradient-to-br from-teal-400 to-teal-700',
    btnPrimary: 'bg-teal-600 hover:bg-teal-700',
  },
  emerald: {
    from: 'from-emerald-400',
    to: 'to-emerald-700',
    solid: '#059669',
    soft: 'bg-emerald-50',
    text: 'text-emerald-700',
    shadow: 'shadow-emerald-900/20',
    logoGradient: 'bg-gradient-to-br from-emerald-400 to-emerald-700',
    btnPrimary: 'bg-emerald-600 hover:bg-emerald-700',
  },
  indigo: {
    from: 'from-indigo-500',
    to: 'to-indigo-800',
    solid: '#4f46e5',
    soft: 'bg-indigo-50',
    text: 'text-indigo-700',
    shadow: 'shadow-indigo-900/20',
    logoGradient: 'bg-gradient-to-br from-indigo-500 to-indigo-800',
    btnPrimary: 'bg-indigo-600 hover:bg-indigo-700',
  },
  amber: {
    from: 'from-amber-500',
    to: 'to-amber-700',
    solid: '#d97706',
    soft: 'bg-amber-50',
    text: 'text-amber-700',
    shadow: 'shadow-amber-900/20',
    logoGradient: 'bg-gradient-to-br from-amber-500 to-amber-700',
    btnPrimary: 'bg-amber-600 hover:bg-amber-700',
  },
}

export function useTheme(name: ThemeName): ThemeConfig {
  return THEMES[name]
}

/** 角色 ID → 主题色（用于 AppLayout 自动选色） */
export const ROLE_THEME: Record<number, ThemeName> = {
  1: 'rose', // admin
  2: 'teal', // teacher
  3: 'emerald', // student
  4: 'indigo', // parent
}
