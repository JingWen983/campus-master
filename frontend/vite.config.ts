import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'
import pkg from './package.json'

// 多入口：7 个独立 HTML 页面（保留原 URL 结构：/ /admin.html /teacher.html ... /screen.html /logistics.html）
// GitHub Pages 部署时通过环境变量 VITE_BASE 配置子路径（如 /campus-master/）
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    plugins: [vue()],
    base: env.VITE_BASE || '/',
    define: {
      // 显式定义 VITE_USE_MOCK，确保构建时替换为字符串字面量
      'import.meta.env.VITE_USE_MOCK': JSON.stringify(env.VITE_USE_MOCK || 'false'),
      // 应用版本号：发布构建用 VITE_APP_VERSION 注入（release.yml 传 tag），
      // 未注入时回落到 package.json 的 version（检查更新用）
      'import.meta.env.VITE_APP_VERSION': JSON.stringify(env.VITE_APP_VERSION || pkg.version),
    },
    build: {
      outDir: 'dist',
      emptyOutDir: true,
      rollupOptions: {
        input: {
          index: resolve(__dirname, 'index.html'),
          admin: resolve(__dirname, 'admin.html'),
          teacher: resolve(__dirname, 'teacher.html'),
          student: resolve(__dirname, 'student.html'),
          parent: resolve(__dirname, 'parent.html'),
          screen: resolve(__dirname, 'screen.html'),
          logistics: resolve(__dirname, 'logistics.html'),
        },
      },
    },
    server: {
      port: 5173,
      proxy: {
        // 开发时把 /api 请求转发到 C++ 后端（默认 8080）
        '/api': {
          target: 'http://127.0.0.1:8080',
          changeOrigin: true,
        },
      },
    },
  }
})
