<script setup>
import { storeToRefs } from 'pinia'
import Button from 'primevue/button'
import Card from 'primevue/card'
import InputText from 'primevue/inputtext'
import InputNumber from 'primevue/inputnumber'
import Fieldset from 'primevue/fieldset'
import Message from 'primevue/message'
import Divider from 'primevue/divider'
import FloatLabel from 'primevue/floatlabel'
import Checkbox from 'primevue/checkbox'
import Select from 'primevue/select'
import { useCaptureRecorderStore } from '../../stores/captureRecorder.js'

const store = useCaptureRecorderStore()
const {
  captureReady,
  recording,
  errorMsg,
  outputPath,
  config,
  selectedAudioDeviceId,
  audioDeviceSelectOptions
} = storeToRefs(store)
</script>

<template>
  <aside
    class="box-border h-full w-[380px] min-w-[300px] max-w-[min(420px,100%)] overflow-x-hidden overflow-y-auto py-4 pr-4 pl-0 [&_input]:select-text [&_.p-inputtext]:select-text [&_.p-inputnumber-input]:select-text"
  >
    <Card class="settings-card">
      <template #title>录制设置</template>
      <template #subtitle
        >输出路径与 RecorderConfig；开始后由原生 Media Foundation 写入 MP4。</template
      >
      <template #content>
        <Message
          v-if="errorMsg"
          severity="error"
          class="mb-3"
          :closable="true"
          @close="store.clearError()"
        >
          {{ errorMsg }}
        </Message>

        <Fieldset legend="输出文件" class="mb-3">
          <FloatLabel variant="on" class="w-full">
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

        <Fieldset legend="录制参数" class="mb-3">
          <!-- 无步进按钮的 InputNumber，避免窄栏右侧加减块溢出 -->
          <div class="grid grid-cols-2 items-end gap-x-3 gap-y-5 [&>*]:min-w-0">
            <FloatLabel variant="on" class="field-num min-w-0">
              <InputNumber
                id="cfg-w"
                v-model="config.width"
                input-id="cfg-w"
                :min="1"
                :use-grouping="false"
                :show-buttons="false"
                :disabled="!captureReady"
                class="w-full"
              />
              <label for="cfg-w">宽度</label>
            </FloatLabel>
            <FloatLabel variant="on" class="field-num min-w-0">
              <InputNumber
                id="cfg-h"
                v-model="config.height"
                input-id="cfg-h"
                :min="1"
                :use-grouping="false"
                :show-buttons="false"
                :disabled="!captureReady"
                class="w-full"
              />
              <label for="cfg-h">高度</label>
            </FloatLabel>
            <FloatLabel variant="on" class="field-num min-w-0">
              <InputNumber
                id="cfg-fps"
                v-model="config.fps"
                input-id="cfg-fps"
                :min="1"
                :max="240"
                :use-grouping="false"
                :show-buttons="false"
                :disabled="!captureReady"
                class="w-full"
              />
              <label for="cfg-fps">帧率 FPS</label>
            </FloatLabel>
            <FloatLabel variant="on" class="field-num min-w-0">
              <InputNumber
                id="cfg-mbps"
                v-model="config.videoBitrateMbps"
                input-id="cfg-mbps"
                :min="1"
                :max="200"
                :max-fraction-digits="2"
                :use-grouping="false"
                :show-buttons="false"
                :disabled="!captureReady"
                class="w-full"
              />
              <label for="cfg-mbps">视频码率（Mbps）</label>
            </FloatLabel>
            <FloatLabel variant="on" class="field-num min-w-0">
              <InputNumber
                id="cfg-sr"
                v-model="config.audioSampleRate"
                input-id="cfg-sr"
                :min="8000"
                :max="192000"
                :step="1000"
                :use-grouping="false"
                :show-buttons="false"
                :disabled="!captureReady"
                class="w-full"
              />
              <label for="cfg-sr">音频采样率</label>
            </FloatLabel>
            <FloatLabel variant="on" class="field-num min-w-0">
              <InputNumber
                id="cfg-ch"
                v-model="config.audioChannels"
                input-id="cfg-ch"
                :min="1"
                :max="8"
                :use-grouping="false"
                :show-buttons="false"
                :disabled="!captureReady"
                class="w-full"
              />
              <label for="cfg-ch">声道数</label>
            </FloatLabel>
          </div>
          <div class="mt-2 flex items-start gap-2.5">
            <Checkbox
              v-model="config.enableAudio"
              input-id="cfg-audio"
              binary
              :disabled="!captureReady"
            />
            <label for="cfg-audio" class="cursor-pointer text-[0.9rem] leading-snug">
              封装 AAC 音频轨（关闭则仅 H.264，可绕过部分机器上 PCM/AAC 协商失败）
            </label>
          </div>
          <div class="mt-4">
            <label class="mb-1.5 block text-[0.85rem] opacity-85" for="audio-out-dev">
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
            <p class="mt-2 text-[0.78rem] leading-snug opacity-75">
              录制「该输出设备上听到的」系统音频；留空为默认设备。开始录制后采样率/声道由系统混音格式覆盖下方数字。
            </p>
          </div>
        </Fieldset>

        <div class="flex flex-col gap-2.5">
          <Button
            label="开始录制"
            icon="pi pi-video"
            severity="danger"
            :disabled="!captureReady || recording"
            @click="store.startRecording()"
          />
          <Button
            label="停止录制"
            icon="pi pi-stop"
            severity="secondary"
            outlined
            :disabled="!captureReady || !recording"
            @click="store.stopRecording()"
          />
        </div>

        <Divider />

        <p class="m-0 text-[0.8rem] leading-normal opacity-65">
          若目录不存在或 MF 初始化失败，请查看控制台与主进程日志。
        </p>
      </template>
    </Card>
  </aside>
</template>

<style scoped>
.settings-card :deep(.p-card-body) {
  padding-top: 0.5rem;
}

.field-num :deep(.p-inputnumber) {
  width: 100%;
  min-width: 0;
}

.field-num :deep(.p-inputnumber-input) {
  width: 100%;
  min-width: 0;
}
</style>
