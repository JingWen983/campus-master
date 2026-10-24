/**
 * 积分卡导出工具 —— 复用已安装的 xlsx
 */
import * as XLSX from 'xlsx'

export interface CardExportRow {
  code: string
  face_value: number
  status: string
  status_text?: string
  batch_no: string
  bound_student_name?: string
  issuer_name?: string
  created_at: string
  remark?: string
}

/** 导出积分卡列表为 xlsx（列：卡号/面额/状态/批次/绑定学生/签发人/生成时间/备注） */
export function exportCardsXlsx(rows: CardExportRow[], filename: string): void {
  const header = ['卡号', '面额(积分)', '状态', '批次号', '绑定学生', '签发人', '生成时间', '备注']
  const aoa: (string | number)[][] = [header]
  rows.forEach(r => {
    aoa.push([
      r.code,
      r.face_value,
      r.status_text || r.status,
      r.batch_no,
      r.bound_student_name || '',
      r.issuer_name || '',
      r.created_at,
      r.remark || '',
    ])
  })
  const ws = XLSX.utils.aoa_to_sheet(aoa)
  // 列宽
  ;(ws as XLSX.WorkSheet)['!cols'] = [
    { wch: 24 }, { wch: 10 }, { wch: 8 }, { wch: 20 },
    { wch: 10 }, { wch: 10 }, { wch: 18 }, { wch: 16 },
  ]
  const wb = XLSX.utils.book_new()
  XLSX.utils.book_append_sheet(wb, ws, '积分卡')
  XLSX.writeFile(wb, filename)
}
