import { contextBridge } from 'electron'
import { join, dirname } from 'path'
import { existsSync } from 'fs'
import { fileURLToPath } from 'node:url'
import { createRequire } from 'node:module'
import { electronAPI } from '@electron-toolkit/preload'

const __dirname = dirname(fileURLToPath(import.meta.url))
const require = createRequire(import.meta.url)

const api = {}

/** 懒加载：避免 preload 顶层 require 失败导致整页无法注入 API */
let addon = null
let addonTried = false

function loadAddon() {
  if (addonTried) return addon
  addonTried = true
  if (process.env.SKIP_NATIVE === '1') return null
  const p = join(__dirname, '../../native/wgc-addon/capture_addon.node')
  if (!existsSync(p)) {
    console.error('[preload] 未找到', p)
    return null
  }
  try {
    addon = require(p)
    return addon
  } catch (e) {
    console.error('[preload] require .node 失败', e)
    return null
  }
}

function getAddon() {
  return loadAddon()
}

const captureAPI = {
  start: () => {
    if (process.env.SKIP_NATIVE === '1') return Promise.resolve()
    const a = getAddon()
    if (!a) throw new Error('native addon not loaded')
    return Promise.resolve(a.start())
  },
  stop: () => {
    if (process.env.SKIP_NATIVE === '1') return undefined
    const a = getAddon()
    if (!a) return undefined
    return a.stop()
  },
  getSize: () => {
    if (process.env.SKIP_NATIVE === '1') return { width: 800, height: 600 }
    const a = getAddon()
    if (!a) return { width: 0, height: 0 }
    return a.getSize()
  },
  getFrame: () => {
    if (process.env.SKIP_NATIVE === '1') return null
    const a = getAddon()
    if (!a) return null
    return a.getFrame()
  }
}

if (process.contextIsolated) {
  try {
    contextBridge.exposeInMainWorld('captureAPI', captureAPI)
    contextBridge.exposeInMainWorld('electron', electronAPI)
    contextBridge.exposeInMainWorld('api', api)
  } catch (error) {
    console.error('[preload] expose failed:', error)
  }
} else {
  window.captureAPI = captureAPI
  window.electron = electronAPI
  window.api = api
}
