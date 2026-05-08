import { definePreset } from '@primeuix/themes'
import Aura from '@primeuix/themes/aura'

/**
 * 黑白灰风格：主色与强调均映射到 zinc 色阶，与深色 surface 搭配。
 */
export const CaptureGrayscalePreset = definePreset(Aura, {
  semantic: {
    primary: {
      50: '{zinc.50}',
      100: '{zinc.100}',
      200: '{zinc.200}',
      300: '{zinc.300}',
      400: '{zinc.400}',
      500: '{zinc.500}',
      600: '{zinc.600}',
      700: '{zinc.700}',
      800: '{zinc.800}',
      900: '{zinc.900}',
      950: '{zinc.950}'
    },
    colorScheme: {
      dark: {
        primary: {
          color: '{primary.300}',
          contrastColor: '{zinc.950}',
          hoverColor: '{primary.200}',
          activeColor: '{primary.100}'
        }
      }
    }
  }
})
