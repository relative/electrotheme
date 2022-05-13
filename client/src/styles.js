import { webContents } from 'electron'

let currentStyleSheet = ''

const fnRenderer = () => {
  let style = document.querySelector('style#electrotheme')
  if (!style) {
    style = document.createElement('style')
    style.setAttribute('id', 'electrotheme')
    document.head.appendChild(style)
  }
  style.innerHTML = atob(`{css}`)
}
function injectStyle(wc, style) {
  if (!wc) return
  let code = fnRenderer.toString() // "() => {/*...*/}"
  code = code.replace(
    /\{css}/gi,
    Buffer.from(style, 'utf8').toString('base64') // lol
  )
  wc.executeJavaScript(';(' + code + ')();', true)
}

export function setStyleSheet(style) {
  currentStyleSheet = style
  updateAllWebContents()
}
export function getStyleSheet() {
  return currentStyleSheet
}
export function updateAllWebContents() {
  const wcs = webContents.getAllWebContents()
  if (!Array.isArray(wcs)) return
  wcs.forEach((wc) => {
    injectStyle(wc, currentStyleSheet)
  })
}
export function setupWebContents(wc) {
  if (!wc) return
  wc.on('did-finish-load', () => {
    injectStyle(wc, getStyleSheet())
  })
  if (!wc.isLoading()) injectStyle(wc, getStyleSheet())
}
