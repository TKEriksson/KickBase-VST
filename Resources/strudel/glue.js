// Körs efter bundlen i QuickJS. Stubbar det webbläsaren normalt ger.
var S = globalThis.Strudel;
// G3: mini-strängar får dokumentpositioner. Vi skannar kodens strängliteraler och ger varje sträng sin plats (n:te förekomsten av samma text),
// så att hap.context.locations blir teckenindex i editorn.
var strLits = [], strSeen = {};
function scanStrings(code) {
  strLits = []; strSeen = {};
  for (var i = 0; i < code.length; i++) {
    var c = code[i], j;
    if (c === '/' && code[i + 1] === '/') { while (i < code.length && code[i] !== '\n') i++; }
    else if (c === '/' && code[i + 1] === '*') { j = code.indexOf('*/', i + 2); i = j < 0 ? code.length : j + 1; }
    else if (c === '"' || c === "'" || c === '`') {
      j = i + 1;
      while (j < code.length && code[j] !== c) { if (code[j] === '\\') j++; j++; }
      strLits.push({ text: code.slice(i + 1, j), q: i });
      i = j;
    }
  }
}
S.setStringParser(function (str) {
  var list = strLits.filter(function (l) { return l.text === str; });
  if (!list.length) return S.mini(str);
  strSeen[str] = strSeen[str] === undefined ? 0 : strSeen[str] + 1;
  return S.m(str, list[strSeen[str] % list.length].q);
});
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
      var lines = parts.pop().split('\n'), setRe = /^\s*[A-Za-z_]\w*\.[A-Za-z_]\w*\.set\(.*\)\s*;?\s*$/;
      while (lines.length > 1 && setRe.test(lines[0])) parts.push(lines.shift().replace(/;?\s*$/, ';')); // instrN.Param.set(..) rakt före mönstret är ett eget påstående
      var last = lines.join('\n').replace(/;\s*$/, '');
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
function noteLocs(h) { // [[start, end], ...] i dokumentet, tomt om hapen saknar position
  var ls = (h.context && h.context.locations) || [], r = [];
  for (var i = 0; i < ls.length; i++) r.push([ls[i].start, ls[i].end]);
  return r;
}
globalThis.queryJson = function (from, to) {
  var out = [];
  var haps = current.queryArc(from, to);
  for (var i = 0; i < haps.length; i++) {
    var h = haps[i];
    out.push({ b: h.whole ? h.whole.begin.valueOf() : null, e: h.whole ? h.whole.end.valueOf() : null,
               hasOnset: h.hasOnset(), v: h.value, l: noteLocs(h) });
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
globalThis.setPattern = function (code) { sliderCounter = 0; scanStrings(code); return origSet(code); };

// Modulation från koden: .lfo(param, hz, djup, form?) och .env(param, attackMs, decayMs, djup) lägger en _mod-lista på varje hap.
// C++ (LiveCode::collectMods) plockar ut den; lagret kommer från s("iN"). Parametern är ett id från list_parameters, djupet -1..1.
var modProto = Object.getPrototypeOf(S.pure(0));
var modShapes = ['sine', 'tri', 'saw', 'isaw', 'square', 'rand'];
function modNum(name, v) {
  if (typeof v !== 'number' || !isFinite(v)) throw new Error(name + ' måste vara ett tal');
  return v;
}
function addMod(pat, spec) {
  return pat.withValue(function (v) {
    var o = (v !== null && typeof v === 'object') ? Object.assign({}, v) : { note: v };
    o._mod = (o._mod || []).concat([spec]);
    return o;
  });
}
modProto.lfo = function (param, hz, depth, shape) {
  if (typeof param !== 'string') throw new Error('lfo: första argumentet är parameterns id, t.ex. "bass1Cutoff"');
  var sh = shape === undefined ? 0 : modShapes.indexOf(shape);
  if (sh < 0) throw new Error('lfo: formen är en av ' + modShapes.join(', '));
  return addMod(this, { param: param, kind: 'lfo', rate: modNum('hz', hz), depth: modNum('djup', depth === undefined ? 1 : depth), shape: sh });
};
modProto.env = function (param, attackMs, decayMs, depth) {
  if (typeof param !== 'string') throw new Error('env: första argumentet är parameterns id, t.ex. "bass1Cutoff"');
  return addMod(this, { param: param, kind: 'env', attack: modNum('attack', attackMs), decay: modNum('decay', decayMs), depth: modNum('djup', depth === undefined ? 1 : depth) });
};

// .chord(CHORD, BASETONE, START, LENGTH): staplar ackordtonerna (intervall från grundtonen) som egna händelser.
// Tonhöjd i C++-skalan (C2 = 0, som LiveCode::pitchFrom). START och LENGTH är andelar av en cykel: ett tal (0.25) eller "1/4".
// LENGTH kan vara en lista; ton k får längd k (lista kortare än ackordet börjar om). Utan LENGTH är varje ton 1/4.
var chordTypes = {
  major: [0, 4, 7], minor: [0, 3, 7], dim: [0, 3, 6], aug: [0, 4, 8], sus2: [0, 2, 7], sus4: [0, 5, 7], power: [0, 7],
  major7: [0, 4, 7, 11], minor7: [0, 3, 7, 10], dom7: [0, 4, 7, 10], dim7: [0, 3, 6, 9], minor7b5: [0, 3, 6, 10],
  major9: [0, 4, 7, 11, 14], minor9: [0, 3, 7, 10, 14], dom9: [0, 4, 7, 10, 14], add9: [0, 4, 7, 14], six: [0, 4, 7, 9], minor6: [0, 3, 7, 9]
};
function chordFraction(name, v) {
  if (typeof v === 'number' && isFinite(v)) return v;
  var m = typeof v === 'string' ? /^\s*(-?\d+(?:\.\d+)?)\s*(?:\/\s*(\d+(?:\.\d+)?))?\s*$/.exec(v) : null;
  if (!m || (m[2] !== undefined && +m[2] === 0)) throw new Error('chord: ' + name + ' är ett tal eller en bråkdel som "1/4"');
  return m[2] === undefined ? +m[1] : +m[1] / +m[2];
}
function chordPitch(v) {
  if (v === undefined) return 0;
  if (typeof v === 'number' && isFinite(v)) return Math.round(v);
  var m = typeof v === 'string' ? /^\s*([a-gA-G])([#sbf]*)(-?\d*)\s*$/.exec(v) : null;
  if (!m) throw new Error('chord: grundtonen är ett tonnamn som "c2" eller ett tal');
  var semi = { a: 9, b: 11, c: 0, d: 2, e: 4, f: 5, g: 7 }[m[1].toLowerCase()];
  for (var i = 0; i < m[2].length; i++) semi += (m[2][i] === '#' || m[2][i] === 's') ? 1 : -1;
  return ((m[3] === '' ? 3 : parseInt(m[3], 10)) + 1) * 12 + semi - 36;
}
modProto.chord = function (chord, base, start, length) {
  var intervals = Array.isArray(chord) ? chord : chordTypes[chord];
  if (!intervals) throw new Error('chord: ackordet är en av ' + Object.keys(chordTypes).join(', '));
  var root = chordPitch(base), st = start === undefined ? 0 : chordFraction('START', start);
  var lens = (length === undefined ? ['1/4'] : Array.isArray(length) ? length : [length]).map(function (l) { return chordFraction('LENGTH', l); });
  if (!lens.length) lens = [0.25];
  var self = this;
  return S.stack.apply(null, intervals.map(function (iv, k) {
    return self.note(root + iv).late(st).clip(lens[k % lens.length]);
  }));
};

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
  Object.keys(made).forEach(function (g) {
    if (!made[g]) return;
    var inst = /^instr(\d+)$/.exec(g);
    var target = inst ? 'bass' + inst[1] : (g === 'kick' ? 'kick' : null);
    if (!target) return;
    made[g].lock = function () { return host.lock(target, 'all'); };
    made[g].pianoroll = { lock: function () { return host.lock(target, 'notes'); } };
  });
  return JSON.stringify(skipped);
};

// Effekter (F3): fx1.slot2.setEffect("reverb") byter typ på plats 2 i FX1; fx1.slot2.mix.set(0.3) sätter en ratt (i egna enheter, val som text).
// Katalogen (typer, rattnamn, nuvarande typ per plats) kommer från C++ (ToolLayer::liveCodeEffects). Samma typ som förut ger ingen post, så
// handvridna rattar rörs bara när koden ändras. Namn jämförs utan versaler, mellanslag och understreck ("trancegate", "bit_crusher").
globalThis.buildEffects = function (json) {
  var cat = JSON.parse(json), typeByKey = { off: 'Off' };
  function key(s) { return String(s).toLowerCase().replace(/[^a-z0-9]/g, ''); }
  cat.types.forEach(function (t) { typeByKey[key(t)] = t; });
  cat.slots.forEach(function (row, c) {
    var g = 'fx' + (c + 1), grp = globalThis[g];
    if (grp === undefined) { grp = {}; globalThis[g] = grp; } else if (grp === null || typeof grp !== 'object') return;
    var cur = row.slice();
    row.forEach(function (t0, s) {
      var label = g + '.slot' + (s + 1), obj = {}, knobKeys = [];
      function knobs() {
        knobKeys.forEach(function (k) { delete obj[k]; });
        knobKeys = [];
        (cat.knobs[cur[s]] || []).forEach(function (name) {
          var k = key(name);
          if (k === 'seteffect') return;
          knobKeys.push(k);
          obj[k] = { set: function (v) {
            if (typeof v !== 'number' && typeof v !== 'string') throw new Error(label + '.' + k + '.set: ge ett tal eller en text');
            host.effect(g, s + 1, cur[s], name, v);
            return v;
          } };
        });
      }
      obj.setEffect = function (name) {
        var type = typeByKey[key(name)];
        if (!type) throw new Error(label + '.setEffect: okänd effekt "' + name + '". Val: off, ' + cat.types.map(key).join(', '));
        if (type !== cur[s]) { cur[s] = type; host.effect(g, s + 1, type, '', 0); knobs(); }
        return type;
      };
      knobs();
      grp['slot' + (s + 1)] = new Proxy(obj, { get: function (t, k) {
        if (typeof k === 'symbol' || k in t) return t[k];
        throw new Error(label + ' (' + cur[s] + ') har ingen ratt "' + String(k) + '"' + (knobKeys.length ? '. Rattar: ' + knobKeys.join(', ') : ' (platsen är tom, börja med setEffect)'));
      } });
    });
  });
};

// Mätare (F4): meter.outputPeak, meter.gainReduction, meter.duck1..10, meter.solo. Bara .get() (värdet från före körningen); .set/.lfo/.lock ger felrad.
globalThis.buildMeters = function (names) {
  var m = {};
  names.forEach(function (n) {
    var leaf = { get: function () { return host.paramGet('meter.' + n); } };
    ['set', 'lfo', 'LFO', 'lock'].forEach(function (bad) {
      leaf[bad] = function () { throw new Error('meter.' + n + '.' + bad + ': en mätare kan bara läsas, använd .get()'); };
    });
    m[n] = leaf;
  });
  globalThis.meter = m;
};
