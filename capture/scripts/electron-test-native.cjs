/**
 * 仅验证 Electron 能否 require .node（主进程，ABI / delay-load 烟雾测试）。
 * 应用内实际在 preload 加载 capture_addon；二者路径一致即可。
 * pnpm run electron:test-native
 */
const { app } = require('electron')
const { join } = require('path')
const { createRequire } = require('module')

const requireAddon = createRequire(__filename)

app.whenReady().then(() => {
  console.log('[test-native] electron=', process.versions.electron, 'modules=', process.versions.modules)
  const p = join(__dirname, '../native/wgc-addon/capture_addon.node')
  try {
    const a = requireAddon(p)
    console.log('[test-native] require OK', Object.keys(a))
  } catch (e) {
    console.error('[test-native] require FAIL', e)
  }
  setTimeout(() => app.quit(), 1500)
})
