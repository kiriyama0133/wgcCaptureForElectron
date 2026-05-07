<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import Button from 'primevue/button'
import Card from 'primevue/card'
import InputText from 'primevue/inputtext'
import InputNumber from 'primevue/inputnumber'
import Fieldset from 'primevue/fieldset'
import Tag from 'primevue/tag'
import Message from 'primevue/message'
import Divider from 'primevue/divider'
import FloatLabel from 'primevue/floatlabel'
import Checkbox from 'primevue/checkbox'
import Select from 'primevue/select'

const screenCanvas = ref(null)
let timerId = null

const captureReady = ref(false)
const recording = ref(false)
const errorMsg = ref('')

/** UTF-8 路径，Windows 示例：C:\\Users\\Public\\Videos\\test.mp4 */
const outputPath = ref('C:\\Users\\Public\\Videos\\wgc-capture.mp4')

/** 与原生 RecorderConfig 对应；码率 UI 用 Mbps，提交时换算为 bps */
const config = ref({
  width: 1920,
  height: 1080,
  fps: 30,
  videoBitrateMbps: 10,
  audioSampleRate: 48000,
  audioChannels: 2,
  enableAudio: true
})

/** WASAPI 枚举的播放设备；model 为设备 id，null 表示默认播放设备 */
const audioOutputDevices = ref([])
const selectedAudioDeviceId = ref(null)

const audioDeviceSelectOptions = computed(() => [
  { id: '', name: '（系统默认播放设备）' },
  ...audioOutputDevices.value
])

async function loadAudioOutputDevices() {
  try {
    const list = await window.captureAPI.getAudioOutputDevices()
    audioOutputDevices.value = Array.isArray(list) ? list : []
  } catch (e) {
    console.error('getAudioOutputDevices', e)
  }
}

function refreshRecordingState() {
  try {
    recording.value = window.captureAPI.isRecording()
  } catch {
    recording.value = false
  }
}

async function handleStartRecording() {
  errorMsg.value = ''
  const p = outputPath.value.trim()
  if (!p) {
    errorMsg.value = '请填写输出 MP4 完整路径（UTF-8）'
    return
  }
  const c = config.value
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
    if (c.enableAudio && selectedAudioDeviceId.value)
      rec.audioOutputDeviceId = selectedAudioDeviceId.value

    await window.captureAPI.startRecording(p, rec)
    refreshRecordingState()
    if (!recording.value) {
      errorMsg.value = 'startRecording 已返回但未处于录制状态，请查看主进程日志'
    }
  } catch (e) {
    errorMsg.value = e?.message ?? String(e)
    refreshRecordingState()
  }
}

function handleStopRecording() {
  errorMsg.value = ''
  try {
    window.captureAPI.stopRecording()
    refreshRecordingState()
  } catch (e) {
    errorMsg.value = e?.message ?? String(e)
  }
}

onMounted(async () => {
  const canvas = screenCanvas.value
  const ctx = canvas.getContext('2d')

  try {
    await window.captureAPI.start()
  } catch (e) {
    console.error('capture start failed', e)
    errorMsg.value = `捕获启动失败：${e?.message ?? e}`
    return
  }

  const size = await window.captureAPI.getSize()
  if (!size || size.width === 0) {
    errorMsg.value = '无法获取显示器尺寸'
    return
  }

  captureReady.value = true
  config.value.width = size.width
  config.value.height = size.height

  await loadAudioOutputDevices()

  canvas.width = size.width
  canvas.height = size.height
  const imgData = ctx.createImageData(size.width, size.height)

  const tick = async () => {
    refreshRecordingState()
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
    window.captureAPI.stopRecording()
  } catch (e) {
    console.error('stopRecording', e)
  }
  try {
    await window.captureAPI.stop()
  } catch (e) {
    console.error('stop', e)
  }
})
</script>

