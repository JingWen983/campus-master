/**
 * ECharts 生命周期 composable —— 封装 init / setOption / resize / dispose
 *
 * 用法：
 *   const chartEl = ref<HTMLElement>()
 *   const { setOption, resize, getInstance } = useChart(chartEl)
 *   onMounted(() => setOption({...}))
 */
import { onMounted, onBeforeUnmount, onActivated, onDeactivated, type Ref } from 'vue'
import * as echarts from 'echarts'

export function useChart(el: Ref<HTMLElement | undefined>, theme?: string) {
  let chart: echarts.ECharts | null = null

  function ensureChart(): echarts.ECharts | null {
    if (!el.value) return null
    if (!chart) {
      chart = echarts.init(el.value, theme)
    }
    return chart
  }

  function setOption(option: echarts.EChartsOption, opts?: { notMerge?: boolean; lazyUpdate?: boolean }) {
    const c = ensureChart()
    if (c) c.setOption(option, opts)
  }

  function resize() {
    chart?.resize()
  }

  function getInstance(): echarts.ECharts | null {
    return chart
  }

  function dispose() {
    if (chart) {
      chart.dispose()
      chart = null
    }
  }

  // 全局 resize 监听
  const onWindowResize = () => resize()

  onMounted(() => {
    window.addEventListener('resize', onWindowResize)
  })

  onActivated(() => {
    window.addEventListener('resize', onWindowResize)
    resize()
  })

  onDeactivated(() => {
    window.removeEventListener('resize', onWindowResize)
  })

  onBeforeUnmount(() => {
    window.removeEventListener('resize', onWindowResize)
    dispose()
  })

  return { setOption, resize, getInstance, dispose }
}
