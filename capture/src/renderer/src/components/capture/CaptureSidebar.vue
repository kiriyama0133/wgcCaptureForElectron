<script setup>
import { storeToRefs } from 'pinia'
import { useCaptureRecorderStore } from '../../stores/captureRecorder.js'

defineEmits(['open-about'])

const store = useCaptureRecorderStore()
const { recording } = storeToRefs(store)

const primaryItem = { label: '屏幕录制', icon: 'pi pi-desktop' }
const aboutItem = { label: '关于', icon: 'pi pi-info-circle' }
</script>

<template>
  <aside
    class="flex h-full min-w-[200px] max-w-[220px] flex-col border-r border-[color:var(--capture-border)] bg-[color:var(--capture-sidebar-bg)] px-3 py-4"
  >
    <div class="mb-1 flex items-center gap-2.5 px-2 pb-5 pt-1">
      <span
        class="h-9 w-9 shrink-0 rounded-lg border border-white/[0.06] bg-gradient-to-br from-zinc-600 via-zinc-700 to-zinc-900"
        aria-hidden="true"
      />
      <div class="flex min-w-0 flex-col gap-0.5">
        <span class="text-[0.95rem] font-semibold tracking-wide text-[color:var(--capture-text)]">
          WGC Capture
        </span>
        <span class="text-xs opacity-55">屏幕捕获</span>
      </div>
    </div>

    <!-- 固定 36px 图标列，所有图标左缘对齐 -->
    <nav class="flex flex-1 flex-col gap-1" aria-label="应用导航">
      <button
        type="button"
        class="flex w-full items-center gap-0 rounded-lg bg-primary/12 py-2 pl-2 pr-2 text-left text-sm text-zinc-100"
      >
        <span
          class="inline-flex h-9 w-9 shrink-0 items-center justify-center text-zinc-300 [&_.pi]:text-[1rem]"
          aria-hidden="true"
        >
          <i :class="primaryItem.icon" />
        </span>
        <span class="min-w-0 truncate pl-0.5">{{ primaryItem.label }}</span>
      </button>

      <button
        type="button"
        class="flex w-full items-center gap-0 rounded-lg py-2 pl-2 pr-2 text-left text-sm text-zinc-100 transition-colors hover:bg-zinc-800/60"
        @click="$emit('open-about')"
      >
        <span
          class="inline-flex h-9 w-9 shrink-0 items-center justify-center text-zinc-300 [&_.pi]:text-[1rem]"
          aria-hidden="true"
        >
          <i :class="aboutItem.icon" />
        </span>
        <span class="min-w-0 truncate pl-0.5">{{ aboutItem.label }}</span>
      </button>
    </nav>

    <div class="mt-auto pt-4">
      <div
        class="flex items-center gap-2 rounded-lg border border-[color:var(--capture-border)] bg-[color:var(--capture-elevated)] px-2.5 py-2 text-[0.8rem] text-[color:var(--capture-muted)] data-[on=true]:border-red-400/35 data-[on=true]:text-[color:var(--capture-text)]"
        :data-on="recording"
      >
        <span
          class="h-2 w-2 shrink-0 rounded-full bg-zinc-500 data-[on=true]:bg-red-500 data-[on=true]:shadow-[0_0_0_3px_rgba(239,68,68,0.2)]"
          :data-on="recording"
          aria-hidden="true"
        />
        {{ recording ? '录制中' : '就绪' }}
      </div>
    </div>
  </aside>
</template>