<template>
  <div class="app-root allow-select">
    <Card class="toolbar-card">
      <template #title>
        <div class="title-row">
          <span>WGC 屏幕捕获 · 录制测验</span>
          <Tag v-if="recording" severity="danger" value="录制中" rounded />
          <Tag v-else severity="secondary" value="未录制" rounded />
        </div>
      </template>
      <template #subtitle>
        先启动预览，再填写路径与参数后开始写入 MP4（原生 Media Foundation）。
      </template>
      <template #content>
        <Message
          v-if="errorMsg"
          severity="error"
          class="mb-3"
          :closable="true"
          @close="errorMsg = ''"
        >
          {{ errorMsg }}
        </Message>

        <Fieldset legend="输出文件" class="mb-3">
          <FloatLabel variant="on" class="path-field">
            <InputText
              id="out-path"
              v-model="outputPath"
              class="w-full"
              :disabled="!captureReady"
              autocomplete="off"
              spellcheck="false"
            />
            <label for="out-path">MP4 路径（UTF-8）</label>
          </FloatLabel>
        </Fieldset>

        <Fieldset legend="录制参数（RecorderConfig）" class="mb-3">
          <div class="config-grid">
            <FloatLabel variant="on">
              <InputNumber
                id="cfg-w"
                v-model="config.width"
                input-id="cfg-w"
                :min="1"
                :disabled="!captureReady"
                show-buttons
              />
              <label for="cfg-w">宽度</label>
            </FloatLabel>
            <FloatLabel variant="on">
              <InputNumber
                id="cfg-h"
                v-model="config.height"
                input-id="cfg-h"
                :min="1"
                :disabled="!captureReady"
                show-buttons
              />
              <label for="cfg-h">高度</label>
            </FloatLabel>
            <FloatLabel variant="on">
              <InputNumber
                id="cfg-fps"
                v-model="config.fps"
                input-id="cfg-fps"
                :min="1"
                :max="240"
                :disabled="!captureReady"
                show-buttons
              />
              <label for="cfg-fps">帧率 FPS</label>
            </FloatLabel>
            <FloatLabel variant="on">
              <InputNumber
                id="cfg-mbps"
                v-model="config.videoBitrateMbps"
                input-id="cfg-mbps"
                :min="1"
                :max="200"
                :max-fraction-digits="2"
                :disabled="!captureReady"
                show-buttons
              />
              <label for="cfg-mbps">视频码率（Mbps）</label>
            </FloatLabel>
            <FloatLabel variant="on">
              <InputNumber
                id="cfg-sr"
                v-model="config.audioSampleRate"
                input-id="cfg-sr"
                :min="8000"
                :max="192000"
                :step="1000"
                :disabled="!captureReady"
                show-buttons
              />
              <label for="cfg-sr">音频采样率</label>
            </FloatLabel>
            <FloatLabel variant="on">
              <InputNumber
                id="cfg-ch"
                v-model="config.audioChannels"
                input-id="cfg-ch"
                :min="1"
                :max="8"
                :disabled="!captureReady"
                show-buttons
              />
              <label for="cfg-ch">声道数</label>
            </FloatLabel>
          </div>
          <div class="audio-row">
            <Checkbox
              v-model="config.enableAudio"
              input-id="cfg-audio"
              binary
              :disabled="!captureReady"
            />
            <label for="cfg-audio" class="audio-row-label">
              封装 AAC 音频轨（关闭则仅 H.264，可绕过部分机器上 PCM/AAC 协商失败）
            </label>
          </div>
          <div class="audio-device-field">
            <label class="audio-device-label" for="audio-out-dev">
              loopback 音频来源（播放设备）
            </label>
            <Select
              id="audio-out-dev"
              v-model="selectedAudioDeviceId"
              :options="audioDeviceSelectOptions"
              option-label="name"
              option-value="id"
              placeholder="选择播放设备"
              class="w-full"
              show-clear
              :disabled="!captureReady || !config.enableAudio"
            />
            <p class="device-hint">
              录制「该输出设备上听到的」系统音频；留空为默认设备。开始录制后采样率/声道由系统混音格式覆盖下方数字。
            </p>
          </div>
        </Fieldset>

        <div class="actions-row">
          <Button
            label="开始录制"
            icon="pi pi-video"
            severity="danger"
            :disabled="!captureReady || recording"
            @click="handleStartRecording"
          />
          <Button
            label="停止录制"
            icon="pi pi-stop"
            severity="secondary"
            outlined
            :disabled="!captureReady || !recording"
            @click="handleStopRecording"
          />
        </div>
        <Divider />
        <p class="hint">
          预览区域下方；参数默认与当前捕获分辨率同步。 若目录不存在或 MF
          初始化失败，请查看控制台与主进程日志。
        </p>
      </template>
    </Card>

    <Card class="preview-card">
      <template #title>实时预览</template>
      <template #content>
        <div class="canvas-wrap">
          <canvas ref="screenCanvas" class="screen-render"></canvas>
        </div>
      </template>
    </Card>
  </div>
</template>

<style scoped>
.app-root {
  width: min(960px, 100vw);
  max-height: 100vh;
  overflow: auto;
  padding: 12px;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.allow-select :deep(input),
.allow-select :deep(.p-inputtext),
.allow-select :deep(.p-inputnumber-input) {
  user-select: text;
}

.title-row {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}

.path-field {
  width: 100%;
}

.config-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
  gap: 1.25rem 1rem;
  align-items: end;
}

.audio-row {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  margin-top: 0.5rem;
}

.audio-row-label {
  font-size: 0.95rem;
  line-height: 1.45;
  cursor: pointer;
}

.audio-device-field {
  margin-top: 1rem;
}

.audio-device-label {
  display: block;
  font-size: 0.9rem;
  margin-bottom: 0.35rem;
}

.device-hint {
  margin: 0.5rem 0 0;
  font-size: 0.82rem;
  opacity: 0.85;
  line-height: 1.45;
}

.actions-row {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
}

.hint {
  margin: 0;
  font-size: 0.85rem;
  opacity: 0.85;
  line-height: 1.5;
}

.preview-card :deep(.p-card-body) {
  padding-top: 0;
}

.canvas-wrap {
  display: flex;
  justify-content: center;
  align-items: center;
  background: #0a0a0f;
  border-radius: 8px;
  overflow: hidden;
  min-height: 200px;
}

.screen-render {
  max-width: 100%;
  max-height: min(52vh, 520px);
  width: auto;
  height: auto;
  object-fit: contain;
  display: block;
}

.mb-3 {
  margin-bottom: 1rem;
}
</style>
