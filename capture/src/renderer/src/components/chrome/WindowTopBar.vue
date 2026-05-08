<script setup>
import { onMounted, onUnmounted, ref } from 'vue'

const maximized = ref(false)
let offMaximized = null

onMounted(async () => {
  const wc = window.windowControls
  if (!wc) return
  try {
    maximized.value = await wc.isMaximized()
  } catch {
    maximized.value = false
  }
  offMaximized = wc.onMaximizedChange((v) => {
    maximized.value = v
  })
})

onUnmounted(() => {
  offMaximized?.()
})

function onMinimize() {
  void window.windowControls?.minimize()
}

function onToggleMaximize() {
  void window.windowControls?.toggleMaximize()
}

function onClose() {
  void window.windowControls?.close()
}

/** 标题区双击切换最大化（与 Win32 行为一致） */
function onTitleBarDblclick(event) {
  const el = /** @type {HTMLElement} */ (event.target)
  if (el.closest('[data-win-controls]')) return
  onToggleMaximize()
}
</script>

<template>
  <header
    class="flex h-10 shrink-0 select-none border-b border-[color:var(--capture-border)] bg-[color:var(--capture-sidebar-bg)]"
  >
    <!-- 拖动区域：Electron 使用 -webkit-app-region -->
    <div
      class="flex min-h-0 min-w-0 flex-1 cursor-default items-center gap-2 px-3 [-webkit-app-region:drag]"
      @dblclick="onTitleBarDblclick"
    >
      <span
        class="h-7 w-7 shrink-0 rounded-md border border-white/[0.06] bg-gradient-to-br from-zinc-600 via-zinc-700 to-zinc-900"
        aria-hidden="true"
      />
      <span class="truncate text-sm font-medium text-[color:var(--capture-text)]">WGC Capture</span>
    </div>

    <!-- 窗口按钮不可拖动 -->
    <div class="flex h-full shrink-0 items-stretch [-webkit-app-region:no-drag]" data-win-controls>
      <button
        type="button"
        class="flex h-full w-11 items-center justify-center text-zinc-400 transition-colors hover:bg-zinc-700 hover:text-zinc-100"
        title="最小化"
        aria-label="最小化"
        @click="onMinimize"
      >
        <svg width="10" height="1" viewBox="0 0 10 1" aria-hidden="true">
          <rect width="10" height="1" fill="currentColor" />
        </svg>
      </button>
      <button
        type="button"
        class="flex h-full w-11 items-center justify-center text-zinc-400 transition-colors hover:bg-zinc-700 hover:text-zinc-100"
        :title="maximized ? '向下还原' : '最大化'"
        :aria-label="maximized ? '向下还原' : '最大化'"
        @click="onToggleMaximize"
      >
        <!-- 最大化 / 还原 -->
        <svg
          v-if="!maximized"
          width="10"
          height="10"
          viewBox="0 0 10 10"
          fill="none"
          aria-hidden="true"
        >
          <rect x="0.5" y="0.5" width="9" height="9" stroke="currentColor" stroke-width="1" />
        </svg>
        <svg v-else width="10" height="10" viewBox="0 0 10 10" fill="none" aria-hidden="true">
          <rect x="0.5" y="2.5" width="7" height="7" stroke="currentColor" stroke-width="1" />
          <path d="M2.5 2.5V0.5H9.5V7.5H7.5" stroke="currentColor" stroke-width="1" fill="none" />
        </svg>
      </button>
      <button
        type="button"
        class="flex h-full w-11 items-center justify-center text-zinc-400 transition-colors hover:bg-red-600 hover:text-white"
        title="关闭"
        aria-label="关闭"
        @click="onClose"
      >
        <svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true">
          <path
            d="M1 1L9 9M9 1L1 9"
            stroke="currentColor"
            stroke-width="1.2"
            stroke-linecap="square"
          />
        </svg>
      </button>
    </div>
  </header>
</template>
