/**
 * 二维码工具 —— 基于已安装的 qrcode（生成）/ jsqr（识别）
 *
 * - makeQrDataUrl: 学生端展示兑换二维码（Data URL）
 * - scanVideoFrame: 后勤端摄像头扫帧识别二维码内容
 * - normalizeCode: 与后端 normalize_redemption_code 同规则的验证码归一化
 */
import QRCode from 'qrcode'
import jsQR from 'jsqr'

/** 生成二维码 DataURL（学生端展示验证码 XXXX-XXXX） */
export async function makeQrDataUrl(text: string, size = 220): Promise<string> {
  return QRCode.toDataURL(text, {
    width: size,
    margin: 2,
    errorCorrectionLevel: 'M',
    color: { dark: '#1c1917', light: '#ffffff' },
  })
}

/** 从 video 帧识别二维码（后勤端摄像头扫码），未识别返回 null */
export function scanVideoFrame(
  video: HTMLVideoElement,
  canvas: HTMLCanvasElement
): string | null {
  if (video.readyState !== video.HAVE_ENOUGH_DATA) return null
  canvas.width = video.videoWidth
  canvas.height = video.videoHeight
  const ctx = canvas.getContext('2d', { willReadFrequently: true })
  if (!ctx) return null
  ctx.drawImage(video, 0, 0)
  const img = ctx.getImageData(0, 0, canvas.width, canvas.height)
  const result = jsQR(img.data, img.width, img.height, { inversionAttempts: 'dontInvert' })
  return result?.data ?? null
}

/** 归一化验证码输入：去空格/横线 → 大写 → 0→O/1→I → 重新插横线 */
export function normalizeCode(raw: string): string {
  const s = raw
    .replace(/[\s-]/g, '')
    .toUpperCase()
    .replace(/0/g, 'O')
    .replace(/1/g, 'I')
  if (s.length !== 8) return ''
  return s.slice(0, 4) + '-' + s.slice(4)
}
