import './assets/main.css'
import 'primeicons/primeicons.css'
import PrimeVue from 'primevue/config'
import { createPinia } from 'pinia'
import { createApp } from 'vue'
import App from './App.vue'
import { CaptureGrayscalePreset } from './themes/captureGrayscalePreset.js'

document.documentElement.classList.add('capture-dark')

const app = createApp(App)
app.use(createPinia())
app.use(PrimeVue, {
  theme: {
    preset: CaptureGrayscalePreset,
    options: {
      darkModeSelector: '.capture-dark',
      cssLayer: false
    }
  }
})
app.mount('#app')
