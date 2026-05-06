<script setup>
import { onMounted, onUnmounted, ref } from 'vue'

const screenCanvas = ref(null)
let timerId = null

onMounted(async () => {
  const canvas = screenCanvas.value
  const ctx = canvas.getContext('2d')

  try {
    await window.captureAPI.start()
  } catch (e) {
    console.error('capture start failed', e)
    return
  }

  const size = await window.captureAPI.getSize()
  if (!size || size.width === 0) {
    console.error('无法获取显示器尺寸')
    return
  }
  canvas.width = size.width
  canvas.height = size.height
  const imgData = ctx.createImageData(size.width, size.height)

  // 原生已输出 RGBA；preload 调 getFrame。先约 15fps，后续可降采样 / SharedArrayBuffer
  const tick = async () => {
    try {
      const buffer = await window.captureAPI.getFrame()
      if (buffer) {
        imgData.data.set(new Uint8Array(buffer))
        ctx.putImageData(imgData, 0, 0)
      }
    } catch (e) {
      console.error('getFrame', e)
    }
  }
  timerId = window.setInterval(tick, 66)
})

onUnmounted(async () => {
  if (timerId != null) window.clearInterval(timerId)
  try {
    await window.captureAPI.stop()
  } catch (e) {
    console.error('stop', e)
  }
})
</script>

<template>
  <div class="capture-container">
    <canvas ref="screenCanvas" class="screen-render"></canvas>
  </div>
</template>

<style scoped>
.capture-container {
  width: 100%;
  height: 100vh;
  display: flex;
  justify-content: center;
  align-items: center;
  background: #000;
}
.screen-render {
  max-width: 100%;
  max-height: 100%;
  object-fit: contain;
}
</style>
