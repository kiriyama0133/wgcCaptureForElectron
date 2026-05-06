/**
 * 必须用 cmake-js 针对 **Electron** 重新生成 CMake 缓存并链接 Electron 的 node.lib。
 * 若以前用默认 Node 编过，`build/` 里会残留 NODE_RUNTIME=node，此时复制 .node 仍会一加载就崩。
 *
 * 用法：在 capture 目录执行 `pnpm run build:addon`
 */
import { spawnSync } from 'node:child_process'
import { copyFileSync, existsSync, mkdirSync, readFileSync } from 'node:fs'
import { createRequire } from 'node:module'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = dirname(fileURLToPath(import.meta.url))
const require = createRequire(import.meta.url)

const electronPkg = join(__dirname, '../node_modules/electron/package.json')
const electronVer = require(electronPkg).version
const addonRoot = join(__dirname, '../../glfwExample')

console.log('[build:addon] cmake 工程:', addonRoot)
console.log('[build:addon] Electron:', electronVer)

const npx = process.platform === 'win32' ? 'npx.cmd' : 'npx'

function run(args, label) {
  console.log('[build:addon]', label, args.join(' '))
  const r = spawnSync(npx, args, { cwd: addonRoot, stdio: 'inherit', shell: true })
  if ((r.status ?? 1) !== 0) {
    console.error('[build:addon] 失败:', label)
    process.exit(r.status ?? 1)
  }
}

/** 清掉旧缓存，否则会一直链接「系统 Node」的 node.lib */
run(['cmake-js', 'clean'], 'clean')

run(
  ['cmake-js', 'compile', '--runtime=electron', `--runtime-version=${electronVer}`],
  'compile(electron)'
)

const cacheFile = join(addonRoot, 'build/CMakeCache.txt')
if (existsSync(cacheFile)) {
  const cache = readFileSync(cacheFile, 'utf8')
  const rtLine = cache.split('\n').find((l) => l.startsWith('NODE_RUNTIME'))
  if (!rtLine || !/electron/i.test(rtLine)) {
    console.error('[build:addon] CMakeCache 未指向 electron runtime，当前行:', rtLine ?? '(无)')
    console.error('[build:addon] 请先删除文件夹:', join(addonRoot, 'build'), '再重跑本脚本')
    process.exit(1)
  }
}

const release = join(addonRoot, 'build/Release/capture_addon.node')
const debug = join(addonRoot, 'build/Debug/capture_addon.node')
const src = existsSync(release) ? release : debug
if (!existsSync(src)) {
  console.error('[build:addon] 未找到:', release, debug)
  process.exit(1)
}

const dstDir = join(__dirname, '../native/wgc-addon')
const dst = join(dstDir, 'capture_addon.node')
mkdirSync(dstDir, { recursive: true })
copyFileSync(src, dst)
console.log('[build:addon] 已复制 ->', dst)
