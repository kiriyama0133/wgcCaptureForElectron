import { contextBridge, ipcRenderer } from 'electron'
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

const windowControls = {
  minimize: () => ipcRenderer.invoke('capture:win-minimize'),
  toggleMaximize: () => ipcRenderer.invoke('capture:win-toggle-maximize'),
  close: () => ipcRenderer.invoke('capture:win-close'),
  isMaximized: () => ipcRenderer.invoke('capture:win-is-maximized'),
  /** @param {(maximized: boolean) => void} callback */
  onMaximizedChange(callback) {
    const channel = 'capture:win-maximized-changed'
    const listener = (_event, maximized) => {
      callback(maximized)
    }
    ipcRenderer.on(channel, listener)
    return () => ipcRenderer.removeListener(channel, listener)
  }
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
  },
  /** @param {string} outputPath UTF-8 路径，如 C:\\\\out\\\\cap.mp4 */
  startRecording: (outputPath, config) => {
    if (process.env.SKIP_NATIVE === '1') return Promise.resolve()
    const a = getAddon()
    if (!a) throw new Error('native addon not loaded')
    return Promise.resolve(a.startRecording(outputPath, config))
  },
  stopRecording: () => {
    if (process.env.SKIP_NATIVE === '1') return undefined
    const a = getAddon()
    if (!a) return undefined
    return a.stopRecording()
  },
  isRecording: () => {
    if (process.env.SKIP_NATIVE === '1') return false
    const a = getAddon()
    if (!a) return false
    return a.isRecording()
  },
  /** @returns {Promise<Array<{ id: string, name: string }>>} */
  getAudioOutputDevices: () => {
    if (process.env.SKIP_NATIVE === '1') return Promise.resolve([])
    const a = getAddon()
    if (!a) return Promise.resolve([])
    return Promise.resolve(a.getAudioOutputDevices())
  }
}

if (process.contextIsolated) {
  try {
    contextBridge.exposeInMainWorld('captureAPI', captureAPI)
    contextBridge.exposeInMainWorld('windowControls', windowControls)
    contextBridge.exposeInMainWorld('electron', electronAPI)
    contextBridge.exposeInMainWorld('api', api)
  } catch (error) {
    console.error('[preload] expose failed:', error)
  }
} else {
  window.captureAPI = captureAPI
  window.windowControls = windowControls
  window.electron = electronAPI
  window.api = api
}
