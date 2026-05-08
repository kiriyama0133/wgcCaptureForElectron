import { defineStore } from 'pinia'
import { nextTick, unref } from 'vue'

/** 预览帧循环（非持久化状态） */
let previewTimerId = null
let previewImageData = null

export const useCaptureRecorderStore = defineStore('captureRecorder', {
  state: () => ({
    captureReady: false,
    recording: false,
    errorMsg: '',
    outputPath: 'C:\\Users\\Public\\Videos\\wgc-capture.mp4',
    config: {
      width: 1920,
      height: 1080,
      fps: 30,
      videoBitrateMbps: 10,
      audioSampleRate: 48000,
      audioChannels: 2,
      enableAudio: true
    },
    audioOutputDevices: [],
    selectedAudioDeviceId: null
  }),

  getters: {
    audioDeviceSelectOptions: (state) => [
      { id: '', name: '（系统默认播放设备）' },
      ...state.audioOutputDevices
    ]
  },

  actions: {
    clearError() {
      this.errorMsg = ''
    },

    refreshRecordingState() {
      try {
        this.recording = window.captureAPI.isRecording()
      } catch {
        this.recording = false
      }
    },

    async loadAudioOutputDevices() {
      try {
        const list = await window.captureAPI.getAudioOutputDevices()
        this.audioOutputDevices = Array.isArray(list) ? list : []
      } catch (e) {
        console.error('getAudioOutputDevices', e)
      }
    },

    async startRecording() {
      this.errorMsg = ''
      const p = this.outputPath.trim()
      if (!p) {
        this.errorMsg = '请填写输出 MP4 完整路径（UTF-8）'
        return
      }
      const c = this.config
      try {
        const rec = {
          width: c.width,
          height: c.height,
          fps: c.fps,
          videoBitrate: Math.round(c.videoBitrateMbps * 1_000_000),
          audioSampleRate: c.audioSampleRate,
          audioChannels: c.audioChannels,
          enableAudio: c.enableAudio
        }
        if (c.enableAudio && this.selectedAudioDeviceId)
          rec.audioOutputDeviceId = this.selectedAudioDeviceId

        await window.captureAPI.startRecording(p, rec)
        this.refreshRecordingState()
        if (!this.recording) {
          this.errorMsg = 'startRecording 已返回但未处于录制状态，请查看主进程日志'
        }
      } catch (e) {
        this.errorMsg = e?.message ?? String(e)
        this.refreshRecordingState()
      }
    },

    stopRecording() {
      this.errorMsg = ''
      try {
        window.captureAPI.stopRecording()
        this.refreshRecordingState()
      } catch (e) {
        this.errorMsg = e?.message ?? String(e)
      }
    },

    /**
     * 关联预览画布并启动捕获与帧轮询（在根组件 onMounted 中调用）
     * @param {import('vue').Ref} previewPanelRef CapturePreviewPanel 组件 ref
     */
    async bindPreview(previewPanelRef) {
      await nextTick()
      const canvas = unref(previewPanelRef.value?.canvasEl)
      const ctx = canvas?.getContext('2d')
      if (!canvas || !ctx) {
        this.errorMsg = '预览画布未就绪'
        return
      }

      try {
        await window.captureAPI.start()
      } catch (e) {
        console.error('capture start failed', e)
        this.errorMsg = `捕获启动失败：${e?.message ?? e}`
        return
      }

      const size = await window.captureAPI.getSize()
      if (!size || size.width === 0) {
        this.errorMsg = '无法获取显示器尺寸'
        return
      }

      this.captureReady = true
      this.config.width = size.width
      this.config.height = size.height

      await this.loadAudioOutputDevices()

      canvas.width = size.width
      canvas.height = size.height
      previewImageData = ctx.createImageData(size.width, size.height)

      const tick = async () => {
        this.refreshRecordingState()
        try {
          const buffer = await window.captureAPI.getFrame()
          if (buffer && previewImageData) {
            previewImageData.data.set(new Uint8Array(buffer))
            ctx.putImageData(previewImageData, 0, 0)
          }
        } catch (e) {
          console.error('getFrame', e)
        }
      }
      previewTimerId = window.setInterval(tick, 66)
    },

    async teardown() {
      if (previewTimerId != null) {
        window.clearInterval(previewTimerId)
        previewTimerId = null
      }
      previewImageData = null
      try {
        window.captureAPI.stopRecording()
      } catch (e) {
        console.error('stopRecording', e)
      }
      try {
        await window.captureAPI.stop()
      } catch (e) {
        console.error('stop', e)
      }
    }
  }
})
