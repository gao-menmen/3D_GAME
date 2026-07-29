import { defineConfig } from 'vite';

export default defineConfig({
  // 开发服务器配置
  server: {
    port: 5173,
    open: true,
  },
  // 生产构建配置
  build: {
    target: 'es2022',
    outDir: 'dist',
    sourcemap: false,
  },
});
