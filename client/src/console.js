export default {
  ...console,
  log: console.log.bind(console, '[electrotheme]'),
  info: console.info.bind(console, '[electrotheme]'),
  warn: console.warn.bind(console, '[electrotheme]'),
  error: console.error.bind(console, '[electrotheme]'),
}
