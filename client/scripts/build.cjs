const esbuild = require('esbuild'),
  { join } = require('path')

const ROOT_PATH = join(__dirname, '..'),
  DIST_PATH = join(ROOT_PATH, 'dist'),
  SRC_PATH = join(ROOT_PATH, 'src')

const args = process.argv.slice(2)

// should be 'production' | 'development'
const env = args.some((a) => a.match(/^(?:--production)$/gi) !== null)
  ? 'production'
  : 'development'

// --watch or -w
const watch =
  args.length > 0 && args.some((a) => a.match(/^(?:--watch|-w)$/gi) !== null)
if (watch) process.stdout.write('[watch] ')

const production = env === 'production'

esbuild.build({
  entryPoints: [join(SRC_PATH, 'index.js')],

  watch: watch,

  bundle: true,
  outdir: DIST_PATH,
  sourcemap: !production,
  minify: production,

  platform: 'node',
  format: 'cjs',

  legalComments: 'inline',

  external: ['electron', 'events'],

  logLevel: 'info',
  logLimit: process.env.CI ? 0 : 30,
})
