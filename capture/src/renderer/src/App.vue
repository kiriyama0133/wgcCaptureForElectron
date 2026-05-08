<script setup>
import { onMounted, onUnmounted, reactive, ref } from 'vue'
import Dialog from 'primevue/dialog'
import WindowTopBar from './components/chrome/WindowTopBar.vue'
import CaptureSidebar from './components/capture/CaptureSidebar.vue'
import CapturePreviewPanel from './components/capture/CapturePreviewPanel.vue'
import CaptureSettingsPanel from './components/capture/CaptureSettingsPanel.vue'
import { useCaptureRecorderStore } from './stores/captureRecorder.js'

const store = useCaptureRecorderStore()
const previewPanelRef = ref(null)
const aboutOpen = ref(false)

const versions = reactive({ ...window.electron.process.versions })

onMounted(() => {
  void store.bindPreview(previewPanelRef)
})

onUnmounted(() => {
  void store.teardown()
})
</script>

<template>
  <div class="box-border flex h-screen min-h-0 w-full flex-col bg-[var(--capture-app-bg,#0c0c0e)]">
    <WindowTopBar />

    <div class="flex min-h-0 flex-1 flex-row items-stretch">
      <CaptureSidebar @open-about="aboutOpen = true" />

      <CapturePreviewPanel ref="previewPanelRef" />

      <CaptureSettingsPanel />
    </div>

    <Dialog
      v-model:visible="aboutOpen"
      modal
      header="关于"
      class="about-dialog"
      :style="{ width: 'min(400px, 92vw)' }"
    >
      <ul class="m-0 list-none p-0 font-mono text-sm leading-relaxed opacity-90">
        <li class="py-0.5">Electron v{{ versions.electron }}</li>
        <li class="py-0.5">Chromium v{{ versions.chrome }}</li>
        <li class="py-0.5">Node v{{ versions.node }}</li>
      </ul>
    </Dialog>
  </div>
</template>
