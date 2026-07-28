<script setup lang="ts">
/**
 * ECharts 包装组件 —— 基于 useChart composable
 * 接收 option prop，自动 init / setOption / resize / dispose
 *
 * 用法：
 *   <BaseChart :option="radarOption" height="320px" />
 */
import { ref, watch, onMounted, nextTick } from 'vue'
import type { EChartsOption } from 'echarts'
import { useChart } from '../composables/useChart'

const props = withDefaults(defineProps<{
  option: EChartsOption
  height?: string
  /** 切换 option 时是否不合并（默认合并） */
  notMerge?: boolean
}>(), {
  height: '320px',
  notMerge: false,
})

const chartEl = ref<HTMLElement>()
const { setOption, resize } = useChart(chartEl)

onMounted(async () => {
  await nextTick()
  setOption(props.option)
})

watch(
  () => props.option,
  (opt) => {
    setOption(opt, { notMerge: props.notMerge })
  },
  { deep: true }
)

// 容器尺寸变化时重绘（v-show 切换 tab 后图表宽度为 0 的修复）
watch(
  () => props.height,
  () => nextTick(resize)
)

defineExpose({ resize })
</script>

<template>
  <div ref="chartEl" class="w-full" :style="{ height }"></div>
</template>
