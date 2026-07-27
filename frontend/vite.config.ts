import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'

// 多入口：5 个独立 HTML 页面（保留原 URL 结构：/ /admin.html /teacher.html ...）
export default defineConfig({
  plugins: [vue()],
  base: '/',
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
})
