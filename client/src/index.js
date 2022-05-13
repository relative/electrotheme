import { MESSAGE_TYPES } from './constants'
import { EventEmitter } from 'events'
import { app, session, webContents } from 'electron'
import WebSocket from 'ws'
import console from './console'
import { setStyleSheet, setupWebContents } from './styles'

const executableName = (globalThis || global).electrothemeOptions.executableName
const removeCSP = (globalThis || global).electrothemeOptions.removeCSP

const RETRY_TIME = 50

class EWS extends EventEmitter {
  url
  retries = 0
  selfClose = false
  reconnecting = false
  constructor(url) {
    super()
    this.url = url
  }
  send(type, params) {
    return this.ws.send(
      JSON.stringify({
        type: type,
        ...params,
      })
    )
  }
  close() {
    this.selfClose = true
    this.ws.close()
  }
  reconnect() {
    this.reconnecting = true
    setTimeout(() => {
      this.connect()
    }, this.retries * RETRY_TIME)
  }
  connect() {
    this.ws = new WebSocket(this.url)
    this.ws.on('open', this.onOpen)
    this.ws.on('message', this.onMessage)
    this.ws.on('error', this.onError)
    this.ws.on('close', this.onClose)
  }
  onError = (err) => {
    if (this.reconnecting) {
      this.retries++
    }
    console.error('Error in WebSocket', err)
  }
  onOpen = () => {
    this.retries = 0
    this.reconnecting = false
    this.emit('open')
  }
  onMessage = (message) => {
    let str = message.toString('utf8')
    try {
      let payload = JSON.parse(str)
      this.emit('message', payload)
    } catch (ex) {
      console.error('Failed to parse', str, 'ex: \n', ex)
    }
  }

  onClose = () => {
    this.emit('close')
    if (this.selfClose) return
    this.reconnect()
  }
}

const ws = new EWS('ws://127.0.0.1:64132/client')
ws.on('open', () => {
  ws.send(MESSAGE_TYPES.Hello, {
    exe: executableName,
  })
})
ws.on('message', (msg) => {
  switch (msg.type) {
    case MESSAGE_TYPES.StylesUpdate:
      setStyleSheet(msg.css)
      break
    default:
      break
  }
})
ws.connect()

app.on('quit', () => {
  ws.close()
})

app.on('web-contents-created', (e, wc) => {
  setupWebContents(wc)
})

// Does not apply on config reload
if (removeCSP) {
  session.defaultSession.webRequest.onHeadersReceived((details, cb) => {
    const CSP_KEY = 'content-security-policy'
    if (CSP_KEY in details.responseHeaders)
      delete details.responseHeaders[CSP_KEY]
    return cb({
      responseHeaders: details.responseHeaders,
    })
  })

  // Reloads the CSP in all pages
  webContents.getAllWebContents().forEach((wc) => wc.reload())
}

webContents.getAllWebContents().forEach((wc) => setupWebContents(wc))
// Kills the inspector thread in the process, freeing :9229 for the next process
// and closing the WebSocket connection to :9229
process._debugEnd()
