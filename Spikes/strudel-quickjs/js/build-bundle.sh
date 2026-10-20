#!/bin/sh
# Bygger strudel.all.js = licensbanner + prelude + Strudel-bundle + glue (inget nätverk vid körning).
# Strudel: Copyright (C) Strudel contributors, AGPL-3.0-or-later, https://codeberg.org/uzu/strudel
set -e
cd "$(dirname "$0")"
npm ci --silent
npx esbuild entry.mjs --bundle --format=iife --platform=neutral --main-fields=module,main \
  --target=es2020 --legal-comments=inline --outfile=strudel.bundle.js
{
  echo '/* Contains Strudel (@strudel/core, @strudel/mini), Copyright (C) Strudel contributors, AGPL-3.0-or-later, https://codeberg.org/uzu/strudel */'
  cat prelude.js strudel.bundle.js glue.js
} > strudel.all.js
ls -l strudel.all.js
