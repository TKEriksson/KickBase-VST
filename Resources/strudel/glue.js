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

// Parameterträd (F1): instr1.Cutoff.set(v) / .get(). Namnen kommer ur parameter-id (bass1Cutoff -> bass1 + Cutoff, bass1 även som instr1).
// .set noteras bara (host.paramSet) och skrivs av C++ efter körningen; .get() ger värdet från före körningen. Grupper som krockar med
// Strudels egna namn (s, n, note ...) hoppas över. Ger tillbaka listan över hoppade id:n.
var paramWritten = {};
globalThis.buildParams = function (ids) {
  var skipped = [], made = {};
  function leaf(id) {
    return {
      set: function (v) {
        if (typeof v !== 'number' && typeof v !== 'string' && typeof v !== 'boolean') throw new Error(id + '.set: ge ett tal, en text eller true/false');
        paramWritten[id] = true;
        return host.paramSet(id, v);
      },
      get: function () {
        if (paramWritten[id]) throw new Error(id + '.get: läsning efter skrivning i samma körning');
        return host.paramGet(id);
      }
    };
  }
  ids.forEach(function (id) {
    var m = /^([a-z][a-z0-9]*)([A-Z].*)$/.exec(id);
    if (!m) { skipped.push(id); return; }
    var names = [m[1]];
    var inst = /^bass(\d+)$/.exec(m[1]);
    if (inst) names.push('instr' + inst[1]);
    names.forEach(function (g) {
      if (!made[g]) {
        if (g in globalThis) { made[g] = null; } else { made[g] = {}; globalThis[g] = made[g]; }
      }
      if (made[g]) made[g][m[2]] = leaf(id); else skipped.push(id);
    });
  });
  return JSON.stringify(skipped);
};
