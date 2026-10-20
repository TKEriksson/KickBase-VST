// Körs efter bundlen i QuickJS. Stubbar det webbläsaren normalt ger.
var S = globalThis.Strudel;
S.setStringParser(S.mini);
Object.assign(globalThis, S); // s, note, stack, ... som globala namn, som på strudel.cc
var current = null;
// Koden får vara flera satser: `const x = ...`, och mönster som `$: uttryck` (eller `namn: uttryck`).
// Ett block börjar på en rad med etikett och slutar vid tom rad eller nästa etikett; alla block läggs i en stack.
// Utan etiketter är sista blocket (efter sista tomma raden) mönstret. Svarar "" när det gick bra, annars felmeddelandet.
var labelRe = /^(\$|[A-Za-z_]\w*)\s*:(?!:)\s*/;
function splitCode(code) {
  var lines = code.split('\n'), pre = [], blocks = [], cur = null;
  for (var i = 0; i < lines.length; i++) {
    var line = lines[i], m = labelRe.exec(line);
    if (m) { cur = [line.slice(m[0].length)]; blocks.push(cur); }
    else if (/^\s*$/.test(line)) { cur = null; pre.push(line); }
    else if (cur) cur.push(line);
    else pre.push(line);
  }
  return { pre: pre.join('\n'), blocks: blocks.map(function (b) { return b.join('\n').replace(/[;\s]+$/, ''); }) };
}
globalThis.setPattern = function (code) {
  try {
    var sp = splitCode(code), body;
    if (sp.blocks.length) {
      body = sp.pre + '\nreturn stack(' + sp.blocks.map(function (b) { return '(' + b + ')'; }).join(',') + ');';
    } else {
      var parts = code.replace(/\s+$/, '').split(/\n\s*\n/);
      var last = parts.pop().replace(/;\s*$/, '');
      body = parts.join('\n\n') + '\nreturn (' + last + ');';
    }
    current = (new Function(body))();
    if (!current || typeof current.queryArc !== 'function') return 'Koden ger inget mönster (skriv t.ex. $: s("bd*4"))';
    return '';
  } catch (e) {
    current = null;
    return String(e && e.message ? e.message : e);
  }
};
globalThis.queryJson = function (from, to) {
  var out = [];
  var haps = current.queryArc(from, to);
  for (var i = 0; i < haps.length; i++) {
    var h = haps[i];
    out.push({ b: h.whole ? h.whole.begin.valueOf() : null, e: h.whole ? h.whole.end.valueOf() : null,
               hasOnset: h.hasOnset(), v: h.value });
  }
  return JSON.stringify(out);
};

// slider(v, min, max): C++ registrerar objektet "host" (DynamicObject) med metoden slider(id, v, min, max).
// Utan Strudels transpiler får vi inget källpositions-id, så vi numrerar anropen i ordning per setPattern.
var sliderCounter = 0;
globalThis.slider = function (v, min, max) {
  var id = sliderCounter++;
  var val = host.slider(id, v, min, max); // C++ svarar med aktuellt värde (användaren kan ha dragit i reglaget)
  return S.pure(val); // konstant värde med onset (en kontinuerlig signal ger inga händelser i note()); ett nytt värde kräver ny setPattern (eller en signal som läser host.get)
};
globalThis.sliderLive = function (id, min, max) { // läser C++-värdet vid varje fråga: reglage utan omkompilering
  return S.signal(function () { return host.get(id); }).range(min, max);
};
var origSet = globalThis.setPattern;
globalThis.setPattern = function (code) { sliderCounter = 0; return origSet(code); };
