<script setup>
import { ref } from 'vue'

const canvasEl = ref(null)

defineExpose({
  canvasEl
})
</script>

<template>
  <section class="flex min-h-0 min-w-0 flex-1 flex-col py-4 pr-4 pl-0" aria-label="实时预览">
    <header class="mb-3">
      <h2 class="mb-1 text-base font-semibold text-[color:var(--capture-text)]">实时预览</h2>
      <p class="m-0 text-[0.82rem] leading-snug opacity-55">
        与捕获分辨率一致；录制时画面持续更新。
      </p>
    </header>
    <div
      class="flex min-h-0 flex-1 items-center justify-center overflow-hidden rounded-[10px] border border-[color:var(--capture-border)] bg-[#0a0a0c]"
    >
      <canvas ref="canvasEl" class="block h-auto max-h-full w-auto max-w-full object-contain" />
    </div>
  </section>
</template>
