/** 与原生 RecorderConfig / media_recorder.h 对齐 */
export interface RecorderConfig {
  width: number
  height: number
  fps: number
  /** 默认 10_000_000（约 10Mbps） */
  videoBitrate?: number
  audioSampleRate?: number
  audioChannels?: number
  /** 默认 true；false 时仅写入 H.264 视频轨 */
  enableAudio?: boolean
  /** WASAPI 播放设备 ID（getAudioOutputDevices 返回的 id）；不传则用默认播放设备 */
  audioOutputDeviceId?: string
}

export interface AudioOutputDeviceInfo {
  id: string
  name: string
}

export interface CaptureSize {
  width: number
  height: number
}

declare global {
  interface Window {
    windowControls?: {
      minimize(): Promise<void>
      toggleMaximize(): Promise<boolean>
      close(): Promise<void>
      isMaximized(): Promise<boolean>
      onMaximizedChange(callback: (maximized: boolean) => void): () => void
    }
    captureAPI: {
      start(): Promise<void>
      stop(): void
      getSize(): CaptureSize
      getFrame(): Promise<ArrayBuffer | Uint8Array | null>
      /** outputPath 为 UTF-8，Windows 下可用 drive:\\path\\file.mp4 */
      startRecording(outputPath: string, config: RecorderConfig): Promise<void>
      stopRecording(): void
      isRecording(): boolean
      getAudioOutputDevices(): Promise<AudioOutputDeviceInfo[]>
    }
  }
}

export {}
