/* Contains Strudel (@strudel/core, @strudel/mini), Copyright (C) Strudel contributors, AGPL-3.0-or-later, https://codeberg.org/uzu/strudel */
var console = { log(){}, warn(){}, error(){}, debug(){}, info(){} };
// Webbläsar-globaler som Strudel/kabelsalat rör vid inläsning. Stubbade, inget körs på riktigt.
var performance = { now: function () { return Date.now(); } };
var setTimeout = function () { return 0; }, clearTimeout = function () {}, setInterval = setTimeout, clearInterval = clearTimeout;
(() => {
  var __defProp = Object.defineProperty;
  var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
  var __export = (target, all) => {
    for (var name in all)
      __defProp(target, name, { get: all[name], enumerable: true });
  };
  var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);

  // node_modules/@strudel/core/dist/index.mjs
  var dist_exports = {};
  __export(dist_exports, {
    ClockCollator: () => _n,
    Cyclist: () => hf,
    FXr: () => uf,
    FXrel: () => cf,
    FXrelease: () => of,
    Fraction: () => m,
    Hap: () => S2,
    Pattern: () => f2,
    State: () => ut2,
    TimeSpan: () => B2,
    __chooseWith: () => Me2,
    _brandBy: () => Oe2,
    _fitslice: () => Fn,
    _irand: () => ze2,
    _keyDown: () => je2,
    _match: () => In,
    _mod: () => bt2,
    _morph: () => de2,
    _polymeterListSteps: () => Hn,
    _retime: () => Yt2,
    _slices: () => Zt2,
    accelerate: () => Ys,
    activeLabel: () => va,
    ad: () => Bp,
    add: () => $h,
    adsr: () => xp,
    almostAlways: () => Jw,
    almostNever: () => jw,
    always: () => Nw,
    amp: () => sr,
    analyze: () => gc,
    anchor: () => Ua,
    and: () => nd,
    apply: () => Pd,
    applyN: () => Wn,
    ar: () => zp,
    arp: () => wh,
    arpWith: () => yh,
    arrange: () => kh,
    as: () => Kp,
    asym: () => py,
    att: () => or,
    attack: () => rr,
    averageArray: () => nn,
    backgroundImage: () => Hw,
    band: () => Ih,
    bandf: () => Cc,
    bandq: () => Oc,
    bank: () => yc,
    base64ToUnicode: () => gn,
    bbexpr: () => ti2,
    bbst: () => ni2,
    beat: () => oy,
    begin: () => Mc,
    berlin: () => qw,
    bgain: () => ja,
    binary: () => iw,
    binaryL: () => uw,
    binaryN: () => Jf,
    binaryNL: () => $f,
    bind: () => xh,
    binshift: () => Zl,
    bite: () => Ld,
    bjork: () => _y,
    bjorklund: () => ge2,
    blshift: () => Dh,
    bmod: () => sf,
    bor: () => Vh,
    bp: () => Bc,
    bpa: () => ji2,
    bpattack: () => Ei2,
    bpd: () => Wi2,
    bpdc: () => yu,
    bpdecay: () => Ri2,
    bpdepth: () => fu,
    bpdepthfreq: () => du,
    bpdepthfrequency: () => hu,
    bpe: () => Bi2,
    bpenv: () => xi2,
    bpf: () => xc,
    bpq: () => zc,
    bpr: () => Zi2,
    bprate: () => lu,
    bprelease: () => Yi2,
    bps: () => Gi2,
    bpshape: () => mu,
    bpskew: () => wu,
    bpsustain: () => Di2,
    bpsync: () => pu,
    brak: () => Ud,
    brand: () => mw,
    brandBy: () => dw,
    brshift: () => Gh,
    bus: () => Pa,
    busgain: () => Ea,
    bxor: () => Hh,
    bypass: () => Tm,
    byteBeatExpression: () => Zc,
    byteBeatStartTime: () => ei2,
    calculateSteps: () => dh,
    cat: () => mt2,
    ccn: () => $p,
    ccv: () => Np,
    ceil: () => id,
    ch: () => ri2,
    channel: () => gi2,
    channels: () => si2,
    chebyshev: () => dy,
    choose: () => Rf,
    chooseCycles: () => Wf,
    chooseIn: () => ww,
    chooseInWith: () => Pe2,
    chooseOut: () => gw,
    chooseWith: () => It2,
    chop: () => Xm,
    chord: () => Da,
    chorus: () => wc,
    chunk: () => mm,
    chunkBack: () => gm,
    chunkBackInto: () => Am,
    chunkInto: () => qm,
    chunkback: () => bm,
    chunkbackinto: () => Sm,
    chunkinto: () => km,
    clamp: () => an,
    cleanupUi: () => Dw,
    clip: () => pp,
    coarse: () => Rc,
    code2hash: () => ph,
    color: () => Ap,
    colour: () => Tp,
    comb: () => Xl,
    compose: () => oh,
    compress: () => dd,
    compressSpan: () => md,
    compressor: () => bl,
    compressorAttack: () => kl,
    compressorKnee: () => _l,
    compressorRatio: () => vl,
    compressorRelease: () => ql,
    compressspan: () => yd,
    constant: () => ch,
    contract: () => Kn,
    control: () => Jp,
    controls: () => gy,
    cosine: () => Qy,
    cosine2: () => Uy,
    cpm: () => Ed,
    cps: () => lp,
    createClock: () => ff,
    createParam: () => Nt2,
    createParams: () => Cp,
    crush: () => Lc,
    ctf: () => vi2,
    ctlNum: () => Lp,
    ctranspose: () => Aa,
    cubic: () => ay,
    curry: () => w2,
    curve: () => yp,
    cut: () => bi2,
    cutoff: () => _i2,
    cycleToSeconds: () => Kt2,
    cyclesPer: () => Ww,
    dec: () => vc,
    decay: () => _c,
    degrade: () => Tw,
    degradeBy: () => Aw,
    degradeByWith: () => Sw,
    degree: () => qa,
    delay: () => Ru,
    delayfb: () => Fu,
    delayfeedback: () => Wu,
    delayspeed: () => Vu,
    delaysync: () => Qu,
    delayt: () => Du,
    delaytime: () => Hu,
    deltaSlide: () => wp,
    det: () => Ku,
    detune: () => Xu,
    dfb: () => Iu,
    dict: () => Qa,
    dictionary: () => Ga,
    diode: () => ly,
    dist: () => yl,
    distort: () => ml,
    distorttype: () => gl,
    distortvol: () => wl,
    div: () => Rh,
    djf: () => Lu,
    drawLine: () => Cn,
    drive: () => Qc,
    drop: () => Qn,
    dry: () => ta,
    ds: () => Op,
    dt: () => Gu,
    duck: () => Uc,
    duckattack: () => Yc,
    duckdepth: () => Xc,
    duckonset: () => Kc,
    dur: () => dp,
    duration: () => hp,
    early: () => jd,
    echo: () => im,
    echoWith: () => sm,
    echowith: () => rm,
    eish: () => Ty,
    end: () => Pc,
    enhance: () => Ul,
    env: () => nf,
    eq: () => Yh,
    eqt: () => Zh,
    errorLogger: () => zt2,
    euclid: () => by,
    euclidLegato: () => qy,
    euclidLegatoRot: () => Sy,
    euclidRot: () => ky,
    euclidish: () => Ay,
    euclidrot: () => vy,
    evalScope: () => xn,
    evaluate: () => On,
    every: () => Md,
    expand: () => Xn,
    expression: () => Ol,
    extend: () => Un,
    fadeInTime: () => sa,
    fadeOutTime: () => na,
    fadeTime: () => ea,
    fanchor: () => eu,
    fast: () => qd,
    fastChunk: () => vm,
    fastGap: () => wd,
    fastcat: () => N2,
    fastchunk: () => _m,
    fastgap: () => gd,
    fft: () => bc,
    filter: () => zm,
    filterWhen: () => Mm,
    firstOf: () => zd,
    fit: () => ey,
    flatten: () => G,
    floor: () => cd,
    fm: () => Sr,
    fm1: () => Ar,
    fm2: () => Tr,
    fm3: () => Cr,
    fm4: () => xr,
    fm5: () => Br,
    fm6: () => Or,
    fm7: () => zr,
    fm8: () => Mr,
    fmatt: () => Kr,
    fmatt1: () => Yr,
    fmatt2: () => Zr,
    fmatt3: () => to,
    fmatt4: () => eo,
    fmatt5: () => no,
    fmatt6: () => so,
    fmatt7: () => ro,
    fmatt8: () => oo,
    fmattack: () => Fr,
    fmattack1: () => Ir,
    fmattack2: () => Vr,
    fmattack3: () => Hr,
    fmattack4: () => Dr,
    fmattack5: () => Gr,
    fmattack6: () => Qr,
    fmattack7: () => Ur,
    fmattack8: () => Xr,
    fmdec: () => Ao,
    fmdec1: () => To,
    fmdec2: () => Co,
    fmdec3: () => xo,
    fmdec4: () => Bo,
    fmdec5: () => Oo,
    fmdec6: () => zo,
    fmdec7: () => Mo,
    fmdec8: () => Po,
    fmdecay: () => yo,
    fmdecay1: () => wo,
    fmdecay2: () => go,
    fmdecay3: () => bo,
    fmdecay4: () => _o,
    fmdecay5: () => vo,
    fmdecay6: () => ko,
    fmdecay7: () => qo,
    fmdecay8: () => So,
    fmenv: () => Pr,
    fmenv1: () => Er,
    fmenv2: () => jr,
    fmenv3: () => Jr,
    fmenv4: () => $r,
    fmenv5: () => Nr,
    fmenv6: () => Lr,
    fmenv7: () => Rr,
    fmenv8: () => Wr,
    fmh: () => cr,
    fmh1: () => ir,
    fmh2: () => ur,
    fmh3: () => ar,
    fmh4: () => lr,
    fmh5: () => pr,
    fmh6: () => fr,
    fmh7: () => hr,
    fmh8: () => dr,
    fmi: () => mr,
    fmi1: () => yr,
    fmi2: () => wr,
    fmi3: () => gr,
    fmi4: () => br,
    fmi5: () => _r,
    fmi6: () => vr,
    fmi7: () => kr,
    fmi8: () => qr,
    fmrel: () => ic,
    fmrel1: () => uc,
    fmrel2: () => ac,
    fmrel3: () => lc,
    fmrel4: () => pc,
    fmrel5: () => fc,
    fmrel6: () => hc,
    fmrel7: () => dc,
    fmrel8: () => mc,
    fmrelease: () => Yo,
    fmrelease1: () => Zo,
    fmrelease2: () => tc,
    fmrelease3: () => ec,
    fmrelease4: () => nc,
    fmrelease5: () => sc,
    fmrelease6: () => rc,
    fmrelease7: () => oc,
    fmrelease8: () => cc,
    fmsus: () => Io,
    fmsus1: () => Vo,
    fmsus2: () => Ho,
    fmsus3: () => Do,
    fmsus4: () => Go,
    fmsus5: () => Qo,
    fmsus6: () => Uo,
    fmsus7: () => Xo,
    fmsus8: () => Ko,
    fmsustain: () => Eo,
    fmsustain1: () => jo,
    fmsustain2: () => Jo,
    fmsustain3: () => $o,
    fmsustain4: () => No,
    fmsustain5: () => Lo,
    fmsustain6: () => Ro,
    fmsustain7: () => Wo,
    fmsustain8: () => Fo,
    fmwave: () => co,
    fmwave1: () => io,
    fmwave2: () => uo,
    fmwave3: () => ao,
    fmwave4: () => lo,
    fmwave5: () => po,
    fmwave6: () => fo,
    fmwave7: () => ho,
    fmwave8: () => mo,
    focus: () => bd,
    focusSpan: () => _d,
    focusspan: () => vd,
    fold: () => fy,
    fractionalArgs: () => ih,
    frameRate: () => np,
    frames: () => sp,
    freeze: () => Vl,
    freq: () => ra,
    freqToMidi: () => Ze2,
    fromBipolar: () => ad,
    fshift: () => Ml,
    fshiftnote: () => Pl,
    fshiftphase: () => El,
    ftype: () => tu,
    func: () => rd,
    fxr: () => af,
    gain: () => er,
    gap: () => pt2,
    gat: () => wa,
    gate: () => ya,
    getAccidentalsOffset: () => Ye2,
    getControlName: () => yt2,
    getCps: () => Fy,
    getCurrentKeyboardState: () => qn,
    getEventOffsetMs: () => th,
    getFreq: () => tn,
    getFrequency: () => rh,
    getIsStarted: () => Hy,
    getPattern: () => Iy,
    getPerformanceTimeSeconds: () => hh,
    getPlayableNoteValue: () => sh,
    getRandsAtTime: () => K2,
    getSoundIndex: () => nh,
    getTime: () => Wy,
    getTrigger: () => Sf,
    getTriggerFunc: () => Vy,
    grow: () => jm,
    gt: () => Uh,
    gte: () => Kh,
    hard: () => uy,
    harmonic: () => Ta,
    hash2code: () => fh,
    hbrick: () => tp,
    hcutoff: () => Mu,
    hold: () => Tc,
    hours: () => rp,
    hp: () => Eu,
    hpa: () => Pi2,
    hpattack: () => Mi2,
    hpd: () => Li2,
    hpdc: () => Su,
    hpdecay: () => Ni2,
    hpdepth: () => _u,
    hpdepthfreq: () => ku,
    hpdepthfrequency: () => vu,
    hpe: () => Ci2,
    hpenv: () => Ti2,
    hpf: () => Pu,
    hpq: () => Ju,
    hpr: () => Ki2,
    hprate: () => gu,
    hprelease: () => Xi2,
    hps: () => Hi2,
    hpshape: () => qu,
    hpskew: () => Au,
    hpsustain: () => Vi2,
    hpsync: () => bu,
    hresonance: () => ju,
    hsl: () => Om,
    hsla: () => Bm,
    hurry: () => Ad,
    id: () => ot2,
    imag: () => Ql,
    inhabit: () => Jy,
    inhabitmod: () => Ny,
    innerBind: () => Bh,
    inside: () => xd,
    inv: () => Dd,
    invert: () => Hd,
    ir: () => cl,
    irand: () => yw,
    irbegin: () => al,
    iresponse: () => il,
    irspeed: () => ul,
    isControlName: () => is,
    isNote: () => Mt2,
    isNoteWithOctave: () => Yf,
    isPattern: () => pe2,
    isaw: () => Rt2,
    isaw2: () => Ae2,
    iter: () => pm,
    iterBack: () => fm,
    iterback: () => hm,
    itri: () => Zy,
    itri2: () => tw,
    jux: () => nm,
    juxBy: () => tm,
    juxby: () => em,
    kcutoff: () => $l,
    keep: () => jh,
    keepif: () => Jh,
    keyAlias: () => kn,
    keyDown: () => Rw,
    krush: () => Jl,
    label: () => ka,
    lastOf: () => Od,
    late: () => Ln,
    lbrick: () => ep,
    legato: () => fp,
    leslie: () => ga,
    lfo: () => ef,
    linger: () => Rd,
    listRange: () => _t2,
    lock: () => Uu,
    logKey: () => oe2,
    logger: () => E2,
    loop: () => Ec,
    loopAt: () => Zm,
    loopAtCps: () => ny,
    loopBegin: () => jc,
    loopEnd: () => $c,
    loopat: () => ty,
    loopatcps: () => sy,
    loopb: () => Jc,
    loope: () => Nc,
    lp: () => qi2,
    lpa: () => zi2,
    lpattack: () => Oi2,
    lpd: () => $i2,
    lpdc: () => uu,
    lpdecay: () => Ji2,
    lpdepth: () => ru,
    lpdepthfreq: () => cu,
    lpdepthfrequency: () => ou,
    lpe: () => Ai2,
    lpenv: () => Si2,
    lpf: () => ki2,
    lpq: () => Nu,
    lpr: () => Ui2,
    lprate: () => nu,
    lprelease: () => Qi2,
    lps: () => Ii2,
    lpshape: () => iu,
    lpskew: () => au,
    lpsustain: () => Fi2,
    lpsync: () => su,
    lrate: () => ba,
    lsize: () => _a,
    lt: () => Qh,
    lte: () => Xh,
    mapArgs: () => ie2,
    mask: () => Sh,
    midi2note: () => eh,
    midiToFreq: () => it2,
    midibend: () => Dp,
    midichan: () => Mp,
    midicmd: () => jp,
    midimap: () => Pp,
    midiport: () => Ep,
    miditouch: () => Gp,
    minutes: () => op,
    mod: () => Wh,
    mode: () => Ya,
    morph: () => cy,
    mouseX: () => ow,
    mouseY: () => sw,
    mousex: () => rw,
    mousey: () => nw,
    mtranspose: () => Sa,
    mul: () => Lh,
    n: () => Xs,
    nanFallback: () => sn,
    ne: () => td,
    net: () => ed,
    never: () => $w,
    noise: () => Bu,
    note: () => Ks,
    noteToMidi: () => gt2,
    nothing: () => R,
    nrpnn: () => Rp,
    nrpv: () => Wp,
    nudge: () => Ba,
    numeralArgs: () => L2,
    objectMap: () => bn,
    oct: () => za,
    octave: () => Oa,
    octaveR: () => xa,
    octaves: () => Ka,
    octer: () => Nl,
    octersub: () => Ll,
    octersubsub: () => Rl,
    off: () => Qd,
    offset: () => Xa,
    often: () => Pw,
    or: () => sd,
    orbit: () => Ma,
    oschost: () => Up,
    oscport: () => Xp,
    outerBind: () => Oh,
    outside: () => Bd,
    overgain: () => Ja,
    overshape: () => $a,
    pace: () => Vn,
    pairs: () => un,
    palindrome: () => Zd,
    pan: () => Na,
    panchor: () => ma,
    panorient: () => Fa,
    panspan: () => La,
    pansplay: () => Ra,
    panwidth: () => Wa,
    parray: () => me2,
    parseFractional: () => cn,
    parseNumeral: () => ce2,
    partials: () => my,
    patt: () => ca,
    pattack: () => oa,
    pcurve: () => da,
    pdec: () => ua,
    pdecay: () => ia,
    penv: () => ha,
    per: () => Df,
    perCycle: () => Fw,
    perlin: () => kw,
    perx: () => Iw,
    ph: () => ai2,
    phasdp: () => wi2,
    phaser: () => li2,
    phasercenter: () => hi2,
    phaserdepth: () => mi2,
    phaserrate: () => ui2,
    phasersweep: () => pi2,
    phases: () => yy,
    phc: () => di2,
    phd: () => yi2,
    phs: () => fi2,
    pick: () => yf,
    pickF: () => xy,
    pickOut: () => Oy,
    pickReset: () => Ey,
    pickRestart: () => My,
    pickSqueeze: () => $y,
    pickmod: () => gf,
    pickmodF: () => By,
    pickmodOut: () => zy,
    pickmodReset: () => jy,
    pickmodRestart: () => Py,
    pickmodSqueeze: () => Ly,
    pipe: () => on,
    pitchJump: () => gp,
    pitchJumpTime: () => bp,
    ply: () => kd,
    plyForEach: () => lm,
    plyWith: () => am,
    pm: () => _h,
    polyBind: () => Ph,
    polyTouch: () => Qp,
    polymeter: () => $t2,
    polyrhythm: () => gh,
    postgain: () => nr,
    pow: () => Fh,
    pr: () => bh,
    prel: () => fa,
    prelease: () => pa,
    press: () => Yd,
    pressBy: () => Kd,
    progNum: () => Fp,
    psus: () => la,
    psustain: () => aa,
    pure: () => C2,
    pw: () => oi2,
    pwrate: () => ci2,
    pwsweep: () => ii2,
    rand: () => W2,
    rand2: () => hw,
    randL: () => aw,
    randcat: () => bw,
    randrun: () => Nf,
    range: () => ld,
    range2: () => fd,
    rangex: () => pd,
    rarely: () => Ew,
    ratio: () => hd,
    rdim: () => sl,
    real: () => Gl,
    ref: () => ry,
    register: () => l,
    registerControl: () => c2,
    registerMultiControl: () => V2,
    reify: () => d,
    rel: () => Ac,
    release: () => Sc,
    removeUndefineds: () => lt2,
    repeatCycles: () => dm,
    repl: () => Dy,
    replicate: () => Em,
    reset_state: () => df,
    reset_timelines: () => mf,
    resonance: () => $u,
    rev: () => Rn,
    revv: () => Xd,
    rfade: () => ol,
    rib: () => xm,
    ribbon: () => Cm,
    ring: () => Wl,
    ringdf: () => Il,
    ringf: () => Fl,
    rlp: () => el,
    room: () => Za,
    roomdim: () => nl,
    roomfade: () => rl,
    roomlp: () => tl,
    roomsize: () => ll,
    rotate: () => rn,
    round: () => od,
    rsize: () => hl,
    run: () => jf,
    s: () => us,
    s_add: () => Fm,
    s_alt: () => Nm,
    s_cat: () => $m,
    s_contract: () => Dm,
    s_expand: () => Vm,
    s_extend: () => Hm,
    s_polymeter: () => Lm,
    s_sub: () => Im,
    s_taper: () => Rm,
    s_taperlist: () => Wm,
    s_tour: () => Gm,
    s_zip: () => Qm,
    saw: () => qt2,
    saw2: () => Se2,
    scram: () => Yl,
    scramble: () => pw,
    scrub: () => Yp,
    seconds: () => cp,
    seed: () => fw,
    seg: () => Fd,
    segment: () => Wd,
    semitone: () => Va,
    seq: () => Nn,
    seqPLoop: () => qh,
    sequence: () => Q2,
    sequenceP: () => En,
    set: () => Eh,
    setCpsFunc: () => _f,
    setIsStarted: () => qf,
    setPattern: () => vf,
    setStringParser: () => mh,
    setTime: () => ee2,
    setTriggerFunc: () => kf,
    shape: () => dl,
    shrink: () => Zn,
    shrinklist: () => Yn,
    shuffle: () => lw,
    signal: () => j2,
    silence: () => q2,
    sine: () => Af,
    sine2: () => Te2,
    sinefold: () => hy,
    size: () => pl,
    slice: () => ss,
    slide: () => Ia,
    slow: () => Td,
    slowChunk: () => wm,
    slowcat: () => Z2,
    slowcatPrime: () => fe2,
    slowchunk: () => ym,
    smear: () => Kl,
    soft: () => iy,
    sol2note: () => uh,
    someCycles: () => Mw,
    someCyclesBy: () => zw,
    sometimes: () => Ow,
    sometimesBy: () => Bw,
    songPtr: () => ip,
    sound: () => as,
    source: () => Qs,
    sparsity: () => Cd,
    speak: () => Vw,
    speed: () => ye2,
    splice: () => Ym,
    splitAt: () => ue2,
    spread: () => Zu,
    square: () => Tf,
    square2: () => Xy,
    squeeze: () => Ry,
    squeezeBind: () => zh,
    squiz: () => Tl,
    src: () => Us,
    stack: () => z,
    stackBy: () => vh,
    stackCentre: () => $n,
    stackLeft: () => jn,
    stackRight: () => Jn,
    steady: () => Gy,
    stepBind: () => Mh,
    stepalt: () => Dn,
    stepcat: () => $2,
    steps: () => Um,
    stepsPerOctave: () => Ca,
    stretch: () => Sl,
    striate: () => Km,
    stringifyValues: () => ae,
    struct: () => Ah,
    strudelScope: () => le,
    stut: () => um,
    stutWith: () => om,
    stutwith: () => cm,
    sub: () => Nh,
    superimpose: () => Th,
    sus: () => qc,
    sustain: () => kc,
    sustainpedal: () => zl,
    swing: () => Vd,
    swingBy: () => Id,
    sysex: () => Ip,
    sysexdata: () => Hp,
    sysexid: () => Vp,
    sz: () => fl,
    take: () => Gn,
    time: () => ew,
    timeCat: () => ns,
    timecat: () => Jm,
    timeline: () => Cy,
    toBipolar: () => ud,
    tokenizeNote: () => Ue2,
    tour: () => ts,
    transient: () => rf,
    trem: () => Fc,
    tremolo: () => Wc,
    tremolodepth: () => Vc,
    tremolophase: () => Dc,
    tremoloshape: () => Gc,
    tremoloskew: () => Hc,
    tremolosync: () => Ic,
    tri: () => Ky,
    tri2: () => Yy,
    triode: () => jl,
    tsdelay: () => Dl,
    uid: () => up,
    undegrade: () => xw,
    undegradeBy: () => Cw,
    unicodeToBase64: () => wn,
    uniq: () => ah,
    uniqsort: () => lh,
    uniqsortr: () => yn,
    unison: () => Yu,
    unit: () => Al,
    useRNG: () => cw,
    v: () => xu,
    val: () => ap,
    valueToMidi: () => Zf,
    vel: () => tr,
    velocity: () => Zs,
    vib: () => Tu,
    vibmod: () => Ou,
    vibrato: () => Cu,
    vmod: () => zu,
    voice: () => Ha,
    vowel: () => Cl,
    warp: () => Cs,
    warpatt: () => Os,
    warpattack: () => Bs,
    warpdc: () => Rs,
    warpdec: () => Ms,
    warpdecay: () => zs,
    warpdepth: () => Ns,
    warpenv: () => Ds,
    warpmode: () => Fs,
    warprate: () => $s,
    warprel: () => Js,
    warprelease: () => js,
    warpshape: () => Ls,
    warpskew: () => Ws,
    warpsus: () => Es,
    warpsustain: () => Ps,
    warpsync: () => Gs,
    waveloss: () => xl,
    wavetablePhaseRand: () => Hs,
    wavetablePosition: () => ps,
    wavetableWarp: () => xs,
    wavetableWarpMode: () => Is,
    wchoose: () => _w,
    wchooseCycles: () => If,
    when: () => Gd,
    whenKey: () => Lw,
    withSeed: () => Lf,
    withValue: () => Ch,
    within: () => Pm,
    worklet: () => wy,
    wrandcat: () => vw,
    wt: () => ls,
    wtatt: () => ds,
    wtattack: () => hs,
    wtdc: () => As,
    wtdec: () => ys,
    wtdecay: () => ms,
    wtdepth: () => qs,
    wtenv: () => fs,
    wtphaserand: () => Vs,
    wtrate: () => vs,
    wtrel: () => _s,
    wtrelease: () => bs,
    wtshape: () => Ss,
    wtskew: () => Ts,
    wtsus: () => gs,
    wtsustain: () => ws,
    wtsync: () => ks,
    xfade: () => rs,
    xsdelay: () => Hl,
    zcrush: () => kp,
    zdelay: () => qp,
    zip: () => es,
    zipWith: () => Pt2,
    zmod: () => vp,
    znoise: () => _p,
    zoom: () => Jd,
    zoomArc: () => $d,
    zoomarc: () => Nd,
    zrand: () => mp,
    zzfx: () => Sp
  });

  // node_modules/fraction.js/dist/fraction.mjs
  if (typeof BigInt === "undefined") BigInt = function(n) {
    if (isNaN(n)) throw new Error("");
    return n;
  };
  var C_ZERO = BigInt(0);
  var C_ONE = BigInt(1);
  var C_TWO = BigInt(2);
  var C_THREE = BigInt(3);
  var C_FIVE = BigInt(5);
  var C_TEN = BigInt(10);
  var MAX_INTEGER = BigInt(Number.MAX_SAFE_INTEGER);
  var MAX_CYCLE_LEN = 2e3;
  var P = {
    "s": C_ONE,
    "n": C_ZERO,
    "d": C_ONE
  };
  function assign(n, s2) {
    try {
      n = BigInt(n);
    } catch (e) {
      throw InvalidParameter();
    }
    return n * s2;
  }
  function ifloor(x2) {
    return typeof x2 === "bigint" ? x2 : Math.floor(x2);
  }
  function newFraction(n, d2) {
    if (d2 === C_ZERO) {
      throw DivisionByZero();
    }
    const f3 = Object.create(Fraction.prototype);
    f3["s"] = n < C_ZERO ? -C_ONE : C_ONE;
    n = n < C_ZERO ? -n : n;
    const a2 = gcd(n, d2);
    f3["n"] = n / a2;
    f3["d"] = d2 / a2;
    return f3;
  }
  var FACTORSTEPS = [C_TWO * C_TWO, C_TWO, C_TWO * C_TWO, C_TWO, C_TWO * C_TWO, C_TWO * C_THREE, C_TWO, C_TWO * C_THREE];
  function factorize(n) {
    const factors = /* @__PURE__ */ Object.create(null);
    if (n <= C_ONE) {
      factors[n] = C_ONE;
      return factors;
    }
    const add2 = (p) => {
      factors[p] = (factors[p] || C_ZERO) + C_ONE;
    };
    while (n % C_TWO === C_ZERO) {
      add2(C_TWO);
      n /= C_TWO;
    }
    while (n % C_THREE === C_ZERO) {
      add2(C_THREE);
      n /= C_THREE;
    }
    while (n % C_FIVE === C_ZERO) {
      add2(C_FIVE);
      n /= C_FIVE;
    }
    for (let si3 = 0, p = C_TWO + C_FIVE; p * p <= n; ) {
      while (n % p === C_ZERO) {
        add2(p);
        n /= p;
      }
      p += FACTORSTEPS[si3];
      si3 = si3 + 1 & 7;
    }
    if (n > C_ONE) add2(n);
    return factors;
  }
  var parse = function(p1, p2) {
    let n = C_ZERO, d2 = C_ONE, s2 = C_ONE;
    if (p1 === void 0 || p1 === null) {
    } else if (p2 !== void 0) {
      if (typeof p1 === "bigint") {
        n = p1;
      } else if (isNaN(p1)) {
        throw InvalidParameter();
      } else if (p1 % 1 !== 0) {
        throw NonIntegerParameter();
      } else {
        n = BigInt(p1);
      }
      if (typeof p2 === "bigint") {
        d2 = p2;
      } else if (isNaN(p2)) {
        throw InvalidParameter();
      } else if (p2 % 1 !== 0) {
        throw NonIntegerParameter();
      } else {
        d2 = BigInt(p2);
      }
      s2 = n * d2;
    } else if (typeof p1 === "object") {
      if ("d" in p1 && "n" in p1) {
        n = BigInt(p1["n"]);
        d2 = BigInt(p1["d"]);
        if ("s" in p1)
          n *= BigInt(p1["s"]);
      } else if (0 in p1) {
        n = BigInt(p1[0]);
        if (1 in p1)
          d2 = BigInt(p1[1]);
      } else if (typeof p1 === "bigint") {
        n = p1;
      } else {
        throw InvalidParameter();
      }
      s2 = n * d2;
    } else if (typeof p1 === "number") {
      if (isNaN(p1)) {
        throw InvalidParameter();
      }
      if (p1 < 0) {
        s2 = -C_ONE;
        p1 = -p1;
      }
      if (p1 % 1 === 0) {
        n = BigInt(p1);
      } else {
        let z3 = 1;
        let A3 = 0, B3 = 1;
        let C4 = 1, D2 = 1;
        let N4 = 1e7;
        if (p1 >= 1) {
          z3 = 10 ** Math.floor(1 + Math.log10(p1));
          p1 /= z3;
        }
        while (B3 <= N4 && D2 <= N4) {
          let M2 = (A3 + C4) / (B3 + D2);
          if (p1 === M2) {
            if (B3 + D2 <= N4) {
              n = A3 + C4;
              d2 = B3 + D2;
            } else if (D2 > B3) {
              n = C4;
              d2 = D2;
            } else {
              n = A3;
              d2 = B3;
            }
            break;
          } else {
            if (p1 > M2) {
              A3 += C4;
              B3 += D2;
            } else {
              C4 += A3;
              D2 += B3;
            }
            if (B3 > N4) {
              n = C4;
              d2 = D2;
            } else {
              n = A3;
              d2 = B3;
            }
          }
        }
        n = BigInt(n) * BigInt(z3);
        d2 = BigInt(d2);
      }
    } else if (typeof p1 === "string") {
      let ndx = 0;
      let v = C_ZERO, w3 = C_ZERO, x2 = C_ZERO, y2 = C_ONE, z3 = C_ONE;
      let match = p1.replace(/_/g, "").match(/\d+|./g);
      if (match === null)
        throw InvalidParameter();
      if (match[ndx] === "-") {
        s2 = -C_ONE;
        ndx++;
      } else if (match[ndx] === "+") {
        ndx++;
      }
      if (match.length === ndx + 1) {
        w3 = assign(match[ndx++], s2);
      } else if (match[ndx + 1] === "." || match[ndx] === ".") {
        if (match[ndx] !== ".") {
          v = assign(match[ndx++], s2);
        }
        ndx++;
        if (ndx + 1 === match.length || match[ndx + 1] === "(" && match[ndx + 3] === ")" || match[ndx + 1] === "'" && match[ndx + 3] === "'") {
          w3 = assign(match[ndx], s2);
          y2 = C_TEN ** BigInt(match[ndx].length);
          ndx++;
        }
        if (match[ndx] === "(" && match[ndx + 2] === ")" || match[ndx] === "'" && match[ndx + 2] === "'") {
          x2 = assign(match[ndx + 1], s2);
          z3 = C_TEN ** BigInt(match[ndx + 1].length) - C_ONE;
          ndx += 3;
        }
      } else if (match[ndx + 1] === "/" || match[ndx + 1] === ":") {
        w3 = assign(match[ndx], s2);
        y2 = assign(match[ndx + 2], C_ONE);
        ndx += 3;
      } else if (match[ndx + 3] === "/" && match[ndx + 1] === " ") {
        v = assign(match[ndx], s2);
        w3 = assign(match[ndx + 2], s2);
        y2 = assign(match[ndx + 4], C_ONE);
        ndx += 5;
      }
      if (match.length <= ndx) {
        d2 = y2 * z3;
        s2 = /* void */
        n = x2 + d2 * v + z3 * w3;
      } else {
        throw InvalidParameter();
      }
    } else if (typeof p1 === "bigint") {
      n = p1;
      s2 = p1;
      d2 = C_ONE;
    } else {
      throw InvalidParameter();
    }
    if (d2 === C_ZERO) {
      throw DivisionByZero();
    }
    P["s"] = s2 < C_ZERO ? -C_ONE : C_ONE;
    P["n"] = n < C_ZERO ? -n : n;
    P["d"] = d2 < C_ZERO ? -d2 : d2;
  };
  function modpow(b2, e, m3) {
    let r = C_ONE;
    for (; e > C_ZERO; b2 = b2 * b2 % m3, e >>= C_ONE) {
      if (e & C_ONE) {
        r = r * b2 % m3;
      }
    }
    return r;
  }
  function cycleLen(n, d2) {
    for (; d2 % C_TWO === C_ZERO; d2 /= C_TWO) {
    }
    for (; d2 % C_FIVE === C_ZERO; d2 /= C_FIVE) {
    }
    if (d2 === C_ONE)
      return C_ZERO;
    let rem = C_TEN % d2;
    let t = 1;
    for (; rem !== C_ONE; t++) {
      rem = rem * C_TEN % d2;
      if (t > MAX_CYCLE_LEN)
        return C_ZERO;
    }
    return BigInt(t);
  }
  function cycleStart(n, d2, len) {
    let rem1 = C_ONE;
    let rem2 = modpow(C_TEN, len, d2);
    for (let t = 0; t < 300; t++) {
      if (rem1 === rem2)
        return BigInt(t);
      rem1 = rem1 * C_TEN % d2;
      rem2 = rem2 * C_TEN % d2;
    }
    return 0;
  }
  function gcd(a2, b2) {
    if (!a2)
      return b2;
    if (!b2)
      return a2;
    while (1) {
      a2 %= b2;
      if (!a2)
        return b2;
      b2 %= a2;
      if (!b2)
        return a2;
    }
  }
  function Fraction(a2, b2) {
    parse(a2, b2);
    if (this instanceof Fraction) {
      a2 = gcd(P["d"], P["n"]);
      this["s"] = P["s"];
      this["n"] = P["n"] / a2;
      this["d"] = P["d"] / a2;
    } else {
      return newFraction(P["s"] * P["n"], P["d"]);
    }
  }
  var DivisionByZero = function() {
    return new Error("Division by Zero");
  };
  var InvalidParameter = function() {
    return new Error("Invalid argument");
  };
  var NonIntegerParameter = function() {
    return new Error("Parameters must be integer");
  };
  Fraction.prototype = {
    "s": C_ONE,
    "n": C_ZERO,
    "d": C_ONE,
    /**
     * Calculates the absolute value
     *
     * Ex: new Fraction(-4).abs() => 4
     **/
    "abs": function() {
      return newFraction(this["n"], this["d"]);
    },
    /**
     * Inverts the sign of the current fraction
     *
     * Ex: new Fraction(-4).neg() => 4
     **/
    "neg": function() {
      return newFraction(-this["s"] * this["n"], this["d"]);
    },
    /**
     * Adds two rational numbers
     *
     * Ex: new Fraction({n: 2, d: 3}).add("14.9") => 467 / 30
     **/
    "add": function(a2, b2) {
      parse(a2, b2);
      return newFraction(
        this["s"] * this["n"] * P["d"] + P["s"] * this["d"] * P["n"],
        this["d"] * P["d"]
      );
    },
    /**
     * Subtracts two rational numbers
     *
     * Ex: new Fraction({n: 2, d: 3}).add("14.9") => -427 / 30
     **/
    "sub": function(a2, b2) {
      parse(a2, b2);
      return newFraction(
        this["s"] * this["n"] * P["d"] - P["s"] * this["d"] * P["n"],
        this["d"] * P["d"]
      );
    },
    /**
     * Multiplies two rational numbers
     *
     * Ex: new Fraction("-17.(345)").mul(3) => 5776 / 111
     **/
    "mul": function(a2, b2) {
      parse(a2, b2);
      return newFraction(
        this["s"] * P["s"] * this["n"] * P["n"],
        this["d"] * P["d"]
      );
    },
    /**
     * Divides two rational numbers
     *
     * Ex: new Fraction("-17.(345)").inverse().div(3)
     **/
    "div": function(a2, b2) {
      parse(a2, b2);
      return newFraction(
        this["s"] * P["s"] * this["n"] * P["d"],
        this["d"] * P["n"]
      );
    },
    /**
     * Clones the actual object
     *
     * Ex: new Fraction("-17.(345)").clone()
     **/
    "clone": function() {
      return newFraction(this["s"] * this["n"], this["d"]);
    },
    /**
     * Calculates the modulo of two rational numbers - a more precise fmod
     *
     * Ex: new Fraction('4.(3)').mod([7, 8]) => (13/3) % (7/8) = (5/6)
     * Ex: new Fraction(20, 10).mod().equals(0) ? "is Integer"
     **/
    "mod": function(a2, b2) {
      if (a2 === void 0) {
        return newFraction(this["s"] * this["n"] % this["d"], C_ONE);
      }
      parse(a2, b2);
      if (C_ZERO === P["n"] * this["d"]) {
        throw DivisionByZero();
      }
      return newFraction(
        this["s"] * (P["d"] * this["n"]) % (P["n"] * this["d"]),
        P["d"] * this["d"]
      );
    },
    /**
     * Calculates the fractional gcd of two rational numbers
     *
     * Ex: new Fraction(5,8).gcd(3,7) => 1/56
     */
    "gcd": function(a2, b2) {
      parse(a2, b2);
      return newFraction(gcd(P["n"], this["n"]) * gcd(P["d"], this["d"]), P["d"] * this["d"]);
    },
    /**
     * Calculates the fractional lcm of two rational numbers
     *
     * Ex: new Fraction(5,8).lcm(3,7) => 15
     */
    "lcm": function(a2, b2) {
      parse(a2, b2);
      if (P["n"] === C_ZERO && this["n"] === C_ZERO) {
        return newFraction(C_ZERO, C_ONE);
      }
      return newFraction(P["n"] * this["n"], gcd(P["n"], this["n"]) * gcd(P["d"], this["d"]));
    },
    /**
     * Gets the inverse of the fraction, means numerator and denominator are exchanged
     *
     * Ex: new Fraction([-3, 4]).inverse() => -4 / 3
     **/
    "inverse": function() {
      return newFraction(this["s"] * this["d"], this["n"]);
    },
    /**
     * Calculates the fraction to some integer exponent
     *
     * Ex: new Fraction(-1,2).pow(-3) => -8
     */
    "pow": function(a2, b2) {
      parse(a2, b2);
      if (P["d"] === C_ONE) {
        if (P["s"] < C_ZERO) {
          return newFraction((this["s"] * this["d"]) ** P["n"], this["n"] ** P["n"]);
        } else {
          return newFraction((this["s"] * this["n"]) ** P["n"], this["d"] ** P["n"]);
        }
      }
      if (this["s"] < C_ZERO) return null;
      let N4 = factorize(this["n"]);
      let D2 = factorize(this["d"]);
      let n = C_ONE;
      let d2 = C_ONE;
      for (let k3 in N4) {
        if (k3 === "1") continue;
        if (k3 === "0") {
          n = C_ZERO;
          break;
        }
        N4[k3] *= P["n"];
        if (N4[k3] % P["d"] === C_ZERO) {
          N4[k3] /= P["d"];
        } else return null;
        n *= BigInt(k3) ** N4[k3];
      }
      for (let k3 in D2) {
        if (k3 === "1") continue;
        D2[k3] *= P["n"];
        if (D2[k3] % P["d"] === C_ZERO) {
          D2[k3] /= P["d"];
        } else return null;
        d2 *= BigInt(k3) ** D2[k3];
      }
      if (P["s"] < C_ZERO) {
        return newFraction(d2, n);
      }
      return newFraction(n, d2);
    },
    /**
     * Calculates the logarithm of a fraction to a given rational base
     *
     * Ex: new Fraction(27, 8).log(9, 4) => 3/2
     */
    "log": function(a2, b2) {
      parse(a2, b2);
      if (this["s"] <= C_ZERO || P["s"] <= C_ZERO) return null;
      const allPrimes = /* @__PURE__ */ Object.create(null);
      const baseFactors = factorize(P["n"]);
      const T1 = factorize(P["d"]);
      const numberFactors = factorize(this["n"]);
      const T2 = factorize(this["d"]);
      for (const prime in T1) {
        baseFactors[prime] = (baseFactors[prime] || C_ZERO) - T1[prime];
      }
      for (const prime in T2) {
        numberFactors[prime] = (numberFactors[prime] || C_ZERO) - T2[prime];
      }
      for (const prime in baseFactors) {
        if (prime === "1") continue;
        allPrimes[prime] = true;
      }
      for (const prime in numberFactors) {
        if (prime === "1") continue;
        allPrimes[prime] = true;
      }
      let retN = null;
      let retD = null;
      for (const prime in allPrimes) {
        const baseExponent = baseFactors[prime] || C_ZERO;
        const numberExponent = numberFactors[prime] || C_ZERO;
        if (baseExponent === C_ZERO) {
          if (numberExponent !== C_ZERO) {
            return null;
          }
          continue;
        }
        let curN = numberExponent;
        let curD = baseExponent;
        const gcdValue = gcd(curN, curD);
        curN /= gcdValue;
        curD /= gcdValue;
        if (retN === null && retD === null) {
          retN = curN;
          retD = curD;
        } else if (curN * retD !== retN * curD) {
          return null;
        }
      }
      return retN !== null && retD !== null ? newFraction(retN, retD) : null;
    },
    /**
     * Check if two rational numbers are the same
     *
     * Ex: new Fraction(19.6).equals([98, 5]);
     **/
    "equals": function(a2, b2) {
      parse(a2, b2);
      return this["s"] * this["n"] * P["d"] === P["s"] * P["n"] * this["d"];
    },
    /**
     * Check if this rational number is less than another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    "lt": function(a2, b2) {
      parse(a2, b2);
      return this["s"] * this["n"] * P["d"] < P["s"] * P["n"] * this["d"];
    },
    /**
     * Check if this rational number is less than or equal another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    "lte": function(a2, b2) {
      parse(a2, b2);
      return this["s"] * this["n"] * P["d"] <= P["s"] * P["n"] * this["d"];
    },
    /**
     * Check if this rational number is greater than another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    "gt": function(a2, b2) {
      parse(a2, b2);
      return this["s"] * this["n"] * P["d"] > P["s"] * P["n"] * this["d"];
    },
    /**
     * Check if this rational number is greater than or equal another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    "gte": function(a2, b2) {
      parse(a2, b2);
      return this["s"] * this["n"] * P["d"] >= P["s"] * P["n"] * this["d"];
    },
    /**
     * Compare two rational numbers
     * < 0 iff this < that
     * > 0 iff this > that
     * = 0 iff this = that
     *
     * Ex: new Fraction(19.6).compare([98, 5]);
     **/
    "compare": function(a2, b2) {
      parse(a2, b2);
      let t = this["s"] * this["n"] * P["d"] - P["s"] * P["n"] * this["d"];
      return (C_ZERO < t) - (t < C_ZERO);
    },
    /**
     * Calculates the ceil of a rational number
     *
     * Ex: new Fraction('4.(3)').ceil() => (5 / 1)
     **/
    "ceil": function(places) {
      places = C_TEN ** BigInt(places || 0);
      return newFraction(
        ifloor(this["s"] * places * this["n"] / this["d"]) + (places * this["n"] % this["d"] > C_ZERO && this["s"] >= C_ZERO ? C_ONE : C_ZERO),
        places
      );
    },
    /**
     * Calculates the floor of a rational number
     *
     * Ex: new Fraction('4.(3)').floor() => (4 / 1)
     **/
    "floor": function(places) {
      places = C_TEN ** BigInt(places || 0);
      return newFraction(
        ifloor(this["s"] * places * this["n"] / this["d"]) - (places * this["n"] % this["d"] > C_ZERO && this["s"] < C_ZERO ? C_ONE : C_ZERO),
        places
      );
    },
    /**
     * Rounds a rational numbers
     *
     * Ex: new Fraction('4.(3)').round() => (4 / 1)
     **/
    "round": function(places) {
      places = C_TEN ** BigInt(places || 0);
      return newFraction(
        ifloor(this["s"] * places * this["n"] / this["d"]) + this["s"] * ((this["s"] >= C_ZERO ? C_ONE : C_ZERO) + C_TWO * (places * this["n"] % this["d"]) > this["d"] ? C_ONE : C_ZERO),
        places
      );
    },
    /**
      * Rounds a rational number to a multiple of another rational number
      *
      * Ex: new Fraction('0.9').roundTo("1/8") => 7 / 8
      **/
    "roundTo": function(a2, b2) {
      parse(a2, b2);
      const n = this["n"] * P["d"];
      const d2 = this["d"] * P["n"];
      const r = n % d2;
      let k3 = ifloor(n / d2);
      if (r + r >= d2) {
        k3++;
      }
      return newFraction(this["s"] * k3 * P["n"], P["d"]);
    },
    /**
     * Check if two rational numbers are divisible
     *
     * Ex: new Fraction(19.6).divisible(1.5);
     */
    "divisible": function(a2, b2) {
      parse(a2, b2);
      if (P["n"] === C_ZERO) return false;
      return this["n"] * P["d"] % (P["n"] * this["d"]) === C_ZERO;
    },
    /**
     * Returns a decimal representation of the fraction
     *
     * Ex: new Fraction("100.'91823'").valueOf() => 100.91823918239183
     **/
    "valueOf": function() {
      return Number(this["s"] * this["n"]) / Number(this["d"]);
    },
    /**
     * Creates a string representation of a fraction with all digits
     *
     * Ex: new Fraction("100.'91823'").toString() => "100.(91823)"
     **/
    "toString": function(dec = 15) {
      let N4 = this["n"];
      let D2 = this["d"];
      let cycLen = cycleLen(N4, D2);
      let cycOff = cycleStart(N4, D2, cycLen);
      let str = this["s"] < C_ZERO ? "-" : "";
      str += ifloor(N4 / D2);
      N4 %= D2;
      N4 *= C_TEN;
      if (N4)
        str += ".";
      if (cycLen) {
        for (let i = cycOff; i--; ) {
          str += ifloor(N4 / D2);
          N4 %= D2;
          N4 *= C_TEN;
        }
        str += "(";
        for (let i = cycLen; i--; ) {
          str += ifloor(N4 / D2);
          N4 %= D2;
          N4 *= C_TEN;
        }
        str += ")";
      } else {
        for (let i = dec; N4 && i--; ) {
          str += ifloor(N4 / D2);
          N4 %= D2;
          N4 *= C_TEN;
        }
      }
      return str;
    },
    /**
     * Returns a string-fraction representation of a Fraction object
     *
     * Ex: new Fraction("1.'3'").toFraction() => "4 1/3"
     **/
    "toFraction": function(showMixed = false) {
      let n = this["n"];
      let d2 = this["d"];
      let str = this["s"] < C_ZERO ? "-" : "";
      if (d2 === C_ONE) {
        str += n;
      } else {
        const whole = ifloor(n / d2);
        if (showMixed && whole > C_ZERO) {
          str += whole;
          str += " ";
          n %= d2;
        }
        str += n;
        str += "/";
        str += d2;
      }
      return str;
    },
    /**
     * Returns a latex representation of a Fraction object
     *
     * Ex: new Fraction("1.'3'").toLatex() => "\frac{4}{3}"
     **/
    "toLatex": function(showMixed = false) {
      let n = this["n"];
      let d2 = this["d"];
      let str = this["s"] < C_ZERO ? "-" : "";
      if (d2 === C_ONE) {
        str += n;
      } else {
        const whole = ifloor(n / d2);
        if (showMixed && whole > C_ZERO) {
          str += whole;
          n %= d2;
        }
        str += "\\frac{";
        str += n;
        str += "}{";
        str += d2;
        str += "}";
      }
      return str;
    },
    /**
     * Returns an array of continued fraction elements
     *
     * Ex: new Fraction("7/8").toContinued() => [0,1,7]
     */
    "toContinued": function() {
      let a2 = this["n"];
      let b2 = this["d"];
      const res = [];
      while (b2) {
        res.push(ifloor(a2 / b2));
        const t = a2 % b2;
        a2 = b2;
        b2 = t;
      }
      return res;
    },
    "simplify": function(eps = 1e-3) {
      const ieps = BigInt(Math.ceil(1 / eps));
      const thisABS = this["abs"]();
      const cont = thisABS["toContinued"]();
      for (let i = 1; i < cont.length; i++) {
        let s2 = newFraction(cont[i - 1], C_ONE);
        for (let k3 = i - 2; k3 >= 0; k3--) {
          s2 = s2["inverse"]()["add"](cont[k3]);
        }
        let t = s2["sub"](thisABS);
        if (t["n"] * ieps < t["d"]) {
          return s2["mul"](this["s"]);
        }
      }
      return this;
    }
  };

  // node_modules/@kabelsalat/web/dist/index.mjs
  function W(e, t) {
    if (t || (t = "assertion failed"), !e)
      throw new Error(t);
  }
  var c = class {
    constructor(t, i) {
      this.type = t, i !== void 0 && (this.value = i), this.ins = [];
    }
    static parseInput(t, i) {
      if (typeof t == "function") {
        if (!i)
          throw new Error(
            "tried to parse function input without without passing node.."
          );
        return t(i);
      }
      return typeof t == "object" ? t : typeof t == "number" && !isNaN(t) || typeof t == "string" ? f(t) : (console.log(
        `invalid input type "${typeof t}" for node of type "${i.type}", falling back to 0. The input was:`,
        t
      ), 0);
    }
  };
  var x = (e, t) => new c(e, t);
  var y = /* @__PURE__ */ new Map();
  var Z = "poly";
  var Y = "exit";
  c.prototype.inherit = function(e) {
    return e.inputOf && (this.inputOf = e.inputOf), e.outputOf && (this.outputOf = e.outputOf), this;
  };
  c.prototype.toObject = function() {
    return JSON.parse(JSON.stringify(this));
  };
  c.prototype.stringify = function() {
    return JSON.stringify(this, null, 2).replaceAll('"', "'");
  };
  function F(e, ...t) {
    let i = 1;
    if (t = t.map((l2) => {
      if (Array.isArray(l2)) {
        if (l2.length === 1)
          return l2[0];
        l2 = new c(Z).withIns(...l2);
      }
      if (typeof l2 == "function") {
        const o = l2(new c("peek"));
        o.type === Z && (i = Math.max(o.ins.length, i));
      }
      return l2.type === Z && (i = Math.max(l2.ins.length, i)), l2;
    }), i === 1) {
      const l2 = x(e);
      return l2.withIns(...t.map((o) => c.parseInput(o, l2)));
    }
    if (e === Y) {
      const l2 = t.map((o) => o.type === Z ? o.ins.map((d2) => c.parseInput(d2).inherit(d2)) : o).flat();
      return x(Y).withIns(...l2);
    }
    const n = Array.from({ length: i }, (l2, o) => {
      const d2 = new c(e), G2 = t.map((p) => p.type === Z ? c.parseInput(p.ins[o % p.ins.length], d2).inherit(p) : (p = c.parseInput(p, d2), p.type === Z && (p = p.ins[o]), p));
      return d2.withIns(...G2);
    });
    return new c(Z).withIns(...n);
  }
  function oe(e, t) {
    const i = y.get(e);
    return i?.ins?.[t] ? i.ins[t].name : "";
  }
  var a = (e, t) => u(e, (...i) => F(e, ...i), t);
  y.set("register", {
    tags: ["meta"],
    graph: false,
    description: "Registers a new Node function. Sets it on the prototype + returns the function itself. Like `module` but doesn't hide complexity in graph viz.",
    examples: [
      `let kick = register('kick', gate => gate.adsr(0,.11,0,.11)
.apply(env => env.mul(env)
  .mul(158)
  .sine(env)
  .distort(.85)
))
impulse(2).kick().out()`
    ]
  });
  var u = (e, t, i) => (i && y.set(e, i), c.prototype[e] = function(...n) {
    return t(this, ...n);
  }, c.prototype["_" + e] = function() {
    return this;
  }, t);
  y.set("module", {
    tags: ["meta"],
    graph: true,
    description: "Creates a module. Like `register`, but the graph viz will hide the internal complexity of the module.",
    examples: [
      `let kick = module('kick', gate => gate.adsr(0,.11,0,.11)
.apply(env => env.mul(env)
  .mul(158)
  .sine(env)
  .distort(.85)
))
impulse(2).kick().out()`
    ]
  });
  var de = 0;
  function g(e, t, i) {
    return u(
      e,
      (...n) => {
        const l2 = de++;
        return n = n.map(
          (o, d2) => c.parseInput(o).asModuleInput?.(e, l2, d2)
        ), t(...n).asModuleOutput?.(e, l2);
      },
      i
    );
  }
  y.set("n", {
    tags: ["math"],
    description: "Constant value node. Turns a number into a Node.",
    ins: [{ name: "value", default: 0 }]
  });
  function f(e) {
    return Array.isArray(e) ? poly(...e.map((t) => f(t))) : typeof e == "object" ? e : x("n", e);
  }
  y.set("out", {
    tags: ["meta"],
    description: "Sends the node to the audio output"
  });
  y.set("withIns", {
    internal: true,
    tags: ["innards"],
    description: "Sets the inputs of a node. Returns the node itself",
    ins: [{ name: "in", dynamic: true }]
  });
  c.prototype.withIns = function(...e) {
    return this.ins = e, this;
  };
  y.set("flatten", {
    internal: true,
    tags: ["innards"],
    description: "Flattens the node to a list of all nodes in the graph, where each Node's ins are now indices"
  });
  c.prototype.flatten = function() {
    return pe(this);
  };
  y.set("apply", {
    graph: true,
    tags: ["meta"],
    description: "Applies the given function to the Node. Useful when a node has to be used multiple times.",
    examples: [
      `impulse(4)
.apply(imp=>imp
  .seq(110,220,330,440)
  .sine()
  .mul( imp.ad(.1,.1) )
).out()`
    ]
  });
  c.prototype.apply = function(e) {
    return e(this);
  };
  y.set("clone", {
    internal: true,
    tags: ["innards"],
    description: "Clones the node"
  });
  c.prototype.clone = function() {
    return new c(this.type, this.value).withIns(...this.ins);
  };
  y.set("map", {
    tags: ["meta"],
    description: "Applies the given function to all ins if it's poly node. Otherwise it applies the function to itself.",
    examples: [
      `n([110,220,330])
.map( freq=>freq.mul([1,1.007]).saw().mix() )
.mix(2).mul(.5).out()`
    ]
  });
  c.prototype.map = function(e) {
    return this.type !== "poly" ? e(this) : poly(...this.ins.map(e));
  };
  c.prototype.channel = function(e) {
    return this.type !== "poly" ? this : this.ins[e % this.ins.length];
  };
  y.set("select", {
    tags: ["meta"],
    graph: true,
    description: "Find the first occurence of the given type up in the graph and returns the match. Useful to exit a feedback loop at another point.",
    examples: [
      `sine(220).mul(impulse(1).ad(.001,.2))
.add( x=>x.delay(.2).mul(.8) )
.select('delay').out()
`
    ]
  });
  c.prototype.select = function(e) {
    for (let t of this.ins) {
      if (t.type === e)
        return t;
      const i = t.select(e);
      if (i)
        return i;
    }
  };
  y.set("debug", {
    tags: ["meta"],
    description: "Logs the node to the console"
  });
  c.prototype.debug = function(e = (t) => t) {
    return console.log(e(this)), this;
  };
  function ce(e) {
    const t = modules.get(e), i = Array.from(
      { length: t.length },
      (l2, o) => x(`$INPUT${o}`)
    ), n = t(...i);
    return JSON.stringify(n, null, 2);
  }
  function pe(e) {
    const t = [];
    return S(e, (i) => (t.push(i), i)), t.map((i) => {
      let n = {
        ...i,
        type: i.type,
        ins: i.ins.map((l2) => t.indexOf(l2) + "")
      };
      return i.value !== void 0 && (n.value = i.value), i.to !== void 0 && (n.to = t.indexOf(i.to)), n;
    });
  }
  var J = a("exit", { internal: true });
  function j(e, t) {
    let i = [];
    const n = u("out", function(o, d2 = [0, 1]) {
      return i.push(o.output(d2)), o;
    });
    return t ? (t.out = n, Function(...Object.keys(t), e)(...Object.values(t))) : (globalThis.out = n, Function(e)()), J(...i);
  }
  c.prototype.over = function(e) {
    return this.apply((t) => add(t, e(t)));
  };
  c.prototype.dfs = function(e, t) {
    return this.apply((i) => S(i, e, t));
  };
  c.prototype.apply2 = function(e) {
    return e(this, this);
  };
  var S = (e, t, i = []) => (e = t(e, i), i.push(e), e.ins = e.ins.map((n) => i.includes(n) ? n : S(n, t, i)), e);
  c.prototype.asModuleInput = function(e, t, i) {
    return this.inputOf = this.inputOf || [], this.inputOf.push([e, t, i]), this;
  };
  c.prototype.asModuleOutput = function(e, t) {
    return this.outputOf = [e, t], this;
  };
  function T(e, t = {}) {
    const {
      log: i = false,
      lang: n = "js",
      fallbackType: l2 = "thru",
      constType: o = "n",
      getRegister: d2 = (h) => `r[${h}]`,
      getOutput: G2 = (h) => `o[${h}]`,
      getSource: p = (h) => `s[${h}]`
    } = t;
    i && console.log("compile", e);
    const m3 = ue(e);
    let r = [], R2 = (h) => m3[h].type !== o ? d2(h) : typeof m3[h].value == "string" ? `"${m3[h].value}"` : m3[h].value;
    const X = [];
    for (let h in m3) {
      const v = m3[h], I2 = m3[h].ins.map((ae2) => R2(m3.indexOf(ae2))), se2 = X.length;
      let b2 = y.get(v.type);
      b2 || (console.warn(
        `unhandled node type "${m3[h].type}". falling back to "${l2}"`
      ), b2 = y.get(l2));
      const le2 = {
        vars: I2,
        node: v,
        nodes: m3,
        id: h,
        ugenIndex: se2,
        ugen: b2.ugen,
        name: R2(h),
        lang: n,
        getRegister: d2,
        getOutput: G2,
        getSource: p
      };
      b2.compile && r.push(b2.compile(le2)), b2.ugen && X.push({ type: b2.ugen, inputs: I2 });
    }
    const z3 = r.join(`
`);
    return i && (console.log("compiled code:"), console.log(z3)), { src: z3, ugens: X, registers: m3.length };
  }
  c.prototype.compile = function(e) {
    return T(this, e);
  };
  function ue(e) {
    const t = [], i = /* @__PURE__ */ new Set();
    function n(l2) {
      if (!(typeof l2 != "object" || i.has(l2))) {
        i.add(l2);
        for (let o in l2.ins)
          n(l2.ins[o]);
        t.push(l2);
      }
    }
    return n(e), t;
  }
  var U = /* @__PURE__ */ Object.freeze(/* @__PURE__ */ Object.defineProperty({
    __proto__: null,
    compile: T
  }, Symbol.toStringTag, { value: "Module" }));
  var Q = class {
    constructor() {
      this._events = {};
    }
    on(t, i) {
      this._events[t] || (this._events[t] = []);
      let n = this._events[t];
      W(n.indexOf(i) == -1), n.push(i);
    }
    removeListener(t, i) {
      let n = this._events[t], l2 = n.indexOf(i);
      l2 != -1 && n.splice(l2, 1);
    }
    trigger(t, ...i) {
      let n = this._events[t] || [];
      for (let l2 = 0; l2 < n.length; l2++)
        n[l2].apply(null, i);
    }
  };
  var me = class extends Q {
    constructor(t = navigator) {
      super(), this.midiAccess = null, this.getMIDIAccess(t);
    }
    // Try to get MIDI access from the browser
    async getMIDIAccess(t) {
      if ("requestMIDIAccess" in t) {
        this.midiAccess = await t.requestMIDIAccess({ sysex: false }), console.log("got MIDI access");
        for (let i of this.midiAccess.inputs.values())
          i.state == "connected" && (i.onmidimessage = (n) => this.trigger("midimessage", i.id, n.data));
        this.midiAccess.onstatechange = (i) => {
          i.port.type == "input" && i.port.state == "connected" && (console.log(
            "MIDI device connected:",
            i.port.name,
            "PORT:",
            i.port.id
          ), i.port.onmidimessage = (n) => this.trigger("midimessage", i.port.id, n.data));
        };
      }
    }
    // Send a message to all MIDI devices
    broadcast(t, i) {
      if (midi)
        for (let n of this.midiAccess.outputs.values())
          n.send(t, i);
    }
  };
  function re(e) {
    let t = e[0] & 240, i = (e[0] & 15) + 1;
    if (t == 176 && e.length == 3) {
      let n = e[1], o = e[2] / 127 * 2 - 1;
      return { type: "CC", channel: i, cc: n, value: o };
    }
    if (t == 224 && e.length == 3) {
      let n = e[1], d2 = (e[2] << 7 | n) / 16383 * 2 - 1;
      return { type: "PITCHBEND", channel: i, value: d2 };
    }
    if (t == 144 && e.length == 3) {
      let n = e[1], l2 = e[2] / 127;
      return { type: "NOTE_ON", channel: i, note: n, velocity: l2 };
    }
    if (t == 128 && e.length == 3) {
      let n = e[1];
      return { type: "NOTE_ON", channel: i, note: n, velocity: 0 };
    }
  }
  var he = class extends Q {
    constructor() {
      super(), this.attach();
    }
    attach() {
      typeof window < "u" && (this.handleMouseMove = (t) => {
        const i = t.clientX / document.body.clientWidth * 2 - 1, n = t.clientY / document.body.clientHeight * 2 - 1;
        this.trigger("move", i, n);
      }, document.addEventListener("mousemove", this.handleMouseMove));
    }
    detach() {
      typeof window < "u" && document.removeEventListener("mousemove", this.handleMouseMove);
    }
  };
  var Ge = "data:text/javascript;base64,KGZ1bmN0aW9uKCl7InVzZSBzdHJpY3QiO2Z1bmN0aW9uIGcoaCx0KXtpZih0fHwodD0iYXNzZXJ0aW9uIGZhaWxlZCIpLCFoKXRocm93IG5ldyBFcnJvcih0KX1mdW5jdGlvbiBTKGgsdCxzKXtyZXR1cm4gaDw9MD90Omg+PTE/czp0K2gqKHMtdCl9ZnVuY3Rpb24gUChoLHQscyl7cmV0dXJuIGg8PXQ/MDpoPj1zPzE6cz09PXQ/MDooaC10KS8ocy10KX1mdW5jdGlvbiBFKGgsdCl7cmV0dXJuIGg8dD8oaC89dCxoK2gtaCpoLTEpOmg+MS10PyhoPShoLTEpL3QsaCpoK2graCsxKTowfWZ1bmN0aW9uIFUoaCl7cmV0dXJuIE1hdGguZmxvb3IoaCk9PT1ofWZ1bmN0aW9uIF8oaCl7cmV0dXJuIFUoaCkmJmg+MH1mdW5jdGlvbiBrKGgsdCl7dD1NYXRoLm1pbihNYXRoLm1heCh0LDApLDEpLHQtPS4wMTt2YXIgcz0yKnQvKDEtdCksZT0oMStzKSpoLygxK3MqTWF0aC5hYnMoaCkpO3JldHVybiBlfWZ1bmN0aW9uIHcoaCx0LHMpe3JldHVybiBoPj0xP3M6dCtoKihzLXQpfWZ1bmN0aW9uIFQoKXt0aGlzLnN0YXRlPSJvZmYiLHRoaXMuc3RhcnRUaW1lPTAsdGhpcy5zdGFydFZhbD0wfVQucHJvdG90eXBlLmV2YWw9ZnVuY3Rpb24oaCx0LHMsZSxpLG4pe3N3aXRjaCh0aGlzLnN0YXRlKXtjYXNlIm9mZiI6cmV0dXJuIHQ+MCYmKHRoaXMuc3RhdGU9ImF0dGFjayIsdGhpcy5zdGFydFRpbWU9aCx0aGlzLnN0YXJ0VmFsPTApLDA7Y2FzZSJhdHRhY2siOntsZXQgcj1oLXRoaXMuc3RhcnRUaW1lO3JldHVybiByPnM/KHRoaXMuc3RhdGU9ImRlY2F5Iix0aGlzLnN0YXJ0VGltZT1oLDEpOncoci9zLHRoaXMuc3RhcnRWYWwsMSl9Y2FzZSJkZWNheSI6e2xldCByPWgtdGhpcy5zdGFydFRpbWUsbD13KHIvZSwxLGkpO3JldHVybiB0PD0wPyh0aGlzLnN0YXRlPSJyZWxlYXNlIix0aGlzLnN0YXJ0VGltZT1oLHRoaXMuc3RhcnRWYWw9bCxsKTpyPmU/KHRoaXMuc3RhdGU9InN1c3RhaW4iLHRoaXMuc3RhcnRUaW1lPWgsaSk6bH1jYXNlInN1c3RhaW4iOnJldHVybiB0PD0wJiYodGhpcy5zdGF0ZT0icmVsZWFzZSIsdGhpcy5zdGFydFRpbWU9aCx0aGlzLnN0YXJ0VmFsPWkpLGk7Y2FzZSJyZWxlYXNlIjp7bGV0IHI9aC10aGlzLnN0YXJ0VGltZTtpZihyPm4pcmV0dXJuIHRoaXMuc3RhdGU9Im9mZiIsMDtsZXQgbD13KHIvbix0aGlzLnN0YXJ0VmFsLDApO3JldHVybiB0PjAmJih0aGlzLnN0YXRlPSJhdHRhY2siLHRoaXMuc3RhcnRUaW1lPWgsdGhpcy5zdGFydFZhbD1sKSxsfX10aHJvdyJpbnZhbGlkIGVudmVsb3BlIHN0YXRlIn07ZnVuY3Rpb24gdigpe3RoaXMuczA9MCx0aGlzLnMxPTB9di5wcm90b3R5cGUuYXBwbHk9ZnVuY3Rpb24oaCx0LHMpe2coIWlzTmFOKGgpLCJOYU4gdmFsdWUgZmVkIGluIFR3b1BvbGVGaWx0ZXIiKSx0PU1hdGgubWluKHQsMSkscz1NYXRoLm1heChzLDApO3ZhciBlPU1hdGgucG93KC41LCgxLXQpLy4xMjUpLGk9TWF0aC5wb3coLjUsKHMrLjEyNSkvLjEyNSksbj0xLWkqZSxyPXRoaXMuczAsbD10aGlzLnMxO3JldHVybiByPW4qci1lKmwrZSpoLGw9bipsK2UqcixoPWwsdGhpcy5zMD1yLHRoaXMuczE9bCxofTtsZXQgQT1jbGFzcyBDe2NvbnN0cnVjdG9yKHQscyl7dGhpcy5zYW1wbGVSYXRlPXQscz90aGlzLmJ1ZmZlcj1zLnNsaWNlKDApOih0aGlzLmJ1ZmZlcj1uZXcgRmxvYXQzMkFycmF5KDEwKnQpLHRoaXMuYnVmZmVyLmZpbGwoMCkpLHRoaXMud3JpdGVJZHg9MCx0aGlzLnJlYWRJZHg9MH1yZXNldCgpe3RoaXMuYnVmZmVyLmZpbGwoMCksdGhpcy53cml0ZUlkeD0wLHRoaXMucmVhZElkeD0wfWNsb25lKCl7Y29uc3QgdD1uZXcgQyh0aGlzLnNhbXBsZVJhdGUsdGhpcy5idWZmZXIpO3JldHVybiB0LndyaXRlSWR4PXRoaXMud3JpdGVJZHgsdC5yZWFkSWR4PXRoaXMucmVhZElkeCx0fXdyaXRlKHQscyl7dGhpcy53cml0ZUlkeD0odGhpcy53cml0ZUlkeCsxKSV0aGlzLmJ1ZmZlci5sZW5ndGgsdGhpcy5idWZmZXJbdGhpcy53cml0ZUlkeF09dDtsZXQgZT1NYXRoLm1pbihNYXRoLmZsb29yKHRoaXMuc2FtcGxlUmF0ZSpzKSx0aGlzLmJ1ZmZlci5sZW5ndGgtMSk7dGhpcy5yZWFkSWR4PXRoaXMud3JpdGVJZHgtZSx0aGlzLnJlYWRJZHg8MCYmKHRoaXMucmVhZElkeCs9dGhpcy5idWZmZXIubGVuZ3RoKX1yZWFkKCl7cmV0dXJuIHRoaXMuYnVmZmVyW3RoaXMucmVhZElkeF19fTtjb25zdCB5PTEvNDhlMyxNPTI0LE49TS80O2NsYXNzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7dGhpcy5ub2RlSWQ9dCx0aGlzLnN0YXRlPXMsdGhpcy5zYW1wbGVSYXRlPWUsdGhpcy5zYW1wbGVUaW1lPTEvZSx0aGlzLnNlbmQ9aX19Y2xhc3MgcSBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSksdGhpcy5lbnY9bmV3IFR9dXBkYXRlKHQscyxlLGksbixyKXtyZXR1cm4gdGhpcy5lbnYuZXZhbCh0LHMsZSxpLG4scil9fWNsYXNzIEYgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMucGhhc2U9MH11cGRhdGUodCl7bGV0IHM9TSp0LzYwLGU9LjU7cmV0dXJuIHRoaXMucGhhc2UrPXRoaXMuc2FtcGxlVGltZSpzLHRoaXMucGhhc2UlMTxlPzE6LTF9fWNsYXNzIEQgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMuaW5TZ249ITAsdGhpcy5vdXRTZ249ITAsdGhpcy5jbG9ja0NudD0wfXVwZGF0ZSh0LHMpe2xldCBlPXQ+MDtyZXR1cm4gdGhpcy5pblNnbiE9ZSYmKHRoaXMuY2xvY2tDbnQrKyx0aGlzLmNsb2NrQ250Pj1zJiYodGhpcy5jbG9ja0NudD0wLHRoaXMub3V0U2duPSF0aGlzLm91dFNnbikpLHRoaXMuaW5TZ249ZSx0aGlzLm91dFNnbj8xOi0xfX1jbGFzcyBMIGV4dGVuZHMgb3tjb25zdHJ1Y3Rvcih0LHMsZSxpKXtzdXBlcih0LHMsZSxpKSx0aGlzLmluU2duPSExfXVwZGF0ZSh0LHMpe2xldCBlPXM+MDtyZXR1cm4gZSYmdGhpcy5pblNnbiE9ZSYmdGhpcy5zZW5kKHt0eXBlOiJDTE9DS19QVUxTRSIsbm9kZUlkOnRoaXMubm9kZUlkLHRpbWU6dH0pLHRoaXMuaW5TZ249ZSwwfX1jb25zdCBPPW5ldyBNYXA7Y2xhc3MgViBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSk7Y29uc3Qgbj1zLmlucHV0c1syXTtuJiZPLmhhcyhuKT90aGlzLmRlbGF5PU8uZ2V0KG4pLmNsb25lKCk6dGhpcy5kZWxheT1uZXcgQShlKSxuJiZPLnNldChuLHRoaXMuZGVsYXkpfXVwZGF0ZSh0LHMpe3JldHVybiB0aGlzLmRlbGF5LndyaXRlKHQscyksdGhpcy5kZWxheS5yZWFkKCl9fWNsYXNzIEcgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpfXVwZGF0ZSh0LHMpe3JldHVybiBrKHQscyl9fWNsYXNzIFIgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMudmFsdWU9MCx0aGlzLnRyaWdTZ249ITF9d3JpdGUodCxzKXshdGhpcy50cmlnU2duJiZzPjAmJih0aGlzLnZhbHVlPXQpLHRoaXMudHJpZ1Nnbj1zPjB9cmVhZCgpe3JldHVybiB0aGlzLnZhbHVlfXVwZGF0ZSh0LHMpe3JldHVybiB0aGlzLndyaXRlKHQscyksdGhpcy5yZWFkKCl9fWNsYXNzIEJ7Y29uc3RydWN0b3IoKXt0aGlzLnZhbHVlPTB9dXBkYXRlKHQpe3JldHVybiB0aGlzLnZhbHVlPXQsdGhpcy52YWx1ZX19Y29uc3QgJD0zNDA7bGV0IEk9MDtjbGFzcyBIe2NvbnN0cnVjdG9yKCl7dGhpcy5jaD1JLHRoaXMuc3RhcnRfc2VlZD0kKih0aGlzLmNoKzEpPj4+MCx0aGlzLnN0YXRlPXRoaXMuc3RhcnRfc2VlZCx0aGlzLnZhbHVlPTAsdGhpcy5hPTE2NjQ1MjUsdGhpcy5jPTEwMTM5MDQyMjMsdGhpcy5tYXNrPTE2Nzc3MjE1LHRoaXMuc2NhbGU9MS8oMTw8MjQpLEkrK311cGRhdGUodCxzKXtpZighdClyZXR1cm4gdGhpcy52YWx1ZTtzJiYodGhpcy5zdGF0ZT10aGlzLnN0YXJ0X3NlZWQpLHRoaXMuc3RhdGU9dGhpcy5zdGF0ZSp0aGlzLmErdGhpcy5jPj4+MDtjb25zdCBlPSh0aGlzLnN0YXRlJnRoaXMubWFzaykqdGhpcy5zY2FsZTtyZXR1cm4gdGhpcy52YWx1ZT1lKjItMSx0aGlzLnZhbHVlfX1jbGFzcyBqe2NvbnN0cnVjdG9yKCl7dGhpcy52YWx1ZT1NYXRoLnJhbmRvbSgpKjItMX11cGRhdGUodCl7cmV0dXJuIHQ/KHRoaXMudmFsdWU9TWF0aC5yYW5kb20oKSoyLTEsdGhpcy52YWx1ZSk6dGhpcy52YWx1ZX19Y2xhc3MgS3t1cGRhdGUodCl7cmV0dXJuIE1hdGgucmFuZG9tKCk8dCp5P01hdGgucmFuZG9tKCk6MH19Y2xhc3MgV3tjb25zdHJ1Y3Rvcigpe3RoaXMub3V0PTB9dXBkYXRlKCl7bGV0IHQ9TWF0aC5yYW5kb20oKSoyLTE7cmV0dXJuIHRoaXMub3V0PSh0aGlzLm91dCsuMDIqdCkvMS4wMix0aGlzLm91dH19Y2xhc3MgWHtjb25zdHJ1Y3Rvcigpe3RoaXMuYjA9MCx0aGlzLmIxPTAsdGhpcy5iMj0wLHRoaXMuYjM9MCx0aGlzLmI0PTAsdGhpcy5iNT0wLHRoaXMuYjY9MH11cGRhdGUoKXtjb25zdCB0PU1hdGgucmFuZG9tKCkqMi0xO3RoaXMuYjA9Ljk5ODg2KnRoaXMuYjArdCouMDU1NTE3OSx0aGlzLmIxPS45OTMzMip0aGlzLmIxK3QqLjA3NTA3NTksdGhpcy5iMj0uOTY5KnRoaXMuYjIrdCouMTUzODUyLHRoaXMuYjM9Ljg2NjUqdGhpcy5iMyt0Ki4zMTA0ODU2LHRoaXMuYjQ9LjU1KnRoaXMuYjQrdCouNTMyOTUyMix0aGlzLmI1PS0uNzYxNip0aGlzLmI1LXQqLjAxNjg5ODtjb25zdCBzPXRoaXMuYjArdGhpcy5iMSt0aGlzLmIyK3RoaXMuYjMrdGhpcy5iNCt0aGlzLmI1K3RoaXMuYjYrdCouNTM2MjtyZXR1cm4gdGhpcy5iNj10Ki4xMTU5MjYscyouMTF9fWNsYXNzIFkgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMucGhhc2U9MX11cGRhdGUodCl7dGhpcy5waGFzZSs9dGhpcy5zYW1wbGVUaW1lKnQ7bGV0IHM9dGhpcy5waGFzZT49MT8xOjA7cmV0dXJuIHRoaXMucGhhc2U9dGhpcy5waGFzZSUxLHN9fWNsYXNzIHogZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMucGhhc2U9MH11cGRhdGUodCxzKXtyZXR1cm4gdGhpcy5waGFzZSs9dGhpcy5zYW1wbGVUaW1lKnQsdGhpcy5waGFzZSUxPHM/MTotMX19Y2xhc3MgWiBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSksdGhpcy5waGFzZT0wfXVwZGF0ZSh0KXtyZXR1cm4gdGhpcy5waGFzZSs9dGhpcy5zYW1wbGVUaW1lKnQsdGhpcy5waGFzZSUxKjItMX19Y2xhc3MgSntjb25zdHJ1Y3Rvcigpe3RoaXMucGhhc2U9TWF0aC5yYW5kb20oKX11cGRhdGUodCl7Y29uc3Qgcz10L3NhbXBsZVJhdGU7bGV0IGU9RSh0aGlzLnBoYXNlLHMpLGk9Mip0aGlzLnBoYXNlLTEtZTtyZXR1cm4gdGhpcy5waGFzZSs9cyx0aGlzLnBoYXNlPjEmJih0aGlzLnBoYXNlLT0xKSxpfX1jbGFzcyBRIGV4dGVuZHMgb3tjb25zdHJ1Y3Rvcih0LHMsZSxpKXtzdXBlcih0LHMsZSxpKSx0aGlzLnBoYXNlPTAsdGhpcy5zeW5jU2duPSExfXVwZGF0ZSh0LHMsZSl7IXRoaXMuc3luY1NnbiYmcz4wJiYodGhpcy5waGFzZT0wKSx0aGlzLnN5bmNTZ249cz4wO2xldCBpPSh0aGlzLnBoYXNlK2UpJTE7cmV0dXJuIHRoaXMucGhhc2UrPXRoaXMuc2FtcGxlVGltZSp0LE1hdGguc2luKGkqMipNYXRoLlBJKX19Y2xhc3MgdHR7ZEJUb0xpbmVhcih0KXtyZXR1cm4gTWF0aC5wb3coMTAsdC8yMCl9bGluZWFyVG9EQih0KXtyZXR1cm4gMjAqTWF0aC5sb2cxMCh0KX11cGRhdGUodCxzLGUpe2xldCBpPXRoaXMubGluZWFyVG9EQihNYXRoLmFicyh0KSksbj0wO3JldHVybiBpPnMmJihuPShpLXMpKigxLTEvZSkpLHRoaXMuZEJUb0xpbmVhcigtbil9fWNsYXNzIHN0IGV4dGVuZHMgb3tjb25zdHJ1Y3Rvcih0LHMsZSxpKXtzdXBlcih0LHMsZSxpKSx0aGlzLnBoYXNlPTB9dXBkYXRlKHQpe3RoaXMucGhhc2UrPXRoaXMuc2FtcGxlVGltZSp0O2xldCBzPXRoaXMucGhhc2UlMTtyZXR1cm4oczwuNT8yKnM6MS0yKihzLS41KSkqMi0xfX1jbGFzcyBldCBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSk7Y29uc3Qgbj1lLzMwO2coXyhuKSksdGhpcy5idWZmZXI9bmV3IEZsb2F0MzJBcnJheShuKSx0aGlzLndyaXRlUG9zPTB9dXBkYXRlKHQscyxlLGkpe3JldHVybiB0aGlzLmJ1ZmZlclt0aGlzLndyaXRlUG9zXT10LHRoaXMud3JpdGVQb3MrKyx0aGlzLndyaXRlUG9zJXRoaXMuYnVmZmVyLmxlbmd0aD09MCYmKHRoaXMud3JpdGVQb3M9MCx0aGlzLnNlbmQoe3R5cGU6IlNFTkRfU0FNUExFUyIsaWQ6cyxzYW1wbGVzOnRoaXMuYnVmZmVyLGNoYW5uZWxzOmUsY2hhbm5lbDppfSkpLHR9fWNsYXNzIGl0e2NvbnN0cnVjdG9yKCl7dGhpcy5sYWdVbml0PTQ0MTAsdGhpcy5zPTB9dXBkYXRlKHQscyl7cmV0dXJuIHM9cyp0aGlzLmxhZ1VuaXQsczwxJiYocz0xKSx0aGlzLnMrPTEvcyoodC10aGlzLnMpLHRoaXMuc319Y2xhc3MgaHR7Y29uc3RydWN0b3IoKXt0aGlzLmxhc3Q9MH11cGRhdGUodCxzLGUpe2NvbnN0IGk9cyp5LG49ZSp5O2xldCByPXQtdGhpcy5sYXN0O3JldHVybiByPmk/cj1pOnI8LW4mJihyPS1uKSx0aGlzLmxhc3QrPXIsdGhpcy5sYXN0fX1jbGFzcyBhdCBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSksdGhpcy5zPTB9dXBkYXRlKHQscyl7cmV0dXJuIHM9cyoxZTMsczwxJiYocz0xKSx0aGlzLnMrPTEvcyoodC10aGlzLnMpLHRoaXMuc319Y2xhc3MgbnQgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMuZmlsdGVyPW5ldyB2fXVwZGF0ZSh0LHMsZSl7cmV0dXJuIHRoaXMuZmlsdGVyLmFwcGx5KHQscyxlKSx0aGlzLmZpbHRlci5zMX19Y2xhc3MgcnQgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMuZmlsdGVyPW5ldyB2fXVwZGF0ZSh0LHMsZSl7cmV0dXJuIHRoaXMuZmlsdGVyLmFwcGx5KHQscyxlKSx0aGlzLmZpbHRlci5zMH19Y2xhc3MgdXQgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpfXVwZGF0ZSh0LHMpe3JldHVybiBzPDAmJihzPTApLHM9cysxLHQ9dCpzLDQqKE1hdGguYWJzKC4yNSp0Ky4yNS1NYXRoLnJvdW5kKC4yNSp0Ky4yNSkpLS4yNSl9fWNsYXNzIGx0IGV4dGVuZHMgb3t1cGRhdGUodCl7cmV0dXJuIHR9fWNsYXNzIG0gZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMubm90ZT0wLHRoaXMuZnJlcT0wLHRoaXMudmVsb2NpdHk9MCx0aGlzLmdhdGVTdGF0ZT0ib2ZmIix0aGlzLnR5cGU9Im1pZGlpbiIsdGhpcy5jaGFubmVsPS0xfWlzRnJlZSgpe3JldHVybiB0aGlzLmdhdGVTdGF0ZT09PSJvZmYifW5vdGVPbih0LHMpe3M+MD8odGhpcy5ub3RlPXQsdGhpcy52ZWxvY2l0eT1zLHRoaXMuZnJlcT0yKiooKHQtNjkpLzEyKSo0NDAsdGhpcy5nYXRlU3RhdGU9InByZXRyaWciKTp0aGlzLm5vdGVPZmYoKX1ub3RlT2ZmKCl7dGhpcy5ub3RlPTAsdGhpcy5nYXRlU3RhdGU9Im9mZiJ9Z2V0R2F0ZSgpe3N3aXRjaCh0aGlzLmdhdGVTdGF0ZSl7Y2FzZSJwcmV0cmlnIjpyZXR1cm4gdGhpcy5nYXRlU3RhdGU9Im9uIiwwO2Nhc2Uib24iOnJldHVybiAxO2Nhc2Uib2ZmIjpyZXR1cm4gMDtkZWZhdWx0OmcoITEpfX1nZXRGcmVxKCl7c3dpdGNoKHRoaXMuZ2F0ZVN0YXRlKXtjYXNlInByZXRyaWciOnJldHVybiB0aGlzLmdhdGVTdGF0ZT0ib24iLDA7Y2FzZSJvbiI6cmV0dXJuIHRoaXMuZnJlcTtjYXNlIm9mZiI6cmV0dXJuIHRoaXMuZnJlcTtkZWZhdWx0OmcoITEpfX1nZXRWZWxvY2l0eSgpe3N3aXRjaCh0aGlzLmdhdGVTdGF0ZSl7Y2FzZSJwcmV0cmlnIjpyZXR1cm4gdGhpcy5nYXRlU3RhdGU9Im9uIiwwO2Nhc2Uib24iOnJldHVybiB0aGlzLnZlbG9jaXR5O2Nhc2Uib2ZmIjpyZXR1cm4gdGhpcy52ZWxvY2l0eTtkZWZhdWx0OmcoITEpfX19Y2xhc3Mgb3QgZXh0ZW5kcyBte2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMudHlwZT0ibWlkaWdhdGUifXVwZGF0ZSh0KXtyZXR1cm4gdGhpcy5jaGFubmVsPXQsdGhpcy5nZXRHYXRlKCl9fWNsYXNzIGN0IGV4dGVuZHMgbXtjb25zdHJ1Y3Rvcih0LHMsZSxpKXtzdXBlcih0LHMsZSxpKSx0aGlzLnR5cGU9Im1pZGlmcmVxIn11cGRhdGUodCl7cmV0dXJuIHRoaXMuY2hhbm5lbD10LHRoaXMuZ2V0RnJlcSgpfX1jbGFzcyBkdCBleHRlbmRzIG17Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSksdGhpcy50eXBlPSJtaWRpdmVsIn11cGRhdGUodCl7cmV0dXJuIHRoaXMuY2hhbm5lbD10LHRoaXMuZ2V0VmVsb2NpdHkoKX19Y2xhc3MgZnR7Y29uc3RydWN0b3IodCxzLGUsaSl7dGhpcy51cD0hMSx0aGlzLnNlbmQ9aSx0aGlzLnZhbHVlPTAsdGhpcy50eXBlPSJjYyJ9c2V0VmFsdWUodCl7dGhpcy52YWx1ZT10fXVwZGF0ZSh0LHMsZSl7cmV0dXJuIHRoaXMuaWQ9cywhdGhpcy51cCYmdD4wPyh0aGlzLnVwPSEwLHRoaXMuc2VuZCh7dHlwZToiU0lHTkFMX1RSSUdHRVIiLGlkOnMsdGltZTplfSksdGhpcy52YWx1ZSk6KHRoaXMudXA9dD4wLHRoaXMudmFsdWUpfX1jbGFzcyBwdCBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSksdGhpcy50eXBlPSJjYyIsdGhpcy52YWx1ZT1zLmlucHV0c1sxXT8/MH1zZXRWYWx1ZSh0KXt0aGlzLnZhbHVlPXR9dXBkYXRlKHQpe3JldHVybiB0aGlzLmlkPXQsdGhpcy52YWx1ZX19Y2xhc3MgYnQgZXh0ZW5kcyBve2NvbnN0cnVjdG9yKHQscyxlLGkpe3N1cGVyKHQscyxlLGkpLHRoaXMudHlwZT0ibWlkaWNjIix0aGlzLnZhbHVlPXMuaW5wdXRzWzJdPz8tMSx0aGlzLmNoYW5uZWw9LTEsdGhpcy5jY251bWJlcj0tMX1zZXRWYWx1ZSh0KXt0aGlzLnZhbHVlPXR9dXBkYXRlKHQscyl7cmV0dXJuIHRoaXMuY2NudW1iZXI9dCx0aGlzLmNoYW5uZWw9cyx0aGlzLnZhbHVlfX1jbGFzcyBndCBleHRlbmRzIG97Y29uc3RydWN0b3IodCxzLGUsaSl7c3VwZXIodCxzLGUsaSksdGhpcy5jbG9ja1Nnbj0hMCx0aGlzLnN0ZXA9MCx0aGlzLmZpcnN0PSEwfXVwZGF0ZSh0LC4uLnMpe3JldHVybiF0aGlzLmNsb2NrU2duJiZ0PjA/KHRoaXMuc3RlcD0odGhpcy5zdGVwKzEpJXMubGVuZ3RoLHRoaXMuY2xvY2tTZ249dD4wLDApOih0aGlzLmNsb2NrU2duPXQ+MCxzW3RoaXMuc3RlcF0pfX1jbGFzcyBtdCBleHRlbmRzIG97dXBkYXRlKHQsLi4ucyl7Y29uc3QgZT10JXMubGVuZ3RoK3MubGVuZ3RoO3JldHVybiBzW01hdGguZmxvb3IoZSklcy5sZW5ndGhdfX1jbGFzcyBTdHt1cGRhdGUodCxzLGUsaSxuKXtsZXQgcj1QKHQscyxlKTtyZXR1cm4gUyhyLGksbil9fWNsYXNzIHd0e3VwZGF0ZSh0LHMsZSl7cmV0dXJuIE1hdGgubWluKE1hdGgubWF4KHQscyksZSl9fWNsYXNzIHZ0e2NvbnN0cnVjdG9yKCl7dGhpcy5oaT0hMX11cGRhdGUodCl7cmV0dXJuIXRoaXMuaGkmJnQ+MD8odGhpcy5oaT0hMCwxKToodGhpcy5oaSYmdDw9MCYmKHRoaXMuaGk9ITEpLDApfX1jbGFzcyB5dHtjb25zdHJ1Y3Rvcigpe3RoaXMueDE9MCx0aGlzLngyPTAsdGhpcy55MT0wLHRoaXMueTI9MCx0aGlzLmEwPTEsdGhpcy5hMT0wLHRoaXMuYTI9MCx0aGlzLmIwPTEsdGhpcy5iMT0wLHRoaXMuYjI9MH11cGRhdGUodD0wLHM9MCxlPTUwMCxpPTEsbj0xKXtjb25zdCByPTIqTWF0aC5QSSplL3NhbXBsZVJhdGUsbD1NYXRoLnNpbihyKTtpPU1hdGgucG93KDEwLGkvMjApO2NvbnN0IHU9bC8oMippKSxjPU1hdGguY29zKHIpO2lmKHM9PT0wKXRoaXMuYjE9MS1jLHRoaXMuYjA9dGhpcy5iMS8yLHRoaXMuYjI9dGhpcy5iMCx0aGlzLmEwPTErdSx0aGlzLmExPS0yKmMsdGhpcy5hMj0xLXU7ZWxzZSBpZihzPT09MSl0aGlzLmIwPSgxK2MpLzIsdGhpcy5iMT0tKDErYyksdGhpcy5iMj10aGlzLmIwLHRoaXMuYTA9MSt1LHRoaXMuYTE9LTIqYyx0aGlzLmEyPTEtdTtlbHNlIGlmKHM9PT0yKXRoaXMuYjA9bC8yLHRoaXMuYjE9MCx0aGlzLmIyPS10aGlzLmIwLHRoaXMuYTA9MSt1LHRoaXMuYTE9LTIqYyx0aGlzLmEyPTEtdTtlbHNlIGlmKHM9PT0zKXRoaXMuYjA9MSx0aGlzLmIxPS0yKmMsdGhpcy5iMj0xLHRoaXMuYTA9MSt1LHRoaXMuYTE9LTIqYyx0aGlzLmEyPTEtdTtlbHNlIGlmKHM9PT00KXRoaXMuYjA9MS11LHRoaXMuYjE9LTIqYyx0aGlzLmIyPTErdSx0aGlzLmEwPTErdSx0aGlzLmExPS0yKmMsdGhpcy5hMj0xLXU7ZWxzZSBpZihzPT09NSl7Y29uc3QgYT1NYXRoLnBvdygxMCxuLzQwKTt0aGlzLmIwPTErdSphLHRoaXMuYjE9LTIqYyx0aGlzLmIyPTEtdSphLHRoaXMuYTA9MSt1L2EsdGhpcy5hMT0tMipjLHRoaXMuYTI9MS11L2F9ZWxzZSBpZihzPT09Nil7Y29uc3QgYT1NYXRoLnBvdygxMCxuLzQwKSxkPTIqTWF0aC5zcXJ0KGEpKnUsZj0oYS0xKSpjLHA9KGErMSkqYzt0aGlzLmIwPWEqKGErMS1mK2QpLHRoaXMuYjE9MiphKihhLTEtcCksdGhpcy5iMj1hKihhKzEtZi1kKSx0aGlzLmEwPWErMStmK2QsdGhpcy5hMT0tMiooYS0xK3ApLHRoaXMuYTI9YSsxK2YtZH1lbHNlIGlmKHM9PT03KXtjb25zdCBhPU1hdGgucG93KDEwLG4vNDApLGQ9MipNYXRoLnNxcnQoYSkqdSxmPShhLTEpKmMscD0oYSsxKSpjO3RoaXMuYjA9YSooYSsxK2YrZCksdGhpcy5iMT0tMiphKihhLTErcCksdGhpcy5iMj1hKihhKzErZi1kKSx0aGlzLmEwPWErMS1mK2QsdGhpcy5hMT0yKihhLTEtcCksdGhpcy5hMj1hKzEtZi1kfXRoaXMuYjAvPXRoaXMuYTAsdGhpcy5iMS89dGhpcy5hMCx0aGlzLmIyLz10aGlzLmEwLHRoaXMuYTEvPXRoaXMuYTAsdGhpcy5hMi89dGhpcy5hMCx0aGlzLmEwPTE7Y29uc3QgYj10aGlzLmIwKnQrdGhpcy5iMSp0aGlzLngxK3RoaXMuYjIqdGhpcy54Mi10aGlzLmExKnRoaXMueTEtdGhpcy5hMip0aGlzLnkyO3JldHVybiB0aGlzLngyPXRoaXMueDEsdGhpcy54MT10LHRoaXMueTI9dGhpcy55MSx0aGlzLnkxPWIsYn19Y29uc3QgTXQ9T2JqZWN0LmZyZWV6ZShPYmplY3QuZGVmaW5lUHJvcGVydHkoe19fcHJvdG9fXzpudWxsLEFEU1JOb2RlOnEsQXVkaW9JbjpsdCxBdWRpb05vZGU6byxCUEY6cnQsQmlxdWFkRmlsdGVyOnl0LEJyb3duTm9pc2VPc2M6VyxDQzpwdCxDTE9DS19QUFE6TSxDTE9DS19QUFM6TixDbGlwOnd0LENsb2NrOkYsQ2xvY2tEaXY6RCxDbG9ja091dDpMLERlbGF5OlYsRGlzdG9ydDpHLER1c3RPc2M6SyxGaWx0ZXI6bnQsRm9sZDp1dCxIb2xkOlIsSW1wdWxzZU9zYzpZLExhZzppdCxMY2dOb2lzZTpILE1pZGlDQzpidCxNaWRpRnJlcTpjdCxNaWRpR2F0ZTpvdCxNaWRpSW46bSxNaWRpVmVsOmR0LE5vaXNlT3NjOmosT3V0cHV0OkIsUGljazptdCxQaW5rTm9pc2U6WCxQdWxzZU9zYzp6LFJlbWFwOlN0LFNhd09zYzpKLFNjb3BlOmV0LFNlcXVlbmNlOmd0LFNpZGVjaGFpbkNvbXByZXNzb3I6dHQsU2lnbmFsOmZ0LFNpbmVPc2M6USxTbGV3Omh0LFNsaWRlOmF0LFRyaU9zYzpzdCxUcmlnOnZ0LFphd09zYzpafSxTeW1ib2wudG9TdHJpbmdUYWcse3ZhbHVlOiJNb2R1bGUifSkpLHg9bmV3IE1hcChPYmplY3QuZW50cmllcyhNdCkpO2NsYXNzIE90e2NvbnN0cnVjdG9yKHQscyl7Zyh0PT00OGUzKSx0aGlzLnNhbXBsZVJhdGU9dCx0aGlzLnBsYXlQb3M9MCx0aGlzLnNlbmQ9cyx0aGlzLnVuaXRzPVtdLHRoaXMudW5pdElEPTAsdGhpcy5mYWRlVGltZT0uMDEsdGhpcy5tYXhVbml0cz0xLHRoaXMucT1bXX1mYWRlT3V0VW5pdCh0KXt0LmZhZGVPdXQodGhpcy5wbGF5UG9zLHRoaXMuZmFkZVRpbWUpLHRoaXMuZnJlZVVuaXQodC5pZCx0aGlzLmZhZGVUaW1lKX1mYWRlT3V0VW5pdEJ5SWQodCl7Y29uc3Qgcz10aGlzLnVuaXRzLmZpbmQoZT0+ZS5pZD09PXQpO3MmJnRoaXMuZmFkZU91dFVuaXQocyl9ZmFkZU91dEFsbFVuaXRzKCl7dGhpcy51bml0cy5mb3JFYWNoKHQ9PnRoaXMuZmFkZU91dFVuaXQodCkpfWZhZGVPdXRPbGRVbml0cygpe2NvbnN0IHQ9dGhpcy51bml0cy5maWx0ZXIoZT0+ZS5hY3RpdmUpLHM9dC5sZW5ndGgtdGhpcy5tYXhVbml0cztzPD0wfHx0LnNsaWNlKDAscykuZm9yRWFjaChlPT50aGlzLmZhZGVPdXRVbml0KGUpKX1zdG9wKCl7dGhpcy5mYWRlT3V0QWxsVW5pdHMoKSx0aGlzLnNlbmQoe3R5cGU6IlNUT1AiLGZhZGVUaW1lOnRoaXMuZmFkZVRpbWV9KX1zcGF3blVuaXQodCxzKXtjb25zdCBlPW5ldyB4dCh0aGlzLnVuaXRJRCsrLHQsdGhpcy5zYW1wbGVSYXRlLHRoaXMuc2VuZCk7dGhpcy51bml0cy5wdXNoKGUpLGUuZmFkZUluKHRoaXMucGxheVBvcyx0aGlzLmZhZGVUaW1lKSx0aGlzLmZhZGVPdXRPbGRVbml0cygpLGNvbnNvbGUubG9nKGBzcGF3biB1bml0ICR7ZS5pZH0sIHVuaXRzIGFsaXZlOiAke3RoaXMudW5pdHMubGVuZ3RofWApLHMmJnRoaXMuc2NoZWR1bGVNZXNzYWdlKHttc2c6e3R5cGU6IkZBREVfT1VUX1VOSVQiLGlkOmUuaWR9LHRpbWU6c30pfWZyZWVVbml0KHQscyl7aWYocyl7dGhpcy5zY2hlZHVsZU1lc3NhZ2Uoe21zZzp7dHlwZToiRlJFRV9VTklUIixpZDp0fSx0aW1lOnN9KTtyZXR1cm59Y29uc3QgZT10aGlzLnVuaXRzLmxlbmd0aDt0aGlzLnVuaXRzPXRoaXMudW5pdHMuZmlsdGVyKGk9PmkuaWQhPT10KSxlPnRoaXMudW5pdHMubGVuZ3RoJiZjb25zb2xlLmxvZyhgZnJlZSB1bml0ICR7dH0sIHVuaXRzIGFsaXZlOiAke3RoaXMudW5pdHMubGVuZ3RofWApfXBhcnNlTXNnKHQpe3N3aXRjaCh0LnR5cGUpe2Nhc2UiU1BBV05fVU5JVCI6dGhpcy5zcGF3blVuaXQodC51bml0LHQuZHVyYXRpb24pO2JyZWFrO2Nhc2UiRlJFRV9VTklUIjp0aGlzLmZyZWVVbml0KHQuaWQpO2JyZWFrO2Nhc2UiRkFERV9PVVRfVU5JVCI6dGhpcy5mYWRlT3V0VW5pdEJ5SWQodC5pZCk7YnJlYWs7Y2FzZSJOT1RFX09OIjp0aGlzLm5vdGVPbih0KTticmVhaztjYXNlIkNDIjp0aGlzLm1pZGlDQyh0KTticmVhaztjYXNlIlNFVF9DT05UUk9MIjp0aGlzLnNldENvbnRyb2wodCk7YnJlYWs7Y2FzZSJGQURFX1RJTUUiOnRoaXMuZmFkZVRpbWU9TnVtYmVyKHQuZmFkZVRpbWUpO2JyZWFrO2Nhc2UiTUFYX1VOSVRTIjp0aGlzLm1heFVuaXRzPU51bWJlcih0Lm1heFVuaXRzKTticmVhaztjYXNlIlNUT1AiOnRoaXMuc3RvcCgpO2JyZWFrO2Nhc2UiU0VUX1VHRU4iOnRoaXMuYWRkVWdlbih0LmNsYXNzTmFtZSx0LnVnZW4pO2JyZWFrO2Nhc2UiU0NIRURVTEVfTVNHIjp0aGlzLnNjaGVkdWxlTWVzc2FnZSh0KTticmVhaztjYXNlIkJBVENIX01TRyI6dC5tZXNzYWdlcy5mb3JFYWNoKHM9PnRoaXMucGFyc2VNc2cocykpO2JyZWFrO2RlZmF1bHQ6dGhyb3cgbmV3IFR5cGVFcnJvcihgdW5rbm93biBtZXNzYWdlIHR5cGUgJHt0LnR5cGV9YCl9fW5vdGVPbih0KXt0aGlzLnVuaXRzLmZvckVhY2gocz0+cy5ub3RlT24odCkpfW1pZGlDQyh0KXt0aGlzLnVuaXRzLmZvckVhY2gocz0+cy5taWRpQ0ModCkpfXNldENvbnRyb2wodCl7dGhpcy51bml0cy5mb3JFYWNoKHM9PnMuc2V0Q29udHJvbCh0KSl9c2NoZWR1bGVNZXNzYWdlKHQpe2lmKHQudGltZT10aGlzLnBsYXlQb3MrdC50aW1lLCF0aGlzLnEubGVuZ3RoKXt0aGlzLnEucHVzaCh0KTtyZXR1cm59bGV0IHM9MDtmb3IoO3M8dGhpcy5xLmxlbmd0aCYmdGhpcy5xW3NdLnRpbWU8dC50aW1lOylzKys7dGhpcy5xLnNwbGljZShzLDAsdCl9Z2VuU2FtcGxlKHQpe2Zvcig7dGhpcy5xLmxlbmd0aD4wJiZ0aGlzLnFbMF0udGltZTw9dGhpcy5wbGF5UG9zOyl0aGlzLnBhcnNlTXNnKHRoaXMucVswXS5tc2cpLHRoaXMucS5zaGlmdCgpO2lmKCF0aGlzLnVuaXRzLmxlbmd0aClyZXR1cm5bMCwwXTtjb25zdCBzPVswLDBdO2ZvcihsZXQgZT0wO2U8dGhpcy51bml0cy5sZW5ndGg7ZSsrKXtjb25zdCBpPXRoaXMudW5pdHNbZV0sbj1pLmdldExldmVsKHRoaXMucGxheVBvcyk7aS5nZW5TYW1wbGUodGhpcy5wbGF5UG9zLGkubm9kZXMsdCxpLnJlZ2lzdGVycyxpLm91dHB1dHMsaS5zb3VyY2VzKSxzWzBdKz1pLm91dHB1dHNbMF0qbixzWzFdKz1pLm91dHB1dHNbMV0qbn1yZXR1cm4gdGhpcy5wbGF5UG9zKz0xLzQ4ZTMsc31hZGRVZ2VuKHQscyl7Y29uc3QgZT1uZXcgRnVuY3Rpb24oYCR7c307cmV0dXJuICR7dH1gKSgpO3guc2V0KHQsZSl9fWNsYXNzIHh0e2NvbnN0cnVjdG9yKHQscyxlLGkpe3RoaXMuaWQ9dCx0aGlzLnNhbXBsZVJhdGU9ZSx0aGlzLnNlbmQ9aSx0aGlzLm5vZGVzPVtdLHRoaXMuYWN0aXZlPSEwO2ZvcihsZXQgciBpbiBzLnVnZW5zKXtjb25zdCBsPXMudWdlbnNbcl07aWYoeC5oYXMobC50eXBlKSl7Y29uc3QgdT14LmdldChsLnR5cGUpLGM9TnVtYmVyKHIpO3RoaXMubm9kZXNbY109bmV3IHUoYyxsLHRoaXMuc2FtcGxlUmF0ZSx0aGlzLnNlbmQpfWVsc2UgY29uc29sZS53YXJuKGB1bmtub3duIHVnZW4gIiR7bC50eXBlfSJgKX10aGlzLnJlZ2lzdGVycz1uZXcgQXJyYXkocy5yZWdpc3RlcnMpLmZpbGwoMCk7bGV0IG49MTY7dGhpcy5vdXRwdXRzPW5ldyBBcnJheShuKS5maWxsKDApLHRoaXMuc291cmNlcz1uZXcgQXJyYXkobikuZmlsbCgwKSxzLnNyYz1gby5maWxsKDApOyAvLyByZXNldCBvdXRwdXRzCmArcy5zcmMsdGhpcy5nZW5TYW1wbGU9bmV3IEZ1bmN0aW9uKCJ0aW1lIiwibm9kZXMiLCJpbnB1dCIsInIiLCJvIiwicyIscy5zcmMpfW5vdGVPbih0KXt2YXIgdSxjLGI7Y29uc3R7Y2hhbm5lbDpzLG5vdGU6ZSx2ZWxvY2l0eTppfT10LG49dGhpcy5ub2Rlcy5maWx0ZXIoYT0+YS50eXBlPT09Im1pZGlmcmVxIiYmKGEuY2hhbm5lbD09PS0xfHxhLmNoYW5uZWw9PT1zKSkscj10aGlzLm5vZGVzLmZpbHRlcihhPT5hLnR5cGU9PT0ibWlkaWdhdGUiJiYoYS5jaGFubmVsPT09LTF8fGEuY2hhbm5lbD09PXMpKSxsPXRoaXMubm9kZXMuZmlsdGVyKGE9PmEudHlwZT09PSJtaWRpdmVsIiYmKGEuY2hhbm5lbD09PS0xfHxhLmNoYW5uZWw9PT1zKSk7aWYoaT4wKXtsZXQgYT1uLmZpbmQocD0+cC5pc0ZyZWUoKSl8fG5bMF0sZD1yLmZpbmQocD0+cC5pc0ZyZWUoKSl8fHJbMF0sZj1sLmZpbmQocD0+cC5pc0ZyZWUoKSl8fGxbMF07YT09bnVsbHx8YS5ub3RlT24oZSxpKSxkPT1udWxsfHxkLm5vdGVPbihlLGkpLGY9PW51bGx8fGYubm90ZU9uKGUsaSl9ZWxzZSh1PW4uZmluZChhPT5hLm5vdGU9PT1lKSk9PW51bGx8fHUubm90ZU9mZigpLChjPXIuZmluZChhPT5hLm5vdGU9PT1lKSk9PW51bGx8fGMubm90ZU9mZigpLChiPWwuZmluZChhPT5hLm5vdGU9PT1lKSk9PW51bGx8fGIubm90ZU9mZigpfW1pZGlDQyh0KXtjb25zdHtjaGFubmVsOnMsY2M6ZSx2YWx1ZTppfT10O3RoaXMubm9kZXMuZm9yRWFjaChuPT57bi50eXBlPT09Im1pZGljYyImJihuLmNoYW5uZWw9PT0tMXx8bi5jaGFubmVsPT09cykmJm4uY2NudW1iZXI9PT1lJiZuLnNldFZhbHVlKGkpfSl9c2V0Q29udHJvbCh0KXtjb25zdHt2YWx1ZTpzLGlkOmV9PXQsaT10aGlzLm5vZGVzLmZpbmQobj0+bi50eXBlPT09ImNjIiYmbi5pZD09PWUpO2kmJmkuc2V0VmFsdWUocyl9Z2V0TGV2ZWwodCl7cmV0dXJuIHRoaXMuZmFkZVN0YXJ0PT09dm9pZCAwPzA6dGhpcy5hY3RpdmU/UygodC10aGlzLmZhZGVTdGFydCkvdGhpcy5mYWRlVGltZSwwLC4zKTpTKCh0LXRoaXMuZmFkZVN0YXJ0KS90aGlzLmZhZGVUaW1lLHRoaXMuZmFkZUZyb20sMCl9ZmFkZUluKHQscyl7dGhpcy5mYWRlU3RhcnQ9dCx0aGlzLmZhZGVUaW1lPXN9ZmFkZU91dCh0LHMpe3RoaXMuZmFkZVRpbWU9cyx0aGlzLmZhZGVGcm9tPXRoaXMuZ2V0TGV2ZWwodCksdGhpcy5hY3RpdmU9ITEsdGhpcy5mYWRlU3RhcnQ9dH1pc0RvbmUodCl7cmV0dXJuIXRoaXMuYWN0aXZlJiZ0aGlzLmdldExldmVsKHQpPT09MH19Y2xhc3MgVHQgZXh0ZW5kcyBBdWRpb1dvcmtsZXRQcm9jZXNzb3J7Y29uc3RydWN0b3IoKXtzdXBlcigpLHRoaXMucG9ydC5vbm1lc3NhZ2U9dGhpcy5vbm1lc3NhZ2UuYmluZCh0aGlzKSx0aGlzLmF1ZGlvR3JhcGg9bmV3IE90KDQ4ZTMsdGhpcy5wb3J0LnBvc3RNZXNzYWdlLmJpbmQodGhpcy5wb3J0KSl9b25tZXNzYWdlKHQpe2xldCBzPXQuZGF0YTt0aGlzLmF1ZGlvR3JhcGgucGFyc2VNc2cocyl9cHJvY2Vzcyh0LHMsZSl7Y29uc3QgaT1zWzBdLG49dFswXVswXSxyPWlbMF0sbD1pWzFdO2ZvcihsZXQgdT0wO3U8ci5sZW5ndGg7dSsrKXtsZXRbYyxiXT10aGlzLmF1ZGlvR3JhcGguZ2VuU2FtcGxlKG4/blt1XTowKTtyW3VdPWMsbFt1XT1ifXJldHVybiEwfX1yZWdpc3RlclByb2Nlc3Nvcigic2FtcGxlLWdlbmVyYXRvciIsVHQpfSkoKTsK";
  var ye = "data:text/javascript;base64,KGZ1bmN0aW9uKCl7InVzZSBzdHJpY3QiO2NsYXNzIG4gZXh0ZW5kcyBBdWRpb1dvcmtsZXRQcm9jZXNzb3J7c3RhdGljIGdldCBwYXJhbWV0ZXJEZXNjcmlwdG9ycygpe3JldHVyblt7bmFtZToiaXNSZWNvcmRpbmciLGRlZmF1bHRWYWx1ZTowfV19Y29uc3RydWN0b3IoKXtzdXBlcigpLHRoaXMuX2J1ZmZlclNpemU9MjA0OCx0aGlzLl9idWZmZXI9bmV3IEZsb2F0MzJBcnJheSh0aGlzLl9idWZmZXJTaXplKSx0aGlzLl9pbml0QnVmZmVyKCl9X2luaXRCdWZmZXIoKXt0aGlzLl9ieXRlc1dyaXR0ZW49MH1faXNCdWZmZXJFbXB0eSgpe3JldHVybiB0aGlzLl9ieXRlc1dyaXR0ZW49PT0wfV9pc0J1ZmZlckZ1bGwoKXtyZXR1cm4gdGhpcy5fYnl0ZXNXcml0dGVuPT09dGhpcy5fYnVmZmVyU2l6ZX1fYXBwZW5kVG9CdWZmZXIoZSl7dGhpcy5faXNCdWZmZXJGdWxsKCkmJnRoaXMuX2ZsdXNoKCksdGhpcy5fYnVmZmVyW3RoaXMuX2J5dGVzV3JpdHRlbl09ZSx0aGlzLl9ieXRlc1dyaXR0ZW4rPTF9X2ZsdXNoKCl7bGV0IGU9dGhpcy5fYnVmZmVyO3RoaXMuX2J5dGVzV3JpdHRlbjx0aGlzLl9idWZmZXJTaXplJiYoZT1lLnNsaWNlKDAsdGhpcy5fYnl0ZXNXcml0dGVuKSksdGhpcy5wb3J0LnBvc3RNZXNzYWdlKHtldmVudFR5cGU6ImRhdGEiLGF1ZGlvQnVmZmVyOmV9KSx0aGlzLl9pbml0QnVmZmVyKCl9X3JlY29yZGluZ1N0b3BwZWQoKXt0aGlzLnBvcnQucG9zdE1lc3NhZ2Uoe2V2ZW50VHlwZToic3RvcCJ9KX1wcm9jZXNzKGUsbyxoKXtjb25zdCBpPWguaXNSZWNvcmRpbmcsZj1vWzBdLHM9ZVswXSx1PWZbMF0sXz1mWzFdO2xldCByPSExO2ZvcihsZXQgdD0wO3Q8dS5sZW5ndGg7dCsrKXQ8aS5sZW5ndGgmJihyPWlbdF09PT0xKSwhciYmIXRoaXMuX2lzQnVmZmVyRW1wdHkoKSYmKHRoaXMuX2ZsdXNoKCksdGhpcy5fcmVjb3JkaW5nU3RvcHBlZCgpKSxyJiYodGhpcy5fYXBwZW5kVG9CdWZmZXIoc1swXVt0XSksdGhpcy5fYXBwZW5kVG9CdWZmZXIoc1sxXVt0XSkpLHVbdF09c1swXVt0XSxfW3RdPXNbMV1bdF07cmV0dXJuITB9fXJlZ2lzdGVyUHJvY2Vzc29yKCJyZWNvcmRlciIsbil9KSgpOwo=";
  function fe(e, t, i) {
    if (e.length < 1)
      return;
    e[0];
    const n = 3, l2 = 32, o = e.map((X) => X.length).reduce((X, z3) => X + z3, 0), d2 = l2 / 8, G2 = i * d2, p = 44, m3 = new ArrayBuffer(p + o * d2), r = new DataView(m3);
    V(r, 0, "RIFF"), r.setUint32(4, 36 + o * d2, true), V(r, 8, "WAVE"), V(r, 12, "fmt "), r.setUint32(16, 16, true), r.setUint16(20, n, true), r.setUint16(22, i, true), r.setUint32(24, t, true), r.setUint32(28, t * G2, true), r.setUint16(32, G2, true), r.setUint16(34, l2, true), V(r, 36, "data"), r.setUint32(40, o * d2, true);
    let R2 = 44;
    for (const X of e)
      ge(r, R2, X), R2 += X.length * d2;
    return m3;
  }
  function V(e, t, i) {
    for (let n = 0; n < i.length; n++)
      e.setUint8(t + n, i.charCodeAt(n));
  }
  function ge(e, t, i) {
    for (var n = 0; n < i.length; n++, t += 4)
      e.setFloat32(t, i[n], true);
  }
  var k = /* @__PURE__ */ Object.freeze(/* @__PURE__ */ Object.defineProperty({
    __proto__: null,
    Node: c,
    compile: T,
    evaluate: j,
    exit: J,
    exportModule: ce,
    getInletName: oe,
    getNode: F,
    module: g,
    n: f,
    node: x,
    nodeRegistry: y,
    outputType: Y,
    polyType: Z,
    register: u,
    registerNode: a
  }, Symbol.toStringTag, { value: "Module" }));
  var Xe = (e) => `Math.sin(${e})`;
  var Ze = (e) => `Math.cos(${e})`;
  var be = (e) => `Math.tan(${e})`;
  var xe = (e) => `Math.asin(${e})`;
  var Re = (e) => `Math.acos(${e})`;
  var Le = (e) => `Math.atan(${e})`;
  var B = (e, t, i) => `${e} = ${t};${i ? ` /* ${i} */` : ""}`;
  var w = (e, ...t) => B(
    e.name,
    `nodes[${e.ugenIndex}].update(${t.join(",")})`,
    e.node.type
  );
  var Me = (e) => `(2 ** ((${e} - 69) / 12) * 440)`;
  var ze = (e, t) => `${e} ** ${t}`;
  var Ve = (e) => `Math.exp(${e})`;
  var We = (e) => `Math.log(${e})`;
  var Ye = (e, t) => `${e}%${t}`;
  var Ne = (e) => `Math.abs(${e})`;
  var He = (e) => `Math.round(${e})`;
  var Se = (e) => `Math.floor(${e})`;
  var Te = (e) => `Math.sign(${e})`;
  var we = (e) => `Math.ceil(${e})`;
  var Ke = (e, t) => `Math.min(${e}, ${t})`;
  var Ce = (e, t) => `Math.max(${e}, ${t})`;
  var ve = (e, t) => `[${e}, ${t}]`;
  var L = (e) => `${e}[0]`;
  var Ie = (e) => `${e}[1]`;
  var Ue = (e, t) => `(${L(e)} < ${L(t)} ? ${e} : ${t})`;
  var ke = (e, t) => `(${L(e)} > ${L(t)} ? ${e} : ${t})`;
  var Pe = /* @__PURE__ */ Object.freeze(/* @__PURE__ */ Object.defineProperty({
    __proto__: null,
    abs: Ne,
    ceil: we,
    def: B,
    defAcos: Re,
    defAsin: xe,
    defAtan: Le,
    defCos: Ze,
    defSin: Xe,
    defTan: be,
    defUgen: w,
    exp: Ve,
    floor: Se,
    log: We,
    max: Ce,
    midinote: Me,
    min: Ke,
    mod: Ye,
    pair_a: L,
    pair_a_max: ke,
    pair_a_min: Ue,
    pair_b: Ie,
    pair_make: ve,
    pow: ze,
    round: He,
    sign: Te
  }, Symbol.toStringTag, { value: "Module" }));
  var Fe = class {
    constructor(t = null) {
      this.ugens = /* @__PURE__ */ new Map(), this.outputNode = t;
    }
    async spawn(t, i) {
      this.graph = t;
      const { src: n, ugens: l2, registers: o } = t.compile({
        log: false
      });
      !this.mouse && n.includes("mouse") && this.initMouse(), !this.midiInited && l2.some((d2) => d2.type.startsWith("Midi")) && this.initMidi(), !this.audioIn && l2.some((d2) => d2.type === "AudioIn") && await this.initAudioIn(), this.sendCustomUgens(), this.send({
        type: "SPAWN_UNIT",
        unit: { src: n, ugens: l2, registers: o },
        duration: i
        // experimental
      });
    }
    // ugen is expected to be a class
    registerUgen(t) {
      this.ugens.set(t.name, t);
    }
    // custom ugens
    sendCustomUgens() {
      if (!this.ugens.size)
        return;
      let t = [];
      for (let [i, n] of this.ugens)
        t.push({
          type: "SET_UGEN",
          className: i,
          ugen: n + ""
        });
      this.send({
        type: "BATCH_MSG",
        messages: t
      });
    }
    scheduleMessage(t, i) {
      this.send({
        type: "SCHEDULE_MSG",
        msg: t,
        time: i
      });
    }
    setControl(t, i, n) {
      const l2 = {
        type: "SET_CONTROL",
        id: t,
        value: i
      };
      n ? this.send({ type: "SCHEDULE_MSG", time: n, msg: l2 }) : this.send(l2);
    }
    // controls: { id, value, time }[]
    setControls(t) {
      const i = {
        type: "BATCH_MSG",
        messages: t.map((n) => {
          const l2 = { type: "SET_CONTROL", id: n.id, value: n.value };
          return n.time === void 0 ? l2 : {
            type: "SCHEDULE_MSG",
            time: n.time,
            msg: l2
          };
        })
      };
      this.send(i);
    }
    async initAudioIn() {
      console.log("init audio input...");
      const t = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: false
        }
      });
      this.audioCtx.createMediaStreamSource(t).connect(this.audioWorklet);
    }
    initMidi() {
      console.log("init midi input..."), this.midiInited = true, new me().on("midimessage", (i, n) => {
        const l2 = re(n);
        l2 && this.send(l2);
      });
    }
    initMouse() {
      console.log("init mouse"), this.mouse = new he(), this.mouse.on("move", (t, i) => {
        this.setControl("mouseX", t), this.setControl("mouseY", i);
      });
    }
    /**
     * Send a message to the audio thread (audio worket)
     */
    send(t) {
      W(t instanceof Object), this.audioWorklet && this.audioWorklet.port.postMessage(t);
    }
    async init() {
      if (!this.audioCtx) {
        if (W(!this.audioCtx), this.audioCtx = this.outputNode?.context || new AudioContext({
          latencyHint: "interactive",
          sampleRate: 48e3
        }), await this.audioCtx.resume(), !this.audioCtx.audioWorklet)
          throw new Error(
            "Audio cannot be loaded: non-secure origin? (AudioContext.audioWorklet is undefined)"
          );
        await this.audioCtx.audioWorklet.addModule(Ge), await this.audioCtx.audioWorklet.addModule(ye), this.audioWorklet = new AudioWorkletNode(
          this.audioCtx,
          "sample-generator",
          {
            outputChannelCount: [2]
          }
        ), this.audioWorklet.port.onmessage = (t) => {
          const { id: i, time: n, type: l2 } = t.data;
          l2 === "SIGNAL_TRIGGER" ? this.graph.dfs((o) => {
            if (o.type === "signal" && o.id === i) {
              const d2 = o.callback(n, i);
              isNaN(d2) ? d2 !== void 0 && console.warn(
                `expected number from "on" callback with id "${i}", got "${d2}" instead.`
              ) : window.postMessage({
                type: "KABELSALAT_SET_CONTROL",
                value: d2,
                id: i
              });
            }
            return o;
          }) : l2 === "STOP" ? setTimeout(() => this.destroy(), t.data.fadeTime * 1e3 + 200) : l2 === "SEND_SAMPLES" && this.graph.dfs((o) => (o.type !== "scope" || o.ins.length < 2 || o.ins[1].value !== i || window.postMessage({
            type: "KABELSALAT_UPDATE_SCOPE",
            samples: t.data.samples,
            channels: t.data.channels,
            channel: t.data.channel,
            id: i
          }), o));
        }, this.recorder = new window.AudioWorkletNode(this.audioCtx, "recorder"), this.audioWorklet.connect(this.recorder), this.sendCustomUgens(), this.recorder.connect(this.outputNode || this.audioCtx.destination), this.recorder.port.onmessage = (t) => {
          if (t.data.eventType === "data" && this.recordedBuffers.push(t.data.audioBuffer), t.data.eventType === "stop") {
            console.log("recording stopped");
            const i = fe(
              this.recordedBuffers,
              this.audioCtx.sampleRate,
              2
            );
            Je(i, "kabelsalat.wav", "audio/wav"), this.recordedBuffers = [];
          }
        }, this.recordOnPlay && this.record();
      }
    }
    destroy() {
      this.audioWorklet?.disconnect(), this.audioWorklet = null, this.recorder?.disconnect(), this.recorder = null, !this.outputNode && this.audioCtx?.close(), this.audioCtx = null;
    }
    /**
     * Stop audio playback
     */
    stop() {
      this.audioCtx && this.send({ type: "STOP" }), this.mouse?.detach();
    }
    record() {
      if (!this.audioCtx) {
        this.recordOnPlay = true;
        return;
      }
      this.recordedBuffers = [], this.recorder.parameters.get("isRecording").setValueAtTime(1, 0), console.log("recording started");
    }
    stopRecording() {
      this.recordOnPlay = false, this.audioCtx && this.recorder.parameters.get("isRecording").setValueAtTime(0, 0);
    }
    set fadeTime(t) {
      this.send({ type: "FADE_TIME", fadeTime: t });
    }
    set maxUnits(t) {
      this.send({ type: "MAX_UNITS", maxUnits: t });
    }
  };
  function Je(e, t, i) {
    const n = new Blob([e], { type: i }), l2 = document.createElement("a");
    l2.href = window.URL.createObjectURL(n), l2.download = t, l2.click();
  }
  var je = 0;
  var _i = u(
    "signal",
    (e, t) => {
      const i = je++, n = getNode("signal", e, i);
      return n.callback = t, n.id = i, n;
    },
    {
      ugen: "Signal",
      compile: ({ vars: [e, t], ...i }) => w(i, e, t, "time")
    }
  );
  var Qe = (e) => `sin(${e})`;
  var Be = (e) => `cos(${e})`;
  var Ee = (e) => `tan(${e})`;
  var Oe = (e) => `asin(${e})`;
  var $e = (e) => `acos(${e})`;
  var De = (e) => `atan(${e})`;
  var N = (e, t, i) => `${e} = ${t};${i ? ` /* ${i} */` : ""}`;
  var Ae = (e, ...t) => {
    if (t.unshift(`nodes[${e.ugenIndex}]`), e.ugen === "Sequence" || e.ugen === "Pick") {
      const i = t.length - 2, n = `(float[${i}]){${t.slice(2).join(",")}}`;
      return N(
        e.name,
        `${e.ugen}_update(${t[0]}, ${t[1]}, ${i}, ${n})`,
        e.ugen
      );
    }
    return N(e.name, `${e.ugen}_update(${t.join(",")})`, e.ugen);
  };
  var qe = (e) => `pow(2.0, ((${e} - 69.0) / 12.0)) * 440.0`;
  var _e = (e, t) => `pow(${e}, ${t})`;
  var et = (e) => `exp(${e})`;
  var tt = (e) => `log(${e})`;
  var it = (e, t) => `${e}>=${t}?${e}-${t}:${e}`;
  var nt = (e) => `fabs(${e})`;
  var st = (e, t) => `fmin(${e}, ${t})`;
  var lt = (e, t) => `fmax(${e}, ${t})`;
  var at = (e) => `fround(${e})`;
  var ot = (e) => `floor(${e})`;
  var dt = (e) => `ceil(${e})`;
  var ct = (e, t) => `((pair) {${e}, ${t}})`;
  var M = (e) => `${e}.a`;
  var pt = (e) => `${e}.b`;
  var ut = (e, t) => `(${M(e)} < ${M(t)} ? ${e} : ${t})`;
  var mt = (e, t) => `(${M(e)} > ${M(t)} ? ${e} : ${t})`;
  var rt = /* @__PURE__ */ Object.freeze(/* @__PURE__ */ Object.defineProperty({
    __proto__: null,
    abs: nt,
    ceil: dt,
    def: N,
    defAcos: $e,
    defAsin: Oe,
    defAtan: De,
    defCos: Be,
    defSin: Qe,
    defTan: Ee,
    defUgen: Ae,
    exp: et,
    floor: ot,
    log: tt,
    max: lt,
    midinote: qe,
    min: st,
    mod: it,
    pair_a: M,
    pair_a_max: mt,
    pair_a_min: ut,
    pair_b: pt,
    pair_make: ct,
    pow: _e,
    round: at
  }, Symbol.toStringTag, { value: "Module" }));
  var s = {
    js: Pe,
    c: rt
  };
  var E = (e, t) => a(e, {
    ugen: t,
    compile: ({ vars: i, ...n }) => s[n.lang].defUgen(n, ...i)
  });
  var ht = u("time", (e) => new c("time", e), {
    tags: ["meta"],
    description: "Returns elapsed time in seconds",
    compile: ({ name: e, lang: t }) => s[t].def(e, "time")
  });
  var Gt = u(
    "raw",
    (e, t) => new c("raw", t).withIns(f(e)),
    {
      ins: [
        { name: "in" },
        {
          name: "code",
          description: "expression with variable `t` being the elapsed time and `$input` the input."
        }
      ],
      tags: ["meta"],
      description: "Raw code node, expects floats between -1 and 1",
      compile: ({ vars: e, node: t, name: i }) => `let $input = ${e[0]}; 
const ${i} = (${t.value}); // raw`,
      examples: [
        `sine(4).range(.5,1)
.raw("(time*110%1*2-1)*$input")
.out()`
      ]
    }
  );
  var yt = u(
    "bytebeat",
    (e, t) => new c("bytebeat", t).withIns(f(e)),
    {
      ins: [
        { name: "t", description: "time in samples" },
        {
          name: "code",
          description: "bytebeat code with variable `t`"
        }
      ],
      tags: ["meta"],
      description: "Bytebeat node, expects numbers from 0 to 255",
      examples: [
        `time().mul(8000).bytebeat\`
// Fractalized Past
// by: lhphr
// from: https://dollchan.net/btb/res/3.html#69

(t>>10^t>>11)%5*((t>>14&3^t>>15&1)+1)*t%99+((3+(t>>14&3)-(t>>16&1))/3*t%99&64)
\`.out()`
      ],
      compile: ({ vars: e, node: t, name: i }) => `let t = ${e[0]}; 
const ${i} = ((${t.value}) & 255) / 127.5 - 1; // bytebeat`
    }
  );
  var ft = u(
    "floatbeat",
    (e, t) => new c("bytebeat", t).withIns(f(e)),
    {
      ins: [
        { name: "t", description: "time in samples" },
        {
          name: "code",
          description: "floatbeat code with variable `t`"
        }
      ],
      tags: ["meta"],
      description: "Raw code node, expects numbers from -1 to 1",
      compile: ({ vars: e, node: t, name: i }) => `let t = ${e[0]}; const ${i} = (${t.value}); // floatbeat`
    }
  );
  var gt = a("adsr", {
    ugen: "ADSRNode",
    tags: ["envelope"],
    description: "ADSR envelope",
    examples: [
      `impulse(1).perc(.5)
.adsr(.01, .1, .5, .1)
.mul(sine(220)).out()`
    ],
    ins: [
      { name: "gate", default: 0, description: "gate input" },
      { name: "att", default: 0.02, description: "attack time" },
      { name: "dec", default: 0.1, description: "decay time" },
      { name: "sus", default: 0.2, description: "sustain level" },
      { name: "rel", default: 0.1, description: "release time" }
    ],
    compile: ({
      vars: [e = 0, t = 0.02, i = 0.1, n = 0.2, l2 = 0.1],
      ...o
    }) => s[o.lang].defUgen(o, "time", e, t, i, n, l2)
  });
  var Xt = g(
    "ar",
    (e = 0, t = 0.02, i = 0.1) => e.adsr(t, 0, 1, i),
    {
      tags: ["envelope"],
      description: "AR envelope",
      examples: ["impulse(1).ad(.01, .1).mul(sine(220)).out()"],
      ins: [
        { name: "trig", default: 0, description: "gate input" },
        { name: "att", default: 0.02, description: "attack time" },
        { name: "rel", default: 0.1, description: "release time" }
      ]
    }
  );
  var Zt = g(
    "ad",
    (e = 0, t = 0.02, i = 0.1) => e.adsr(t, i, 0, i),
    {
      tags: ["envelope"],
      description: "AD envelope",
      examples: ["impulse(1).ad(.01, .1).mul(sine(220)).out()"],
      ins: [
        { name: "trig", default: 0, description: "gate input" },
        { name: "att", default: 0.02, description: "attack time" },
        { name: "dec", default: 0.1, description: "decay time" }
      ]
    }
  );
  var bt = a("clock", {
    ugen: "Clock",
    internal: true,
    // impulse is the preferred way..
    tags: ["regular", "clock"],
    description: "Clock source, with tempo in BPM",
    examples: ["clock(120).clockdiv(16).mul(sine(220)).out()"],
    ins: [
      {
        name: "bpm",
        default: 120,
        description: "clock tempo in bpm (beats per minute)"
      }
    ],
    compile: ({ vars: [e = 120], ...t }) => s[t.lang].defUgen(t, e)
  });
  var xt = a("clockdiv", {
    ugen: "ClockDiv",
    tags: ["clock", "trigger"],
    description: "Clock signal divider",
    examples: ["impulse(8).clockdiv(2).ad(.1,.1).mul(sine(220)).out()"],
    ins: [
      { name: "clock", default: 0, description: "clock input" },
      { name: "divisor", default: 2, description: "tempo divisor" }
    ],
    compile: ({ vars: [e = 0, t = 2], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var Rt = a("distort", {
    ugen: "Distort",
    tags: ["fx", "distortion"],
    description: "Overdrive-style distortion",
    examples: [
      `sine(220)
.distort( saw(.5).range(0,1) )
.out()`
    ],
    ins: [
      { name: "in", default: 0 },
      { name: "amt", default: 0, description: "distortion amount" }
    ],
    compile: ({ vars: [e = 0, t = 0], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var O = a("noise", {
    ugen: "NoiseOsc",
    tags: ["source", "noise"],
    description: "White noise source",
    examples: ["noise().mul(.25).out()"],
    ins: [
      {
        name: "next",
        default: 1,
        description: "if 0, the noise will hold the previous value. defaults to 1."
      }
    ],
    compile: ({ lang: e, vars: [t = 1], ...i }) => s[e].defUgen(i, t)
  });
  var Lt = a("lcgnoise", {
    ugen: "LcgNoise",
    tags: ["source", "noise"],
    description: "Lcg white noise source.",
    examples: ["lcgnoise().mul(.25).out()"],
    ins: [
      {
        name: "next",
        default: 1,
        description: "if 0, the noise will hold the previous value. defaults to 1."
      },
      {
        name: "reset",
        default: 0,
        description: "if 1, the random number generator sequence will reset."
      }
    ],
    compile: ({ lang: e, vars: [t = 1, i = 0], ...n }) => s[e].defUgen(n, t, i)
  });
  var Mt = a("pink", {
    ugen: "PinkNoise",
    tags: ["source", "noise"],
    description: "Pink noise source",
    examples: ["pink().mul(.5).out()"],
    ins: [],
    compile: ({ lang: e, ...t }) => s[e].defUgen(t)
  });
  var zt = a("brown", {
    ugen: "BrownNoiseOsc",
    tags: ["source", "noise"],
    description: "Brown noise source",
    examples: ["brown().out()"],
    ins: [],
    compile: ({ lang: e, ...t }) => s[e].defUgen(t)
  });
  var Vt = a("dust", {
    ugen: "DustOsc",
    tags: ["trigger", "noise", "source"],
    description: "Generates random impulses from 0 to +1.",
    examples: ["dust(200).out()"],
    ins: [
      { name: "density", default: 0, description: "average impulses per second" }
    ],
    compile: ({ vars: [e = 0], ...t }) => s[t.lang].defUgen(t, e)
  });
  var $ = a("impulse", {
    ugen: "ImpulseOsc",
    tags: ["regular", "trigger"],
    description: "Regular single sample impulses (0 - 1)",
    examples: ["impulse(10).out()"],
    ins: [
      { name: "freq", default: 0 },
      { name: "phase", default: 0 }
    ],
    compile: ({ vars: [e = 0, t = 0], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var Wt = a("saw", {
    ugen: "SawOsc",
    tags: ["regular", "waveform", "source"],
    description: "Sawtooth wave oscillator with anti aliasing",
    examples: ["saw(110).mul(.5).out()"],
    ins: [{ name: "freq", default: 0 }],
    compile: ({ vars: [e = 0], ...t }) => s[t.lang].defUgen(t, e)
  });
  var Yt = a("zaw", {
    ugen: "ZawOsc",
    tags: ["regular", "waveform", "source"],
    description: "Sawtooth wave oscillator with sharp edges. Use saw for anti aliased variant.",
    examples: ["zaw(110).mul(.5).out()"],
    ins: [{ name: "freq", default: 0 }],
    compile: ({ vars: [e = 0], ...t }) => s[t.lang].defUgen(t, e)
  });
  var Nt = a("sine", {
    tags: ["regular", "waveform", "source"],
    ugen: "SineOsc",
    description: "Sine wave oscillator",
    examples: ["sine(220).out()"],
    ins: [
      { name: "freq", default: 0 },
      { name: "sync", default: 0, description: "sync input" },
      { name: "phase", default: 0, description: "phase offset" }
    ],
    compile: ({ vars: [e = 0, t = 0, i = 0], ...n }) => s[n.lang].defUgen(n, e, t, i)
  });
  var Ht = a("tri", {
    ugen: "TriOsc",
    tags: ["regular", "waveform", "source"],
    description: "Triangle wave oscillator",
    examples: ["tri(220).out()"],
    ins: [{ name: "freq", default: 0 }],
    compile: ({ vars: [e = 0], ...t }) => s[t.lang].defUgen(t, e)
  });
  var St = a("pulse", {
    ugen: "PulseOsc",
    tags: ["regular", "waveform", "source"],
    description: "Pulse wave oscillator",
    examples: ["pulse(220, sine(.1).range(.1,.5)).mul(.5).out()"],
    ins: [
      { name: "freq", default: 0 },
      { name: "pw", default: 0.5, description: "pulse width 0 - 1" }
    ],
    compile: ({ vars: [e = 0, t = 0.5], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var Tt = a("slide", {
    ugen: "Slide",
    tags: ["fx"],
    internal: true,
    description: "Slide/portamento node",
    examples: [
      `impulse(2).seq(55,110,220,330)
.slide(4).sine().out()`
    ],
    ins: [
      { name: "in", default: 0 },
      { name: "rate", default: 1 }
    ],
    compile: ({ vars: [e = 0, t = 1], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var wt = a("lag", {
    ugen: "Lag",
    tags: ["fx"],
    description: "Smoothes a signal. Good for slide / portamento effects.",
    examples: [
      `impulse(2).seq(220,330,440,550)
.lag(.4).sine().out()`
    ],
    ins: [
      { name: "in", default: 0 },
      { name: "rate", default: 1, description: "60 dB lag time in seconds" }
    ],
    compile: ({ vars: [e = 0, t = 1], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var Kt = a("slew", {
    ugen: "Slew",
    tags: ["fx"],
    description: "Limits the slope of an input signal. The slope is expressed in units per second.",
    examples: ["pulse(800).slew(4000, 4000).out()"],
    ins: [
      { name: "in", default: 0 },
      {
        name: "up",
        default: 1,
        description: "Maximum upward slope in units per second"
      },
      {
        name: "dn",
        default: 1,
        description: "Maximum downward slope in units per second"
      }
    ],
    compile: ({ vars: [e = 0, t = 1, i = 1], ...n }) => s[n.lang].defUgen(n, e, t, i)
  });
  var D = a("filter", {
    ugen: "Filter",
    tags: ["fx", "filter"],
    internal: true,
    description: "Two-pole low-pass filter",
    examples: ["saw(55).lpf( sine(1).range(.4,.8) ).out()"],
    ins: [
      { name: "in", default: 0 },
      { name: "cutoff", default: 1 },
      { name: "reso", default: 0 }
    ],
    compile: ({ vars: [e = 0, t = 1, i = 0], ...n }) => s[n.lang].defUgen(n, e, t, i)
  });
  var Ct = a("fold", {
    ugen: "Fold",
    tags: ["fx", "distortion", "limiter"],
    description: 'Distort incoming audio signal by "folding"',
    examples: [
      `sine(55)
.fold( sine(.5).range(0.2,4) )
.out()`
    ],
    ins: [
      { name: "in", default: 0 },
      { name: "rate", default: 0 }
    ],
    compile: ({ vars: [e = 0, t = 0], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var vt = a("seq", {
    ugen: "Sequence",
    tags: ["sequencer"],
    description: "Trigger controlled sequencer",
    examples: [
      `impulse(2).seq(220,330,440,550)
.sine().out()`
    ],
    ins: [
      { name: "trig", default: 0 },
      { name: "step", default: 0, dynamic: true, description: "step inputs" }
      // 1-Infinity of steps
    ],
    compile: ({ vars: e, ...t }) => s[t.lang].defUgen(t, ...e)
  });
  var It = a("delay", {
    ugen: "Delay",
    tags: ["fx"],
    description: "Delay line node",
    examples: [
      `impulse(1).ad(.01,.2).mul(sine(220))
.add(x=>x.delay(.1).mul(.8)).out()`
    ],
    ins: [
      { name: "in", default: 0 },
      { name: "time", default: 0 }
    ],
    compile: ({ vars: [e = 0, t = 0], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var Ut = a("hold", {
    ugen: "Hold",
    tags: ["fx"],
    description: "Sample and hold",
    examples: [
      `noise().hold(impulse(2))
.range(220,880).sine().out()`
    ],
    ins: [
      { name: "in", default: 0 },
      { name: "trig", default: 0 }
    ],
    compile: ({ vars: [e = 0, t = 0], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var kt = a("midifreq", {
    ugen: "MidiFreq",
    tags: ["external", "midi"],
    description: "Outputs frequency of midi note in. Multiple instances will do voice allocation",
    examples: ["midifreq().sine().out()"],
    ins: [
      {
        name: "channel",
        default: -1,
        description: "Channel filter. Defaults to all channels"
      }
    ],
    compile: ({ vars: [e = -1], ...t }) => s[t.lang].defUgen(t, e)
  });
  var Pt = a("midigate", {
    ugen: "MidiGate",
    tags: ["external", "midi"],
    description: "outputs gate of midi note in. Multiple instances will do voice allocation",
    examples: ["midigate().lag(1).mul(sine(220)).out()"],
    ins: [{ name: "channel", default: -1 }],
    compile: ({ vars: [e = -1], ...t }) => s[t.lang].defUgen(t, e)
  });
  var Ft = a("midivel", {
    ugen: "MidiVel",
    tags: ["external", "midi"],
    description: "outputs velocity of midi note in. Multiple instances will do voice allocation",
    examples: [
      "midigate().ar(0.01,0.2).mul(saw(midifreq())).mul(midivel()).mul(.8).out()"
    ],
    ins: [{ name: "channel", default: -1 }],
    compile: ({ vars: [e = -1], ...t }) => s[t.lang].defUgen(t, e)
  });
  var Jt = a("midicc", {
    ugen: "MidiCC",
    tags: ["external", "midi"],
    description: "Outputs bipolar value of given midi cc number. initValue can be set to be the output before getting first cc message.",
    examples: ["midicc(74).range(100,200).sine().out()"],
    ins: [
      { name: "ccnumber", default: -1 },
      { name: "channel", default: -1 },
      { name: "initValue", default: -1 }
      // could not name it "default" because syntax error
    ],
    compile: ({ vars: [e = -1, t = -1], ...i }) => s[i.lang].defUgen(i, e, t)
  });
  var K = a("cc", {
    ugen: "CC",
    tags: ["external"],
    description: "CC control",
    ins: [
      { name: "id", default: 0 },
      { name: "value", default: 0 }
    ],
    compile: ({ vars: [e], ...t }) => s[t.lang].defUgen(t, e)
  });
  var jt = a("audioin", {
    ugen: "AudioIn",
    tags: ["source", "external"],
    description: "External Audio Input, depends on your system input",
    examples: ["audioin().add(x=>x.delay(.1).mul(.8)).out()"],
    ins: [],
    compile: (e) => s[e.lang].defUgen(e, "input")
  });
  var H = a("log", {
    tags: ["math"],
    description: "calculates the logarithm (base 10) of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].log(e))
  });
  var A = a("exp", {
    tags: ["math"],
    description: "raises e to the power of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].exp(e))
  });
  var Qt = a("pow", {
    tags: ["math"],
    description: "raises the input to the given power",
    ins: [{ name: "in" }, { name: "power" }],
    compile: ({ vars: [e = 0, t = 1], name: i, lang: n }) => s[n].def(i, s[n].pow(e, t))
  });
  var q = a("sin", {
    tags: ["math"],
    description: "calculates the sine of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].defSin(e))
  });
  var _ = a("cos", {
    tags: ["math"],
    description: "calculates the cosine of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].defCos(e))
  });
  var Bt = a("tan", {
    tags: ["math"],
    description: "calculates the tan of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].defTan(e))
  });
  var Et = a("acos", {
    tags: ["math"],
    description: "calculates the acos of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].defAcos(e))
  });
  var Ot = a("asin", {
    tags: ["math"],
    description: "calculates the asin of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].defAsin(e))
  });
  var $t = a("atan", {
    tags: ["math"],
    description: "calculates the atan of the input signal",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].defAtan(e))
  });
  var Dt = a("mul", {
    tags: ["math"],
    description: "Multiplies the given signals.",
    examples: ["sine(220).mul( sine(4).range(.25,1) ).out()"],
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, e.join(" * ") || 0)
  });
  var C = a("add", {
    tags: ["math"],
    description: "sums the given signals",
    examples: ["n([0,3,7,10]).add(60).midinote().sine().mix(2).out()"],
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, e.join(" + ") || 0)
  });
  var At = a("div", {
    tags: ["math"],
    description: "adds the given signals",
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, e.join(" / ") || 0)
  });
  var qt = a("sub", {
    tags: ["math"],
    description: "subtracts the given signals",
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, e.join(" - ") || 0)
  });
  var _t = a("mod", {
    tags: ["math"],
    description: "calculates the modulo",
    examples: ["add(x=>x.add(.003).mod(1)).out()"],
    ins: [{ name: "in" }, { name: "modulo" }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, s[i].mod(...e) || 0)
  });
  var ei = a("abs", {
    tags: ["math"],
    description: "returns the absolute value of the signal",
    ins: [{ name: "in" }],
    examples: ["sine(440).abs().out()"],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].abs(e))
  });
  var ti = a("round", {
    tags: ["math"],
    description: "Rounds the signal to the nearest integer",
    ins: [{ name: "in" }],
    examples: ["sine(440.5).round().out()"],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].round(e))
  });
  var ii = a("clamp", {
    tags: ["math"],
    description: "Clamps the signal to stay within the given range",
    ins: [{ name: "in" }, { name: "min" }, { name: "max" }],
    examples: ["sine(440.5).clamp(-.6,.6).out()"],
    compile: ({ vars: [e = 0, t = -1, i = 1], name: n, lang: l2 }) => {
      const o = s[l2].min(t, i), d2 = s[l2].max(t, i), G2 = s[l2].min(s[l2].max(e, o), d2);
      return s[l2].def(n, G2);
    }
  });
  var ni = a("floor", {
    tags: ["math"],
    description: "Rounds the signal down",
    ins: [{ name: "in" }],
    examples: ["sine(440.5).floor().out()"],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].floor(e))
  });
  var si = a("ceil", {
    tags: ["math"],
    description: "Rounds the signal up",
    ins: [{ name: "in" }],
    examples: ["sine(440.5).ceil().out()"],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].ceil(e))
  });
  var li = a("sign", {
    tags: ["math"],
    description: "Returns 1 if positive and -1 if negative. uses Math.sign",
    ins: [{ name: "in" }],
    examples: ["sine(440.5).ceil().out()"],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, s[i].sign(e))
  });
  var ai = a("min", {
    tags: ["math"],
    description: "returns the minimum of the given signals",
    examples: [
      "impulse(4).apply(x => min(x.seq(0,3,2), x.seq(0,7,0,5,0)).add(48).midinote().sine()).out()"
    ],
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, e.reduce(s[i].min) || 0)
  });
  var oi = a("max", {
    tags: ["math"],
    description: "returns the maximum of the given signals",
    examples: [
      "impulse(4).apply(x => max(x.seq(0,3,2), x.seq(0,7,0,5,0)).add(48).midinote().sine()).out()"
    ],
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, e.reduce(s[i].max) || 0)
  });
  var di = a("argmin", {
    tags: ["math"],
    description: "returns the index of the minimum of the given signals",
    examples: [
      "argmin(saw(1), saw(3), saw(5)).mul(12).add(48).midinote().sine().out()"
    ],
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(
      t,
      s[i].pair_b(
        e.map(s[i].pair_make).reduce(s[i].pair_a_min)
      ) || 0
    )
  });
  var ci = a("argmax", {
    tags: ["math"],
    description: "returns the index of the maximum of the given signals",
    examples: [
      "argmax(saw(1), saw(3), saw(5)).mul(12).add(48).midinote().sine().out()"
    ],
    ins: [{ name: "in", dynamic: true }],
    compile: ({ vars: e, name: t, lang: i }) => s[i].def(
      t,
      s[i].pair_b(
        e.map(s[i].pair_make).reduce(s[i].pair_a_max)
      ) || 0
    )
  });
  var ee = a("greater", {
    tags: ["logic"],
    description: "returns 1 if input is greater then threshold",
    ins: [{ name: "in" }, { name: "threshold" }],
    examples: [
      `greater(sine(1),0)
.bipolar().range(100,200)
.sine().out()`
    ],
    compile: ({ vars: [e = 0, t = 0], name: i, lang: n }) => s[n].def(i, `${e} > ${t}`)
  });
  var te = a("lower", {
    tags: ["logic"],
    description: "returns 1 if input is lower then threshold",
    ins: [{ name: "in" }, { name: "threshold" }],
    examples: [
      `lower(sine(1),0)
.bipolar().range(100,200)
.sine().out()`
    ],
    compile: ({ vars: [e = 0, t = 0], name: i, lang: n }) => s[n].def(i, `${e} < ${t}`)
  });
  var pi = ee;
  var ui = te;
  var mi = a("xor", {
    tags: ["logic"],
    description: "returns 1 if exactly one of the inputs is 1",
    ins: [{ name: "a" }, { name: "b" }],
    compile: ({ vars: [e = 0, t = 0], name: i, lang: n }) => s[n].def(i, `${e} != ${t} ? 1 : 0`)
  });
  var ri = a("and", {
    tags: ["logic"],
    description: "returns 1 if both inputs are 1",
    ins: [{ name: "a" }, { name: "b" }],
    compile: ({ vars: [e = 0, t = 0], name: i, lang: n }) => s[n].def(i, `${e} && ${t} ? 1 : 0`)
  });
  var hi = a("or", {
    tags: ["logic"],
    description: "returns 1 if one or both inputs are 1",
    ins: [{ name: "a" }, { name: "b" }],
    compile: ({ vars: [e = 0, t = 0], name: i, lang: n }) => s[n].def(i, `${e} || ${t} ? 1 : 0`)
  });
  var Gi = a("not", {
    tags: ["logic"],
    description: "returns 1 if input is 0, otherwise 0",
    ins: [{ name: "in" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, `(${e} === 0 ? 1 : 0)`)
  });
  var yi = a("bool", {
    tags: ["logic"],
    description: "returns 1 signal is non zero. inspired by genish",
    ins: [{ name: "a" }],
    compile: ({ vars: [e = 0], name: t, lang: i }) => s[i].def(t, `(${e} === 0 ? 0 : 1)`)
  });
  var fi = a("ifelse", {
    tags: ["logic"],
    description: "if control is 1, a is returned, otherwise b",
    ins: [{ name: "control" }, { name: "a" }, { name: "b" }],
    compile: ({ vars: [e = 0, t = 0, i = 0], name: n, lang: l2 }) => s[l2].def(n, `(${e} === 1 ? ${t} : ${i})`),
    examples: ["ifelse(pulse(1), sine(220), sine(330)).out()"]
  });
  var gi = a("range", {
    tags: ["math"],
    description: "Scales the incoming bipolar value to the given range.",
    examples: ["sine(.5).range(.25,1).mul(sine(440)).out()"],
    ins: [{ name: "in" }, { name: "min" }, { name: "max" }],
    compile: ({ vars: e, name: t, lang: i }) => {
      const [n, l2, o, d2 = 1] = e, G2 = `((${n} + 1) * 0.5)`, p = d2 === 1 ? G2 : s[i].pow(G2, d2);
      return s[i].def(t, `${p} * (${o} - ${l2}) + ${l2}`);
    }
  });
  var Xi = a("remap", {
    ugen: "Remap",
    tags: ["math"],
    description: "Remaps input from one value range to another",
    ins: [
      { name: "in" },
      { name: "inmin" },
      { name: "inmax" },
      { name: "outmin" },
      { name: "outmax" }
    ],
    // examples: [`sine(440).abs().out()`],
    compile: ({
      vars: [e = 0, t = -1, i = 1, n = -1, l2 = 1],
      ...o
    }) => s[o.lang].defUgen(o, e, t, i, n, l2)
  });
  var Zi = a("thru", {
    compile: ({ name: e, vars: t, lang: i }) => s[i].def(e, t[0], "thru")
  });
  var bi = g(
    "rangex",
    (e, t, i) => {
      let n = H(t), l2 = H(i).sub(n), d2 = e.unipolar().mul(l2).add(n);
      return A(d2);
    },
    {
      tags: ["math"],
      description: "exponential range",
      ins: [{ name: "in" }, { name: "min" }, { name: "max" }],
      examples: ["sine([1,3]).rangex(100, 2e3).sine().out()"]
    }
  );
  var xi = a("midinote", {
    compile: ({ vars: [e], name: t, lang: i }) => s[i].def(t, s[i].midinote(e)),
    tags: ["math"],
    description: "convert midi number to frequency",
    ins: [{ name: "midi" }],
    examples: [
      `impulse(4).seq(0,3,7,12).add(60)
.midinote().sine().out()`
    ]
  });
  var Ri = a("src", {
    internal: true,
    compile: ({ vars: [e = 0], name: t, lang: i, ...n }) => s[i].def(t, n.getSource(e), `read source ${e}`)
  });
  var Li = a("output", {
    internal: true,
    ugen: "Output",
    compile: ({ vars: [e, t = 0], name: i, lang: n, ...l2 }) => {
      const o = l2.getOutput(t), d2 = l2.getSource(t);
      return [
        s[n].def(o, [o, e].join(" + "), `+ output ${t}`),
        s[n].def(d2, o, `write source ${t}`)
      ].join(`
`);
    }
  });
  var ie = a("poly");
  var ne = f(Math.PI);
  var Mi = u(
    "fork",
    (e, t = 1) => ie(...Array.from({ length: t }, () => e.clone())),
    {
      ins: [{ name: "in" }, { name: "times" }],
      tags: ["multi-channel"],
      description: "split the signal into n channels",
      examples: ["dust(4).fork(2).adsr(.1).mul(sine(220)).out()"]
    }
  );
  var zi = g("perc", (e, t) => e.adsr(0, 0, 1, t), {
    tags: ["envelope"],
    description: "percussive envelope. usable with triggers or gates",
    ins: [{ name: "gate" }, { name: "release" }],
    examples: ["impulse(4).perc(.1).mul( pink() ).out()"]
  });
  var Vi = g(
    "hpf",
    (e, t, i = 0) => e.sub(e.lpf(t, i)),
    {
      ins: [{ name: "in" }, { name: "cutoff" }, { name: "reso" }],
      description: "high pass filter",
      tags: ["fx", "filter"],
      examples: ["tri([220,331,442]).mix().hpf(sine(.5).range(0,.9)).out()"]
    }
  );
  var Wi = g("lpf", D, {
    ins: [{ name: "in" }, { name: "cutoff" }, { name: "reso" }],
    description: "low pass filter",
    tags: ["fx", "filter"],
    examples: ["saw(55).lpf( sine(1).range(.4,.8) ).out()"]
  });
  var Yi = a("bpf", {
    ugen: "BPF",
    ins: [{ name: "in" }, { name: "cutoff" }, { name: "reso" }],
    description: "high pass filter",
    tags: ["fx", "filter"],
    compile: ({ vars: [e = 0, t = 1, i = 0], ...n }) => s[n.lang].defUgen(n, e, t, i)
  });
  var Ni = g("lfnoise", (e) => O().hold($(e)), {
    ins: [{ name: "freq" }],
    description: "low frequency stepped noise.",
    tags: ["regular", "noise"],
    examples: ["lfnoise(4).range(200,800).sine().out()"]
  });
  var Hi = g(
    "bipolar",
    (e) => f(e).mul(2).sub(1),
    {
      ins: [{ name: "in" }],
      description: "convert unipolar [0,1] signal to bipolar [-1,1]",
      tags: ["math"]
      // examples: [], // tbd
    }
  );
  var Si = g(
    "unipolar",
    (e) => f(e).add(1).div(2),
    {
      ins: [{ name: "in" }],
      description: "convert bipolar [-1,1] signal to unipolar [0,1]",
      tags: ["math"]
      // examples: [], // tbd
    }
  );
  var Ti = g(
    "pan",
    (e, t) => (t = f(t).add(1).mul(ne, 0.25), e.mul([_(t), q(t)])),
    {
      ins: [
        { name: "in" },
        {
          name: "pos",
          description: "bipolar position: -1 = left, 0 = center, 1 = right"
        }
      ],
      description: "pans signal to stereo position. splits signal path in 2",
      tags: ["multi-channel"],
      examples: ["sine(220).pan(sine(.25)).out()"]
    }
  );
  var wi = a("pick", {
    tags: ["multi-channel"],
    ugen: "Pick",
    description: "Pick",
    ins: [{ name: "index" }, { name: "inputs", dynamic: true }],
    description: "picks input of given index",
    examples: [
      `sine(.25).range(0,2).round()
.pick(...sine([220,330,440]).ins)
.out()`
    ],
    compile: ({ vars: e, ...t }) => s[t.lang].defUgen(t, ...e)
  });
  var Ki = a("clip", {
    tags: ["fx"],
    ugen: "Clip",
    description: "Hard limits the signal between lo and hi.",
    ins: [{ name: "input" }, { name: "lo" }, { name: "hi" }],
    compile: ({ vars: [e = 0, t = -1, i = 1], ...n }) => s[n.lang].defUgen(n, e, t, i)
  });
  var Ci = a("trig", {
    tags: ["trigger"],
    ugen: "Trig",
    description: "Emits a trigger impulse whenever the signal becomes positive. Useful to turn gates into triggers.",
    ins: [
      { name: "input", default: 0 },
      { name: "lo", default: -1 },
      { name: "hi", default: 1 }
    ],
    compile: ({ vars: [e = 0, t = -1, i = 1], ...n }) => s[n.lang].defUgen(n, e, t, i),
    examples: [
      `pulse(2)
.trig() // comment out to hear difference
.ar(.01,.2)
.mul(sine(200)).out()`
    ]
  });
  var vi = a("qf", {
    tags: ["fx", "filter"],
    ugen: "BiquadFilter",
    description: "biQuad Filter.",
    ins: [
      { name: "input", default: 0 },
      {
        name: "type",
        default: 0,
        description: "filter type: 0 = lowpass, 1 = highpass, 2 = band pass, 3 = notch, 4 = allpass, 5 = peaking, 6 = lowshelf, 7 = highshelf"
      },
      { name: "freq", default: 500, description: "filter cutoff in Hz" },
      { name: "q", default: 1, description: "q factor" },
      { name: "gain", default: 1 }
    ],
    compile: ({ vars: e, ...t }) => s[t.lang].defUgen(t, ...e),
    examples: [
      `pink()
.qf(
 impulse(.5).seq(0,1,2,3,4,5,6,7), // type
  tri(0.5).rangex(100, 8000),  // freq
 10, // Q
 1, // gain (only relevant for types 5-7)
).div(4).out();`
    ]
  });
  var Ii = u(
    "qlpf",
    (e, t, i = 10) => e.qf(0, t, i),
    {
      description: "biQuad Low Pass Filter",
      ins: [
        { name: "input" },
        { name: "freq" },
        { name: "q", description: "resonance" }
      ],
      tags: ["fx", "filter"],
      examples: [
        `pink().qlpf(
  tri(0.5).rangex(100, 8000), // cutoff freq
  10 // Q
).out()`
      ]
    }
  );
  var Ui = u(
    "qhpf",
    (e, t, i = 10) => e.qf(1, t, i),
    {
      description: "biQuad High pass filter",
      ins: [
        { name: "input" },
        { name: "freq" },
        { name: "q", description: "resonance" }
      ],
      tags: ["fx", "filter"],
      examples: [
        `pink().qhpf(
  tri(0.5).rangex(100, 8000), // cutoff freq
  10 // Q
).out()`
      ]
    }
  );
  var ki = u(
    "qbpf",
    (e, t, i = 10) => e.qf(2, t, i),
    {
      description: "biQuad Band Pass Filter",
      ins: [
        { name: "input" },
        { name: "freq" },
        { name: "q", description: "resonance" }
      ],
      tags: ["fx", "filter"],
      examples: [
        `pink().qbpf(
  tri(0.5).rangex(100, 8000), // cutoff freq
  10 // Q
).out()`
      ]
    }
  );
  var Pi = u(
    "qnf",
    (e, t, i = 10) => e.qf(3, t, i),
    {
      description: "biQuad Notch Filter",
      ins: [
        { name: "input" },
        { name: "freq" },
        { name: "q", description: "resonance" }
      ],
      tags: ["fx", "filter"],
      examples: [
        `pink().qnf(
  tri(0.5).rangex(100, 8000), // cutoff freq
  10 // Q
).out()`
      ]
    }
  );
  var Fi = u(
    "qapf",
    (e, t, i = 10) => e.qf(3, t, i),
    {
      description: "biQuad All Pass Filter",
      ins: [
        { name: "input" },
        { name: "freq" },
        { name: "q", description: "resonance" }
      ],
      tags: ["fx", "filter"],
      examples: [
        `impulse(1).qapf(
  tri(0.5).rangex(100, 8000), // cutoff freq
  10 // Q
).out()`
      ]
    }
  );
  var Ji = u(
    "split",
    (e, t) => e.type !== "poly" ? t([e]) : t(e.ins),
    {
      ins: [{ name: "input" }, { name: "fn" }],
      tags: ["multi-channel"],
      description: "apply fn to an array of signals, one for each channel in input",
      examples: ["sine([220,330,550]).split(chs => add(...chs)).out()"]
    }
  );
  var ji = u(
    "mix",
    (e, t = 1) => {
      if ([1, 2].includes(t) || (t = 2, console.warn("mix only supports 1 or 2 channels atm.. falling back to 2")), e.type !== "poly")
        return e;
      if (t === 2) {
        const i = e.ins.map((n, l2, o) => {
          const G2 = (l2 / (o.length - 1) * 2 - 1 + 1) * Math.PI / 4;
          return n.mul([Math.cos(G2), Math.sin(G2)]).inherit(e);
        });
        return C(...i);
      }
      return e.ins = e.ins.map((i) => i.inherit(e)), x("mix").withIns(...e.ins);
    },
    {
      compile: ({ vars: e, name: t, lang: i }) => s[i].def(t, `(${e.join(" + ")})`),
      description: `mixes down multiple channels. Useful to make sure you get a mono or stereo signal out at the end. 
When mixing down to 2 channels, the input channels are equally distributed over the stereo image, e.g. 3 channels are panned [-1,0,1]`,
      ins: [
        { name: "in" },
        {
          name: "channels",
          default: 1,
          description: "how many channels to mix down to. Only supports 1 and 2"
        }
      ],
      tags: ["multi-channel"],
      examples: ["sine([220,330,440]).mix(2).out()"]
    }
  );
  c.prototype.feedback = function(e) {
    return this.add(e);
  };
  var Qi = (e) => C(e);
  var Bi = f;
  var Ei = f;
  var Oi = g("mouseX", () => K("mouseX"), {
    ins: [],
    description: "X position of mouse, bipolar range",
    tags: ["external"],
    examples: ["mouseX.range(100,800).sine().out()"]
  });
  var $i = Oi();
  var Di = g("mouseY", () => K("mouseY"), {
    ins: [],
    description: "Y position of mouse, bipolar range",
    tags: ["external"],
    examples: ["mouseY.range(800,100).sine().out()"]
  });
  var Ai = Di();
  var qi = u(
    "scope",
    (e, t) => {
      let i = 1;
      e.type === "poly" && (i = e.ins.length);
      const n = getNode("scope", e, t, i);
      return n.type !== "poly" ? (n.ins.push({ type: "n", value: 0, ins: [] }), n) : (n.ins.forEach((l2, o) => l2.ins.push({ type: "n", value: o, ins: [] })), n);
    },
    {
      ugen: "Scope",
      description: "renders an an oscilloscope of the current point in the graph. expects values between -1 and 1. warning: this feature is still experimental! when using it, make sure to not switch tabs, as it might fry your browser.",
      compile: ({ vars: [e, t, i, n], ...l2 }) => w(l2, e, t, i, n)
    }
  );
  var P2 = /* @__PURE__ */ Object.freeze(/* @__PURE__ */ Object.defineProperty({
    __proto__: null,
    B: Bi,
    PI: ne,
    _: Ei,
    abs: ei,
    acos: Et,
    ad: Zt,
    add: C,
    adsr: gt,
    and: ri,
    ar: Xt,
    argmax: ci,
    argmin: di,
    asin: Ot,
    atan: $t,
    audioin: jt,
    bipolar: Hi,
    bool: yi,
    bpf: Yi,
    brown: zt,
    bytebeat: yt,
    cc: K,
    ceil: si,
    clamp: ii,
    clip: Ki,
    clock: bt,
    clockdiv: xt,
    cos: _,
    delay: It,
    distort: Rt,
    div: At,
    dust: Vt,
    exp: A,
    feedback: Qi,
    filter: D,
    floatbeat: ft,
    floor: ni,
    fold: Ct,
    fork: Mi,
    greater: ee,
    gt: pi,
    hold: Ut,
    hpf: Vi,
    ifelse: fi,
    impulse: $,
    lag: wt,
    lfnoise: Ni,
    log: H,
    lower: te,
    lpf: Wi,
    lt: ui,
    max: oi,
    midicc: Jt,
    midifreq: kt,
    midigate: Pt,
    midinote: xi,
    midivel: Ft,
    min: ai,
    mix: ji,
    mod: _t,
    mouseX: $i,
    mouseY: Ai,
    mul: Dt,
    noise: O,
    not: Gi,
    or: hi,
    output: Li,
    pan: Ti,
    perc: zi,
    pick: wi,
    pink: Mt,
    poly: ie,
    pow: Qt,
    pulse: St,
    qapf: Fi,
    qbpf: ki,
    qf: vi,
    qhpf: Ui,
    qlpf: Ii,
    qnf: Pi,
    range: gi,
    rangex: bi,
    raw: Gt,
    registerUgen: E,
    remap: Xi,
    rng: Lt,
    round: ti,
    saw: Wt,
    scope: qi,
    seq: vt,
    sign: li,
    sin: q,
    sine: Nt,
    slew: Kt,
    slide: Tt,
    split: Ji,
    src: Ri,
    sub: qt,
    tan: Bt,
    thru: Zi,
    time: ht,
    tri: Ht,
    trig: Ci,
    unipolar: Si,
    xor: mi,
    zaw: Yt
  }, Symbol.toStringTag, { value: "Module" }));
  var en = class {
    constructor({
      onToggle: t,
      onToggleRecording: i,
      beforeEval: n,
      transpiler: l2,
      localScope: o = false,
      outputNode: d2 = null
    } = {}) {
      this.outputNode = d2, this.audio = new Fe(this.outputNode), this.onToggle = t, this.transpiler = l2, this.onToggleRecording = i, this.beforeEval = n, this.localScope = o, typeof window < "u" && (o || (Object.assign(globalThis, k), Object.assign(globalThis, P2), Object.assign(globalThis, U), Object.assign(globalThis, { repl: this })), window.addEventListener("message", (p) => {
        p.data.type === "KABELSALAT_SET_CONTROL" && this.audio.setControl(p.data.id, p.data.value);
      }));
      const G2 = this;
      c.prototype.spawn = function(p = [0, 1], m3) {
        G2.audio.spawn(this.output(p).exit(), m3);
      };
    }
    registerUgen(t, i) {
      return this.audio.registerUgen(i), E(t, i.name);
    }
    evaluate(t) {
      this.localScope || (Object.assign(globalThis, { audio: this.audio }), Object.assign(globalThis, {
        addUgen: this.registerUgen.bind(this)
      }));
      let i;
      this.transpiler ? i = this.transpiler(t) : i = { output: t }, this.beforeEval?.(i);
      let n;
      return this.localScope && (n = {
        ...k,
        ...P2,
        ...U,
        audio: this.audio,
        addUgen: this.registerUgen.bind(this),
        repl: this
      }), j(i.output, n);
    }
    async play(t) {
      await this.audio.init(), t.ins.length && this.audio.spawn(t), this.onToggle?.(true);
    }
    run(t) {
      const i = this.evaluate(t);
      this.play(i);
    }
    stop() {
      this.stopRecording(), this.audio.stop(), this.onToggle?.(false);
    }
    record() {
      this.audio.record(), this.onToggleRecording?.(true);
    }
    stopRecording() {
      this.audio.stopRecording(), this.onToggleRecording?.(false);
    }
  };

  // node_modules/@strudel/core/dist/index.mjs
  var import_meta = {};
  var oe2 = "strudel.log";
  var Qe2 = 1e3;
  var Ut2;
  var Xt2;
  function zt2(t, e = "cyclist") {
    process.env.NODE_ENV === "development" && console.error(t), E2(`[${e}] error: ${t.message}`);
  }
  function E2(t, e, n = {}) {
    let s2 = performance.now();
    Ut2 === t && s2 - Xt2 < Qe2 || (Ut2 = t, Xt2 = s2, console.log(`%c${t}`, "background-color: black;color:white;border-radius:15px"), typeof document < "u" && typeof CustomEvent < "u" && document.dispatchEvent(
      new CustomEvent(oe2, {
        detail: {
          message: t,
          type: e,
          data: n
        }
      })
    ));
  }
  E2.key = oe2;
  var Yf = (t) => /^[a-gA-G][#bsf]*[0-9]*$/.test(t);
  var Mt2 = (t) => /^[a-gA-G][#bsf]*-?[0-9]*$/.test(t);
  var Ue2 = (t) => {
    if (typeof t != "string")
      return [];
    const [e, n = "", s2] = t.match(/^([a-gA-G])([#bsf]*)(-?[0-9]*)$/)?.slice(1) || [];
    return e ? [e, n, s2 ? Number(s2) : void 0] : [];
  };
  var Xe2 = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 };
  var Ke2 = { "#": 1, b: -1, s: 1, f: -1 };
  var Ye2 = (t) => t?.split("").reduce((e, n) => e + Ke2[n], 0) || 0;
  var gt2 = (t, e = 3) => {
    const [n, s2, r = e] = Ue2(t);
    if (!n)
      throw new Error('not a note: "' + t + '"');
    const o = Xe2[n.toLowerCase()], i = Ye2(s2);
    return (Number(r) + 1) * 12 + o + i;
  };
  var it2 = (t) => Math.pow(2, (t - 69) / 12) * 440;
  var Ze2 = (t) => 12 * Math.log(t / 440) / Math.LN2 + 69;
  var Zf = (t, e) => {
    if (typeof t != "object")
      throw new Error("valueToMidi: expected object value");
    let { freq: n, note: s2 } = t;
    if (typeof n == "number")
      return Ze2(n);
    if (typeof s2 == "string")
      return gt2(s2);
    if (typeof s2 == "number")
      return s2;
    if (!e)
      throw new Error("valueToMidi: expected freq or note to be set");
    return e;
  };
  var th = (t, e) => (t - e) * 1e3;
  var tn = (t) => it2(typeof t == "number" ? t : gt2(t));
  var en2 = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"];
  var eh = (t) => {
    const e = Math.floor(t / 12) - 1;
    return en2[t % 12] + e;
  };
  var bt2 = (t, e) => (t % e + e) % e;
  var nn = (t) => t.reduce((e, n) => e + n) / t.length;
  function sn(t, e = 0) {
    return isNaN(Number(t)) ? (E2(`"${t}" is not a number, falling back to ${e}`, "warning"), e) : t;
  }
  var nh = (t, e) => bt2(Math.round(sn(t ?? 0, 0)), e);
  var sh = (t) => {
    let { value: e, context: n } = t, s2 = e;
    if (typeof s2 == "object" && !Array.isArray(s2) && (s2 = s2.note || s2.n || s2.value, s2 === void 0))
      throw new Error(`cannot find a playable note for ${JSON.stringify(e)}`);
    if (typeof s2 == "number" && n.type !== "frequency")
      s2 = it2(t.value);
    else if (typeof s2 == "number" && n.type === "frequency")
      s2 = t.value;
    else if (typeof s2 != "string" || !Mt2(s2))
      throw new Error("not a note: " + JSON.stringify(s2));
    return s2;
  };
  var rh = (t) => {
    let { value: e, context: n } = t;
    if (typeof e == "object")
      return e.freq ? e.freq : tn(e.note || e.n || e.value);
    if (typeof e == "number" && n.type !== "frequency")
      e = it2(t.value);
    else if (typeof e == "string" && Mt2(e))
      e = it2(gt2(t.value));
    else if (typeof e != "number")
      throw new Error("not a note or frequency: " + e);
    return e;
  };
  var rn = (t, e) => t.slice(e).concat(t.slice(0, e));
  var on = (...t) => t.reduce(
    (e, n) => (...s2) => e(n(...s2)),
    (e) => e
  );
  var oh = (...t) => on(...t.reverse());
  var lt2 = (t) => t.filter((e) => e != null);
  var G = (t) => [].concat(...t);
  var ot2 = (t) => t;
  var ch = (t, e) => t;
  var _t2 = (t, e) => Array.from({ length: e - t + 1 }, (n, s2) => s2 + t);
  function w2(t, e, n = t.length) {
    const s2 = function r(...o) {
      if (o.length >= n)
        return t.apply(this, o);
      {
        const i = function(...a2) {
          return r.apply(this, o.concat(a2));
        };
        return e && e(i, o), i;
      }
    };
    return e && e(s2, []), s2;
  }
  function ce2(t) {
    const e = Number(t);
    if (!isNaN(e))
      return e;
    if (Mt2(t))
      return gt2(t);
    throw new Error(`cannot parse as numeral: "${t}"`);
  }
  function ie2(t, e) {
    return (...n) => t(...n.map(e));
  }
  function L2(t) {
    return ie2(t, ce2);
  }
  function cn(t) {
    const e = Number(t);
    if (!isNaN(e))
      return e;
    const n = {
      pi: Math.PI,
      w: 1,
      h: 0.5,
      q: 0.25,
      e: 0.125,
      s: 0.0625,
      t: 1 / 3,
      f: 0.2,
      x: 1 / 6
    }[t];
    if (typeof n < "u")
      return n;
    throw new Error(`cannot parse as fractional: "${t}"`);
  }
  var ih = (t) => ie2(t, cn);
  var ue2 = function(t, e) {
    return [e.slice(0, t), e.slice(t)];
  };
  var Pt2 = (t, e, n) => e.map((s2, r) => t(s2, n[r]));
  var un = function(t) {
    const e = [];
    for (let n = 0; n < t.length - 1; ++n)
      e.push([t[n], t[n + 1]]);
    return e;
  };
  var an = (t, e, n) => Math.min(Math.max(t, e), n);
  var ln = ["Do", "Reb", "Re", "Mib", "Mi", "Fa", "Solb", "Sol", "Lab", "La", "Sib", "Si"];
  var pn = [
    "Sa",
    "Re",
    "Ga",
    "Ma",
    "Pa",
    "Dha",
    "Ni"
  ];
  var fn = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Hb", "H"];
  var hn = [
    "Ni",
    "Pab",
    "Pa",
    "Voub",
    "Vou",
    "Ga",
    "Dib",
    "Di",
    "Keb",
    "Ke",
    "Zob",
    "Zo"
  ];
  var dn = [
    "I",
    "Ro",
    "Ha",
    "Ni",
    "Ho",
    "He",
    "To"
  ];
  var mn = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"];
  var uh = (t, e = "letters") => {
    const s2 = (e === "solfeggio" ? ln : e === "indian" ? pn : e === "german" ? fn : e === "byzantine" ? hn : e === "japanese" ? dn : mn)[t % 12], r = Math.floor(t / 12) - 1;
    return s2 + r;
  };
  function ah(t) {
    var e = {};
    return t.filter(function(n) {
      return e.hasOwn(n) ? false : e[n] = true;
    });
  }
  function lh(t) {
    return t.sort().filter(function(e, n, s2) {
      return !n || e != s2[n - 1];
    });
  }
  function yn(t) {
    return t.sort((e, n) => e.compare(n)).filter(function(e, n, s2) {
      return !n || e.ne(s2[n - 1]);
    });
  }
  function wn(t) {
    const e = new TextEncoder().encode(t);
    return btoa(String.fromCharCode(...e));
  }
  function gn(t) {
    const e = new Uint8Array(
      atob(t).split("").map((s2) => s2.charCodeAt(0))
    );
    return new TextDecoder().decode(e);
  }
  function ph(t) {
    return encodeURIComponent(wn(t));
  }
  function fh(t) {
    return gn(decodeURIComponent(t));
  }
  function bn(t, e) {
    return Array.isArray(t) ? t.map(e) : Object.fromEntries(Object.entries(t).map(([n, s2], r) => [n, e(s2, n, r)]));
  }
  function Kt2(t, e) {
    return t / e;
  }
  var _n = class {
    constructor({
      getTargetClockTime: e = vn,
      weight: n = 16,
      offsetDelta: s2 = 5e-3,
      checkAfterTime: r = 2,
      resetAfterTime: o = 8
    }) {
      this.offsetTime, this.timeAtPrevOffsetSample, this.prevOffsetTimes = [], this.getTargetClockTime = e, this.weight = n, this.offsetDelta = s2, this.checkAfterTime = r, this.resetAfterTime = o, this.reset = () => {
        this.prevOffsetTimes = [], this.offsetTime = null, this.timeAtPrevOffsetSample = null;
      };
    }
    calculateOffset(e) {
      const n = this.getTargetClockTime(), s2 = n - this.timeAtPrevOffsetSample, r = n - e;
      if (s2 > this.resetAfterTime && this.reset(), this.offsetTime == null && (this.offsetTime = r), this.prevOffsetTimes.push(r), this.prevOffsetTimes.length > this.weight && this.prevOffsetTimes.shift(), this.timeAtPrevOffsetSample == null || s2 > this.checkAfterTime) {
        this.timeAtPrevOffsetSample = n;
        const o = nn(this.prevOffsetTimes);
        Math.abs(o - this.offsetTime) > this.offsetDelta && (this.offsetTime = o);
      }
      return this.offsetTime;
    }
    calculateTimestamp(e, n) {
      return this.calculateOffset(e) + n;
    }
  };
  function hh() {
    return performance.now() * 1e-3;
  }
  function vn() {
    return Date.now() * 1e-3;
  }
  var kn = /* @__PURE__ */ new Map([
    ["control", "Control"],
    ["ctrl", "Control"],
    ["alt", "Alt"],
    ["shift", "Shift"],
    ["down", "ArrowDown"],
    ["up", "ArrowUp"],
    ["left", "ArrowLeft"],
    ["right", "ArrowRight"]
  ]);
  var rt2;
  function qn() {
    if (rt2 == null) {
      if (typeof window > "u")
        return;
      rt2 = {}, window.addEventListener("keydown", (t) => {
        rt2[t.key] = true;
      }), window.addEventListener("keyup", (t) => {
        rt2[t.key] = false;
      });
    }
    return { ...rt2 };
  }
  function ae(t, e = false) {
    return typeof t == "object" ? e ? JSON.stringify(t).slice(1, -1).replaceAll('"', "").replaceAll(",", " ") : JSON.stringify(t) : t;
  }
  Fraction.prototype.sam = function() {
    return this.floor();
  };
  Fraction.prototype.nextSam = function() {
    return this.sam().add(1);
  };
  Fraction.prototype.wholeCycle = function() {
    return new B2(this.sam(), this.nextSam());
  };
  Fraction.prototype.cyclePos = function() {
    return this.sub(this.sam());
  };
  Fraction.prototype.lt = function(t) {
    return this.compare(t) < 0;
  };
  Fraction.prototype.gt = function(t) {
    return this.compare(t) > 0;
  };
  Fraction.prototype.lte = function(t) {
    return this.compare(t) <= 0;
  };
  Fraction.prototype.gte = function(t) {
    return this.compare(t) >= 0;
  };
  Fraction.prototype.eq = function(t) {
    return this.compare(t) == 0;
  };
  Fraction.prototype.ne = function(t) {
    return this.compare(t) != 0;
  };
  Fraction.prototype.max = function(t) {
    return this.gt(t) ? this : t;
  };
  Fraction.prototype.maximum = function(...t) {
    return t = t.map((e) => new Fraction(e)), t.reduce((e, n) => n.max(e), this);
  };
  Fraction.prototype.min = function(t) {
    return this.lt(t) ? this : t;
  };
  Fraction.prototype.mulmaybe = function(t) {
    return t !== void 0 ? this.mul(t) : void 0;
  };
  Fraction.prototype.divmaybe = function(t) {
    return t !== void 0 ? this.div(t) : void 0;
  };
  Fraction.prototype.addmaybe = function(t) {
    return t !== void 0 ? this.add(t) : void 0;
  };
  Fraction.prototype.submaybe = function(t) {
    return t !== void 0 ? this.sub(t) : void 0;
  };
  Fraction.prototype.show = function() {
    return this.s * this.n + "/" + this.d;
  };
  Fraction.prototype.or = function(t) {
    return this.eq(0) ? t : this;
  };
  var m = (t) => Fraction(t);
  var Sn = (...t) => {
    if (t = lt2(t), t.length !== 0)
      return t.reduce((e, n) => e.gcd(n), m(1));
  };
  var Y2 = (...t) => {
    if (t = lt2(t), t.length === 0)
      return;
    const e = t.pop();
    return t.reduce(
      (n, s2) => n === void 0 || s2 === void 0 ? void 0 : n.lcm(s2),
      e
    );
  };
  var An = (t) => t instanceof Fraction;
  m._original = Fraction;
  var B2 = class _B {
    constructor(e, n) {
      this.begin = m(e), this.end = m(n);
    }
    get spanCycles() {
      const e = [];
      var n = this.begin;
      const s2 = this.end, r = s2.sam();
      if (n.equals(s2))
        return [new _B(n, s2)];
      for (; s2.gt(n); ) {
        if (n.sam().equals(r)) {
          e.push(new _B(n, this.end));
          break;
        }
        const o = n.nextSam();
        e.push(new _B(n, o)), n = o;
      }
      return e;
    }
    get duration() {
      return this.end.sub(this.begin);
    }
    cycleArc() {
      const e = this.begin.cyclePos(), n = e.add(this.duration);
      return new _B(e, n);
    }
    withTime(e) {
      return new _B(e(this.begin), e(this.end));
    }
    withEnd(e) {
      return new _B(this.begin, e(this.end));
    }
    withCycle(e) {
      const n = this.begin.sam(), s2 = n.add(e(this.begin.sub(n))), r = n.add(e(this.end.sub(n)));
      return new _B(s2, r);
    }
    intersection(e) {
      const n = this.begin.max(e.begin), s2 = this.end.min(e.end);
      if (!n.gt(s2) && !(n.equals(s2) && (n.equals(this.end) && this.begin.lt(this.end) || n.equals(e.end) && e.begin.lt(e.end))))
        return new _B(n, s2);
    }
    intersection_e(e) {
      const n = this.intersection(e);
      if (n == null)
        throw "TimeSpans do not intersect";
      return n;
    }
    midpoint() {
      return this.begin.add(this.duration.div(m(2)));
    }
    equals(e) {
      return this.begin.equals(e.begin) && this.end.equals(e.end);
    }
    show() {
      return this.begin.show() + " \u2192 " + this.end.show();
    }
  };
  var S2 = class _S {
    /*
          Event class, representing a value active during the timespan
          'part'. This might be a fragment of an event, in which case the
          timespan will be smaller than the 'whole' timespan, otherwise the
          two timespans will be the same. The 'part' must never extend outside of the
          'whole'. If the event represents a continuously changing value
          then the whole will be returned as None, in which case the given
          value will have been sampled from the point halfway between the
          start and end of the 'part' timespan.
          The context is to store a list of source code locations causing the event.
    
          The word 'Event' is more or less a reserved word in javascript, hence this
          class is named called 'Hap'.
          */
    constructor(e, n, s2, r = {}, o = false) {
      this.whole = e, this.part = n, this.value = s2, this.context = r, this.stateful = o, o && console.assert(typeof this.value == "function", "Stateful values must be functions");
    }
    get duration() {
      let e;
      return typeof this.value?.duration == "number" ? e = m(this.value.duration) : e = this.whole.end.sub(this.whole.begin), typeof this.value?.clip == "number" ? e.mul(this.value.clip) : e;
    }
    get endClipped() {
      return this.whole.begin.add(this.duration);
    }
    isActive(e) {
      return this.whole.begin <= e && this.endClipped >= e;
    }
    isInPast(e) {
      return e > this.endClipped;
    }
    isInNearPast(e, n) {
      return n - e <= this.endClipped;
    }
    isInFuture(e) {
      return e < this.whole.begin;
    }
    isInNearFuture(e, n) {
      return n < this.whole.begin && n > this.whole.begin - e;
    }
    isWithinTime(e, n) {
      return this.whole.begin <= n && this.endClipped >= e;
    }
    wholeOrPart() {
      return this.whole ? this.whole : this.part;
    }
    withSpan(e) {
      const n = this.whole ? e(this.whole) : void 0;
      return new _S(n, e(this.part), this.value, this.context);
    }
    withValue(e) {
      return new _S(this.whole, this.part, e(this.value), this.context);
    }
    hasOnset() {
      return this.whole != null && this.whole.begin.equals(this.part.begin);
    }
    hasTag(e) {
      return this.context.tags?.includes(e);
    }
    resolveState(e) {
      if (this.stateful && this.hasOnset()) {
        console.log("stateful");
        const n = this.value, [s2, r] = n(e);
        return [s2, new _S(this.whole, this.part, r, this.context, false)];
      }
      return [e, this];
    }
    spanEquals(e) {
      return this.whole == null && e.whole == null || this.whole.equals(e.whole);
    }
    equals(e) {
      return this.spanEquals(e) && this.part.equals(e.part) && // TODO would == be better ??
      this.value === e.value;
    }
    show(e = false) {
      const n = typeof this.value == "object" ? e ? JSON.stringify(this.value).slice(1, -1).replaceAll('"', "").replaceAll(",", " ") : JSON.stringify(this.value) : this.value;
      var s2 = "";
      if (this.whole == null)
        s2 = "~" + this.part.show;
      else {
        var r = this.whole.begin.equals(this.part.begin) && this.whole.end.equals(this.part.end);
        this.whole.begin.equals(this.part.begin) || (s2 = this.whole.begin.show() + " \u21DC "), r || (s2 += "("), s2 += this.part.show(), r || (s2 += ")"), this.whole.end.equals(this.part.end) || (s2 += " \u21DD " + this.whole.end.show());
      }
      return "[ " + s2 + " | " + n + " ]";
    }
    showWhole(e = false) {
      return `${this.whole == null ? "~" : this.whole.show()}: ${ae(this.value, e)}`;
    }
    combineContext(e) {
      const n = this;
      return { ...n.context, ...e.context, locations: (n.context.locations || []).concat(e.context.locations || []) };
    }
    setContext(e) {
      return new _S(this.whole, this.part, this.value, e);
    }
    ensureObjectValue() {
      if (typeof this.value != "object")
        throw new Error(
          `expected hap.value to be an object, but got "${this.value}". Hint: append .note() or .s() to the end`,
          "error"
        );
    }
  };
  var ut2 = class _ut {
    constructor(e, n = {}) {
      this.span = e, this.controls = n;
    }
    // Returns new State with different span
    setSpan(e) {
      return new _ut(e, this.controls);
    }
    withSpan(e) {
      return this.setSpan(e(this.span));
    }
    // Returns new State with added controls.
    setControls(e) {
      return new _ut(this.span, { ...this.controls, ...e });
    }
  };
  function Tn(t, e, n) {
    if (e?.value !== void 0 && Object.keys(e).length === 1)
      return E2("[warn]: Can't do arithmetic on control pattern."), t;
    const s2 = Object.keys(t).filter((r) => Object.keys(e).includes(r));
    return Object.assign({}, t, e, Object.fromEntries(s2.map((r) => [r, n(t[r], e[r])])));
  }
  w2((t, e) => t * e);
  w2((t, e) => e.map(t));
  function Cn(t, e = 60) {
    let n = 0, s2 = m(0), r = [""], o = "";
    for (; r[0].length < e; ) {
      const i = t.queryArc(n, n + 1), a2 = i.filter((h) => h.hasOnset()).map((h) => h.duration), u2 = Sn(...a2), p = u2.inverse();
      r = r.map((h) => h + "|"), o += "|";
      for (let h = 0; h < p; h++) {
        const [y2, g2] = [s2, s2.add(u2)], v = i.filter((O2) => O2.whole.begin.lte(y2) && O2.whole.end.gte(g2)), _2 = v.length - r.length;
        _2 > 0 && (r = r.concat(Array(_2).fill(o))), r = r.map((O2, A3) => {
          const I2 = v[A3];
          if (I2) {
            const P3 = I2.whole.begin.eq(y2) ? "" + I2.value : "-";
            return O2 + P3;
          }
          return O2 + ".";
        }), o += ".", s2 = s2.add(u2);
      }
      n++;
    }
    return r.join(`
`);
  }
  var le = {};
  var xn = async (...t) => {
    const e = await Promise.allSettled(t), n = e.filter((s2) => s2.status === "fulfilled").map((s2) => s2.value);
    return e.forEach((s2, r) => {
      s2.status === "rejected" && console.warn(`evalScope: module with index ${r} could not be loaded:`, s2.reason);
    }), n.forEach((s2) => {
      Object.entries(s2).forEach(([r, o]) => {
        globalThis[r] = o, le[r] = o;
      });
    }), n;
  };
  function Bn(t, e = {}) {
    const { wrapExpression: n = true, wrapAsync: s2 = true } = e;
    n && (t = `{${t}}`), s2 && (t = `(async ()=>${t})()`);
    const r = `"use strict";return (${t})`;
    return Function(r)();
  }
  var On = async (t, e, n) => {
    let s2 = {};
    if (e) {
      const i = e(t, n);
      t = i.output, s2 = i;
    }
    return { mode: "javascript", pattern: await Bn(t, { wrapExpression: !!e }), meta: s2 };
  };
  var Ct2;
  var J2 = true;
  var dh = function(t) {
    J2 = !!t;
  };
  var mh = (t) => Ct2 = t;
  var f2 = class _f2 {
    /**
     * Create a pattern. As an end user, you will most likely not create a Pattern directly.
     *
     * @param {function} query - The function that maps a `State` to an array of `Hap`.
     * @noAutocomplete
     */
    constructor(e, n = void 0) {
      __publicField(this, "polyJoin", function() {
        const e = this;
        return e.fmap((n) => n.extend(e._steps.div(n._steps))).outerJoin();
      });
      this.query = e, this._Pattern = true, this._steps = n;
    }
    get _steps() {
      return this.__steps;
    }
    set _steps(e) {
      this.__steps = e === void 0 ? void 0 : m(e);
    }
    setSteps(e) {
      return this._steps = e, this;
    }
    withSteps(e) {
      return J2 ? new _f2(this.query, this._steps === void 0 ? void 0 : e(this._steps)) : this;
    }
    get hasSteps() {
      return this._steps !== void 0;
    }
    //////////////////////////////////////////////////////////////////////
    // Haskell-style functor, applicative and monadic operations
    /**
     * Returns a new pattern, with the function applied to the value of
     * each hap. It has the alias `fmap`.
     * @synonyms fmap
     * @param {Function} func to to apply to the value
     * @returns Pattern
     * @example
     * "0 1 2".withValue(v => v + 10).log()
     */
    withValue(e) {
      const n = new _f2((s2) => this.query(s2).map((r) => r.withValue(e)));
      return n._steps = this._steps, n;
    }
    // runs func on query state
    withState(e) {
      return new _f2((n) => this.query(e(n)));
    }
    /**
     * see `withValue`
     * @noAutocomplete
     */
    fmap(e) {
      return this.withValue(e);
    }
    /**
     * Assumes 'this' is a pattern of functions, and given a function to
     * resolve wholes, applies a given pattern of values to that
     * pattern of functions.
     * @param {Function} whole_func
     * @param {Function} func
     * @noAutocomplete
     * @returns Pattern
     */
    appWhole(e, n) {
      const s2 = this, r = function(o) {
        const i = s2.query(o), a2 = n.query(o), u2 = function(p, h) {
          const y2 = p.part.intersection(h.part);
          if (y2 != null)
            return new S2(
              e(p.whole, h.whole),
              y2,
              p.value(h.value),
              h.combineContext(p)
            );
        };
        return G(
          i.map((p) => lt2(a2.map((h) => u2(p, h))))
        );
      };
      return new _f2(r);
    }
    /**
     * When this method is called on a pattern of functions, it matches its haps
     * with those in the given pattern of values.  A new pattern is returned, with
     * each matching value applied to the corresponding function.
     *
     * In this `_appBoth` variant, where timespans of the function and value haps
     * are not the same but do intersect, the resulting hap has a timespan of the
     * intersection. This applies to both the part and the whole timespan.
     * @param {Pattern} pat_val
     * @noAutocomplete
     * @returns Pattern
     */
    appBoth(e) {
      const n = this, s2 = function(o, i) {
        if (!(o == null || i == null))
          return o.intersection_e(i);
      }, r = n.appWhole(s2, e);
      return J2 && (r._steps = Y2(e._steps, n._steps)), r;
    }
    /**
     * As with `appBoth`, but the `whole` timespan is not the intersection,
     * but the timespan from the function of patterns that this method is called
     * on. In practice, this means that the pattern structure, including onsets,
     * are preserved from the pattern of functions (often referred to as the left
     * hand or inner pattern).
     * @param {Pattern} pat_val
     * @noAutocomplete
     * @returns Pattern
     */
    appLeft(e) {
      const n = this, s2 = function(o) {
        const i = [];
        for (const a2 of n.query(o)) {
          const u2 = e.query(o.setSpan(a2.wholeOrPart()));
          for (const p of u2) {
            const h = a2.whole, y2 = a2.part.intersection(p.part);
            if (y2) {
              const g2 = a2.value(p.value), v = p.combineContext(a2), _2 = new S2(h, y2, g2, v);
              i.push(_2);
            }
          }
        }
        return i;
      }, r = new _f2(s2);
      return r._steps = this._steps, r;
    }
    /**
     * As with `appLeft`, but `whole` timespans are instead taken from the
     * pattern of values, i.e. structure is preserved from the right hand/outer
     * pattern.
     * @param {Pattern} pat_val
     * @noAutocomplete
     * @returns Pattern
     */
    appRight(e) {
      const n = this, s2 = function(o) {
        const i = [];
        for (const a2 of e.query(o)) {
          const u2 = n.query(o.setSpan(a2.wholeOrPart()));
          for (const p of u2) {
            const h = a2.whole, y2 = p.part.intersection(a2.part);
            if (y2) {
              const g2 = p.value(a2.value), v = a2.combineContext(p), _2 = new S2(h, y2, g2, v);
              i.push(_2);
            }
          }
        }
        return i;
      }, r = new _f2(s2);
      return r._steps = e._steps, r;
    }
    bindWhole(e, n) {
      const s2 = this, r = function(o) {
        const i = function(u2, p) {
          return new S2(
            e(u2.whole, p.whole),
            p.part,
            p.value,
            Object.assign({}, u2.context, p.context, {
              locations: (u2.context.locations || []).concat(p.context.locations || [])
            })
          );
        }, a2 = function(u2) {
          return n(u2.value).query(o.setSpan(u2.part)).map((p) => i(u2, p));
        };
        return G(s2.query(o).map((u2) => a2(u2)));
      };
      return new _f2(r);
    }
    bind(e) {
      const n = function(s2, r) {
        if (!(s2 == null || r == null))
          return s2.intersection_e(r);
      };
      return this.bindWhole(n, e);
    }
    join() {
      return this.bind(ot2);
    }
    outerBind(e) {
      return this.bindWhole((n) => n, e).setSteps(this._steps);
    }
    outerJoin() {
      return this.outerBind(ot2);
    }
    innerBind(e) {
      return this.bindWhole((n, s2) => s2, e);
    }
    innerJoin() {
      return this.innerBind(ot2);
    }
    // Flatterns patterns of patterns, by retriggering/resetting inner patterns at onsets of outer pattern haps
    resetJoin(e = false) {
      const n = this;
      return new _f2((s2) => n.discreteOnly().query(s2).map((r) => r.value.late(e ? r.whole.begin : r.whole.begin.cyclePos()).query(s2).map(
        (o) => new S2(
          // Supports continuous haps in the inner pattern
          o.whole ? o.whole.intersection(r.whole) : void 0,
          o.part.intersection(r.part),
          o.value
        ).setContext(r.combineContext(o))
      ).filter((o) => o.part)).flat());
    }
    restartJoin() {
      return this.resetJoin(true);
    }
    // Like the other joins above, joins a pattern of patterns of values, into a flatter
    // pattern of values. In this case it takes whole cycles of the inner pattern to fit each event
    // in the outer pattern.
    squeezeJoin() {
      const e = this;
      function n(s2) {
        const r = e.discreteOnly().query(s2);
        function o(a2) {
          const p = a2.value._focusSpan(a2.wholeOrPart()).query(s2.setSpan(a2.part));
          function h(y2, g2) {
            let v;
            if (g2.whole && y2.whole && (v = g2.whole.intersection(y2.whole), !v))
              return;
            const _2 = g2.part.intersection(y2.part);
            if (!_2)
              return;
            const O2 = g2.combineContext(y2);
            return new S2(v, _2, g2.value, O2);
          }
          return p.map((y2) => h(a2, y2));
        }
        return G(r.map(o)).filter((a2) => a2);
      }
      return new _f2(n);
    }
    squeezeBind(e) {
      return this.fmap(e).squeezeJoin();
    }
    polyBind(e) {
      return this.fmap(e).polyJoin();
    }
    //////////////////////////////////////////////////////////////////////
    // Utility methods mainly for internal use
    /**
     * Query haps inside the given time span.
     *
     * @param {Fraction | number} begin from time
     * @param {Fraction | number} end to time
     * @returns Hap[]
     * @example
     * const pattern = sequence('a', ['b', 'c'])
     * const haps = pattern.queryArc(0, 1)
     * console.log(haps)
     * silence
     * @noAutocomplete
     */
    queryArc(e, n, s2 = {}) {
      try {
        return this.query(new ut2(new B2(e, n), s2));
      } catch (r) {
        return zt2(r, "query"), [];
      }
    }
    /**
     * Returns a new pattern, with queries split at cycle boundaries. This makes
     * some calculations easier to express, as all haps are then constrained to
     * happen within a cycle.
     * @returns Pattern
     * @noAutocomplete
     */
    splitQueries() {
      const e = this, n = (s2) => G(s2.span.spanCycles.map((r) => e.query(s2.setSpan(r))));
      return new _f2(n);
    }
    /**
     * Returns a new pattern, where the given function is applied to the query
     * timespan before passing it to the original pattern.
     * @param {Function} func the function to apply
     * @returns Pattern
     * @noAutocomplete
     */
    withQuerySpan(e) {
      return new _f2((n) => this.query(n.withSpan(e)));
    }
    withQuerySpanMaybe(e) {
      const n = this;
      return new _f2((s2) => {
        const r = s2.withSpan(e);
        return r.span ? n.query(r) : [];
      });
    }
    /**
     * As with `withQuerySpan`, but the function is applied to both the
     * begin and end time of the query timespan.
     * @param {Function} func the function to apply
     * @returns Pattern
     * @noAutocomplete
     */
    withQueryTime(e) {
      return new _f2((n) => this.query(n.withSpan((s2) => s2.withTime(e))));
    }
    /**
     * Similar to `withQuerySpan`, but the function is applied to the timespans
     * of all haps returned by pattern queries (both `part` timespans, and where
     * present, `whole` timespans).
     * @param {Function} func
     * @returns Pattern
     * @noAutocomplete
     */
    withHapSpan(e) {
      return new _f2((n) => this.query(n).map((s2) => s2.withSpan(e)));
    }
    /**
     * As with `withHapSpan`, but the function is applied to both the
     * begin and end time of the hap timespans.
     * @param {Function} func the function to apply
     * @returns Pattern
     * @noAutocomplete
     */
    withHapTime(e) {
      return this.withHapSpan((n) => n.withTime(e));
    }
    /**
     * Returns a new pattern with the given function applied to the list of haps returned by every query.
     * @param {Function} func
     * @returns Pattern
     * @noAutocomplete
     */
    withHaps(e) {
      const n = new _f2((s2) => e(this.query(s2), s2));
      return n._steps = this._steps, n;
    }
    /**
     * As with `withHaps`, but applies the function to every hap, rather than every list of haps.
     * @param {Function} func
     * @returns Pattern
     * @noAutocomplete
     */
    withHap(e) {
      return this.withHaps((n) => n.map(e));
    }
    /**
     * Returns a new pattern with the context field set to every hap set to the given value.
     * @param {*} context
     * @returns Pattern
     * @noAutocomplete
     */
    setContext(e) {
      return this.withHap((n) => n.setContext(e));
    }
    /**
     * Returns a new pattern with the given function applied to the context field of every hap.
     * @param {Function} func
     * @returns Pattern
     * @noAutocomplete
     */
    withContext(e) {
      const n = this.withHap((s2) => s2.setContext(e(s2.context)));
      return this.__pure !== void 0 && (n.__pure = this.__pure, n.__pure_loc = this.__pure_loc), n;
    }
    /**
     * Returns a new pattern with the context field of every hap set to an empty object.
     * @returns Pattern
     * @noAutocomplete
     */
    stripContext() {
      return this.withHap((e) => e.setContext({}));
    }
    /**
     * Returns a new pattern with the given location information added to the
     * context of every hap.
     * @param {Number} start start offset
     * @param {Number} end end offset
     * @returns Pattern
     * @noAutocomplete
     */
    withLoc(e, n) {
      const s2 = {
        start: e,
        end: n
      }, r = this.withContext((o) => {
        const i = (o.locations || []).concat([s2]);
        return { ...o, locations: i };
      });
      return this.__pure && (r.__pure = this.__pure, r.__pure_loc = s2), r;
    }
    /**
     * Returns a new Pattern, which only returns haps that meet the given test.
     * @param {Function} hap_test - a function which returns false for haps to be removed from the pattern
     * @returns Pattern
     * @example
     * s("bd*8").velocity(rand).filterHaps((h) => (h.whole.begin % 1) < h.value.velocity)
     */
    filterHaps(e) {
      return new _f2((n) => this.query(n).filter(e));
    }
    /**
     * As with `filterHaps`, but the function is applied to values
     * inside haps.
     * @param {Function} value_test
     * @returns Pattern
     * @example
     * const drums = s("bd sd bd sd")
     * kick: drums.filterValues((v) => v.s === 'bd').duck(2)
     * snare: drums.filterValues((v) => v.s === 'sd')
     * bass: s("saw!4").note("G#1").lpf(80).lpenv(4).orbit(2)
     */
    filterValues(e) {
      return new _f2((n) => this.query(n).filter((s2) => e(s2.value))).setSteps(this._steps);
    }
    /**
     * Returns a new pattern, with haps containing undefined values removed from
     * query results.
     * @returns Pattern
     * @noAutocomplete
     */
    removeUndefineds() {
      return this.filterValues((e) => e != null);
    }
    /**
     * Returns a new pattern, with all haps without onsets filtered out. A hap
     * with an onset is one with a `whole` timespan that begins at the same time
     * as its `part` timespan.
     * @returns Pattern
     * @noAutocomplete
     */
    onsetsOnly() {
      return this.filterHaps((e) => e.hasOnset());
    }
    /**
     * Returns a new pattern, with 'continuous' haps (those without 'whole'
     * timespans) removed from query results.
     * @returns Pattern
     * @noAutocomplete
     */
    discreteOnly() {
      return this.filterHaps((e) => e.whole);
    }
    /**
     * Combines adjacent haps with the same value and whole.  Only
     * intended for use in tests.
     * @noAutocomplete
     */
    defragmentHaps() {
      return this.discreteOnly().withHaps((n) => {
        const s2 = [];
        for (var r = 0; r < n.length; ++r) {
          for (var o = true, i = n[r]; o; ) {
            const p = JSON.stringify(n[r].value);
            for (var a2 = false, u2 = r + 1; u2 < n.length; u2++) {
              const h = n[u2];
              if (i.whole.equals(h.whole)) {
                if (i.part.begin.eq(h.part.end)) {
                  if (p === JSON.stringify(h.value)) {
                    i = new S2(i.whole, new B2(h.part.begin, i.part.end), i.value), n.splice(u2, 1), a2 = true;
                    break;
                  }
                } else if (h.part.begin.eq(i.part.end) && p == JSON.stringify(h.value)) {
                  i = new S2(i.whole, new B2(i.part.begin, h.part.end), i.value), n.splice(u2, 1), a2 = true;
                  break;
                }
              }
            }
            o = a2;
          }
          s2.push(i);
        }
        return s2;
      });
    }
    /**
     * Queries the pattern for the first cycle, returning Haps. Mainly of use when
     * debugging a pattern.
     * @param {Boolean} with_context - set to true, otherwise the context field
     * will be stripped from the resulting haps.
     * @returns [Hap]
     * @noAutocomplete
     */
    firstCycle(e = false) {
      var n = this;
      return e || (n = n.stripContext()), n.query(new ut2(new B2(m(0), m(1))));
    }
    /**
     * Accessor for a list of values returned by querying the first cycle.
     * @noAutocomplete
     */
    get firstCycleValues() {
      return this.firstCycle().map((e) => e.value);
    }
    /**
     * More human-readable version of the `firstCycleValues` accessor.
     * @noAutocomplete
     */
    get showFirstCycle() {
      return this.firstCycle().map(
        (e) => `${e.value}: ${e.whole.begin.toFraction()} - ${e.whole.end.toFraction()}`
      );
    }
    /**
     * Returns a new pattern, which returns haps sorted in temporal order. Mainly
     * of use when comparing two patterns for equality, in tests.
     * @returns Pattern
     * @noAutocomplete
     */
    sortHapsByPart() {
      return this.withHaps(
        (e) => e.sort(
          (n, s2) => n.part.begin.sub(s2.part.begin).or(n.part.end.sub(s2.part.end)).or(n.whole.begin.sub(s2.whole.begin).or(n.whole.end.sub(s2.whole.end)))
        )
      );
    }
    asNumber() {
      return this.fmap(ce2);
    }
    //////////////////////////////////////////////////////////////////////
    // Operators - see 'make composers' later..
    _opIn(e, n) {
      return this.fmap(n).appLeft(d(e));
    }
    _opOut(e, n) {
      return this.fmap(n).appRight(d(e));
    }
    _opMix(e, n) {
      return this.fmap(n).appBoth(d(e));
    }
    _opSqueeze(e, n) {
      const s2 = d(e);
      return this.fmap((r) => s2.fmap((o) => n(r)(o))).squeezeJoin();
    }
    _opSqueezeOut(e, n) {
      const s2 = this;
      return d(e).fmap((o) => s2.fmap((i) => n(i)(o))).squeezeJoin();
    }
    _opReset(e, n) {
      return d(e).fmap((r) => this.fmap((o) => n(o)(r))).resetJoin();
    }
    _opRestart(e, n) {
      return d(e).fmap((r) => this.fmap((o) => n(o)(r))).restartJoin();
    }
    _opPoly(e, n) {
      const s2 = d(e);
      return this.fmap((r) => s2.fmap((o) => n(o)(r))).polyJoin();
    }
    //////////////////////////////////////////////////////////////////////
    // End-user methods.
    // Those beginning with an underscore (_) are 'patternified',
    // i.e. versions are created without the underscore, that are
    // magically transformed to accept patterns for all their arguments.
    //////////////////////////////////////////////////////////////////////
    // Methods without corresponding toplevel functions
    /**
     * Layers the result of the given function(s). Like `superimpose`, but without the original pattern:
     * @name layer
     * @memberof Pattern
     * @synonyms apply
     * @returns Pattern
     * @example
     * "<0 2 4 6 ~ 4 ~ 2 0!3 ~!5>*8"
     *   .layer(x=>x.add("0,2"))
     *   .scale('C minor').note()
     */
    layer(...e) {
      return z(...e.map((n) => n(this)));
    }
    /**
     * Superimposes the result of the given function(s) on top of the original pattern:
     * @name superimpose
     * @memberof Pattern
     * @returns Pattern
     * @example
     * "<0 2 4 6 ~ 4 ~ 2 0!3 ~!5>*8"
     *   .superimpose(x=>x.add(2))
     *   .scale('C minor').note()
     */
    superimpose(...e) {
      return this.stack(...e.map((n) => n(this)));
    }
    //////////////////////////////////////////////////////////////////////
    // Multi-pattern functions
    stack(...e) {
      return z(this, ...e);
    }
    sequence(...e) {
      return Q2(this, ...e);
    }
    seq(...e) {
      return Q2(this, ...e);
    }
    cat(...e) {
      return mt2(this, ...e);
    }
    fastcat(...e) {
      return N2(this, ...e);
    }
    slowcat(...e) {
      return Z2(this, ...e);
    }
    //////////////////////////////////////////////////////////////////////
    // Context methods - ones that deal with metadata
    onTrigger(e, n = true) {
      return this.withHap(
        (s2) => s2.setContext({
          ...s2.context,
          onTrigger: (...r) => {
            s2.context.onTrigger?.(...r), e(...r);
          },
          // if dominantTrigger is set to true, the default output (webaudio) will be disabled
          // when using multiple triggers, you cannot flip this flag to false again!
          // example: x.csound('CooLSynth').log() as well as x.log().csound('CooLSynth') should work the same
          dominantTrigger: s2.context.dominantTrigger || n
        })
      );
    }
    /**
     * Writes the content of the current event to the console (visible in the side menu).
     * @name log
     * @memberof Pattern
     * @example
     * s("bd sd").log()
     */
    log(e = (s2) => `[hap] ${s2.showWhole(true)}`, n = (s2) => ({ hap: s2 })) {
      return this.onTrigger((...s2) => {
        E2(e(...s2), void 0, n(...s2));
      }, false);
    }
    /**
     * A simplified version of `log` which writes all "values" (various configurable parameters)
     * within the event to the console (visible in the side menu).
     * @name logValues
     * @memberof Pattern
     * @example
     * s("bd sd").gain("0.25 0.5 1").n("2 1 0").logValues()
     */
    logValues(e = (n) => `[hap] ${ae(n, true)}`) {
      return this.log((n) => e(n.value));
    }
    //////////////////////////////////////////////////////////////////////
    // Visualisation
    drawLine() {
      return console.log(Cn(this)), this;
    }
    //////////////////////////////////////////////////////////////////////
    // methods relating to breaking patterns into subcycles
    // Breaks a pattern into a pattern of patterns, according to the structure of the given binary pattern.
    unjoin(e, n = ot2) {
      return e.withHap(
        (s2) => s2.withValue((r) => r ? n(this.ribbon(s2.whole.begin, s2.whole.duration)) : this)
      );
    }
    /**
     * Breaks a pattern into pieces according to the structure of a given pattern.
     * True values in the given pattern cause the corresponding subcycle of the
     * source pattern to be looped, and for an (optional) given function to be
     * applied. False values result in the corresponding part of the source pattern
     * to be played unchanged.
     * @name into
     * @memberof Pattern
     * @example
     * sound("bd sd ht lt").into("1 0", hurry(2))
     */
    into(e, n) {
      return this.unjoin(e, n).innerJoin();
    }
  };
  function zn(t, e) {
    let n = [];
    return e.forEach((s2) => {
      const r = n.findIndex(([o]) => t(s2, o));
      r === -1 ? n.push([s2]) : n[r].push(s2);
    }), n;
  }
  var Mn = (t, e) => t.spanEquals(e);
  f2.prototype.collect = function() {
    return this.withHaps(
      (t) => zn(Mn, t).map((e) => new S2(e[0].whole, e[0].part, e, {}))
    );
  };
  var yh = l("arpWith", (t, e) => e.collect().fmap((n) => d(t(n))).innerJoin().withHap((n) => new S2(n.whole, n.part, n.value.value, n.combineContext(n.value))));
  var wh = l(
    "arp",
    (t, e) => e.arpWith((n) => d(t).fmap((s2) => n[s2 % n.length])),
    false
  );
  function dt2(t) {
    return !Array.isArray(t) && typeof t == "object" && !An(t);
  }
  function Pn(t, e, n) {
    return dt2(t) || dt2(e) ? (dt2(t) || (t = { value: t }), dt2(e) || (e = { value: e }), Tn(t, e, n)) : n(t, e);
  }
  (function() {
    const t = {
      set: [(n, s2) => s2],
      keep: [(n) => n],
      keepif: [(n, s2) => s2 ? n : void 0],
      // numerical functions
      /**
       *
       * Assumes a pattern of numbers. Adds the given number to each item in the pattern.
       * @name add
       * @memberof Pattern
       * @example
       * // Here, the triad 0, 2, 4 is shifted by different amounts
       * n("0 2 4".add("<0 3 4 0>")).scale("C:major")
       * // Without add, the equivalent would be:
       * // n("<[0 2 4] [3 5 7] [4 6 8] [0 2 4]>").scale("C:major")
       * @example
       * // You can also use add with notes:
       * note("c3 e3 g3".add("<0 5 7 0>"))
       * // Behind the scenes, the notes are converted to midi numbers:
       * // note("48 52 55".add("<0 5 7 0>"))
       */
      add: [L2((n, s2) => n + s2)],
      // support string concatenation
      /**
       *
       * Like add, but the given numbers are subtracted.
       * @name sub
       * @memberof Pattern
       * @example
       * n("0 2 4".sub("<0 1 2 3>")).scale("C4:minor")
       * // See add for more information.
       */
      sub: [L2((n, s2) => n - s2)],
      /**
       *
       * Multiplies each number by the given factor.
       * @name mul
       * @memberof Pattern
       * @example
       * "<1 1.5 [1.66, <2 2.33>]>*4".mul(150).freq()
       */
      mul: [L2((n, s2) => n * s2)],
      /**
       *
       * Divides each number by the given factor.
       * @name div
       * @memberof Pattern
       */
      div: [L2((n, s2) => n / s2)],
      mod: [L2(bt2)],
      pow: [L2(Math.pow)],
      log2: [L2(Math.log2)],
      band: [L2((n, s2) => n & s2)],
      bor: [L2((n, s2) => n | s2)],
      bxor: [L2((n, s2) => n ^ s2)],
      blshift: [L2((n, s2) => n << s2)],
      brshift: [L2((n, s2) => n >> s2)],
      // TODO - force numerical comparison if both look like numbers?
      lt: [(n, s2) => n < s2],
      gt: [(n, s2) => n > s2],
      lte: [(n, s2) => n <= s2],
      gte: [(n, s2) => n >= s2],
      eq: [(n, s2) => n == s2],
      eqt: [(n, s2) => n === s2],
      ne: [(n, s2) => n != s2],
      net: [(n, s2) => n !== s2],
      and: [(n, s2) => n && s2],
      or: [(n, s2) => n || s2],
      //  bitwise ops
      func: [(n, s2) => s2(n)]
    }, e = ["In", "Out", "Mix", "Squeeze", "SqueezeOut", "Reset", "Restart", "Poly"];
    for (const [n, [s2, r]] of Object.entries(t)) {
      f2.prototype["_" + n] = function(o) {
        return this.fmap((i) => s2(i, o));
      }, Object.defineProperty(f2.prototype, n, {
        // a getter that returns a function, so 'pat' can be
        // accessed by closures that are methods of that function..
        get: function() {
          const o = this, i = (...a2) => o[n].in(...a2);
          for (const a2 of e)
            i[a2.toLowerCase()] = function(...u2) {
              var p = o;
              u2 = Q2(u2), r && (p = r(p), u2 = r(u2));
              var h;
              return n === "keepif" ? (h = p["_op" + a2](u2, (y2) => (g2) => s2(y2, g2)), h = h.removeUndefineds()) : h = p["_op" + a2](u2, (y2) => (g2) => Pn(y2, g2, s2)), h;
            };
          return i.squeezein = i.squeeze, i;
        }
      });
      for (const o of e)
        f2.prototype[o.toLowerCase()] = function(...i) {
          return this.set[o.toLowerCase()](i);
        };
    }
    f2.prototype.struct = function(...n) {
      return this.keepif.out(...n);
    }, f2.prototype.structAll = function(...n) {
      return this.keep.out(...n);
    }, f2.prototype.mask = function(...n) {
      return this.keepif.in(...n);
    }, f2.prototype.maskAll = function(...n) {
      return this.keep.in(...n);
    }, f2.prototype.reset = function(...n) {
      return this.keepif.reset(...n);
    }, f2.prototype.resetAll = function(...n) {
      return this.keep.reset(...n);
    }, f2.prototype.restart = function(...n) {
      return this.keepif.restart(...n);
    }, f2.prototype.restartAll = function(...n) {
      return this.keep.restart(...n);
    };
  })();
  var gh = z;
  var bh = z;
  var _h = $t2;
  var pt2 = (t) => new f2(() => [], t);
  var q2 = pt2(1);
  var R = pt2(0);
  function C2(t) {
    function e(s2) {
      return s2.span.spanCycles.map((r) => new S2(m(r.begin).wholeCycle(), r, t));
    }
    const n = new f2(e, 1);
    return n.__pure = t, n;
  }
  function pe2(t) {
    return t instanceof f2 || t?._Pattern;
  }
  function d(t) {
    return pe2(t) ? t : Ct2 && typeof t == "string" ? Ct2(t) : C2(t);
  }
  function En(t) {
    let e = C2([]);
    for (const n of t)
      e = e.bind((s2) => n.fmap((r) => s2.concat([r])));
    return e;
  }
  function z(...t) {
    t = t.map((s2) => Array.isArray(s2) ? Q2(...s2) : d(s2));
    const e = (s2) => G(t.map((r) => r.query(s2))), n = new f2(e);
    return J2 && (n._steps = Y2(...t.map((s2) => s2._steps))), n;
  }
  function Et2(t, e) {
    if (e = e.map((o) => Array.isArray(o) ? Q2(...o) : d(o)), e.length === 0)
      return q2;
    if (e.length === 1)
      return e[0];
    const [n, ...s2] = e.map((o) => o._steps), r = J2 ? n.maximum(...s2) : void 0;
    return z(...t(r, e));
  }
  function jn(...t) {
    return Et2(
      (e, n) => n.map((s2) => s2._steps.eq(e) ? s2 : $2(s2, pt2(e.sub(s2._steps)))),
      t
    );
  }
  function Jn(...t) {
    return Et2(
      (e, n) => n.map((s2) => s2._steps.eq(e) ? s2 : $2(pt2(e.sub(s2._steps)), s2)),
      t
    );
  }
  function $n(...t) {
    return Et2(
      (e, n) => n.map((s2) => {
        if (s2._steps.eq(e))
          return s2;
        const r = pt2(e.sub(s2._steps).div(2));
        return $2(r, s2, r);
      }),
      t
    );
  }
  function vh(t, ...e) {
    const [n, ...s2] = e.map((i) => i._steps), r = n.maximum(...s2), o = {
      centre: $n,
      left: jn,
      right: Jn,
      expand: z,
      repeat: (...i) => $t2(...i).steps(r)
    };
    return t.inhabit(o).fmap((i) => i(...e)).innerJoin().setSteps(r);
  }
  function Z2(...t) {
    if (t = t.map((s2) => Array.isArray(s2) ? N2(...s2) : d(s2)), t.length == 1)
      return t[0];
    const e = function(s2) {
      const r = s2.span, o = bt2(r.begin.sam(), t.length), i = t[o];
      if (!i)
        return [];
      const a2 = r.begin.floor().sub(r.begin.div(t.length).floor());
      return i.withHapTime((u2) => u2.add(a2)).query(s2.setSpan(r.withTime((u2) => u2.sub(a2))));
    }, n = J2 ? Y2(...t.map((s2) => s2._steps)) : void 0;
    return new f2(e).splitQueries().setSteps(n);
  }
  function fe2(...t) {
    t = t.map(d);
    const e = function(n) {
      const s2 = Math.floor(n.span.begin) % t.length;
      return t[s2]?.query(n) || [];
    };
    return new f2(e).splitQueries();
  }
  function mt2(...t) {
    return Z2(...t);
  }
  function kh(...t) {
    const e = t.reduce((n, [s2]) => n + s2, 0);
    return t = t.map(([n, s2]) => [n, s2.fast(n)]), $2(...t).slow(e);
  }
  function qh(...t) {
    let e = m(0);
    for (let n of t)
      n.length == 2 && n.unshift(e), e = n[1];
    return z(
      ...t.map(
        ([n, s2, r]) => C2(d(r)).compress(m(n).div(e), m(s2).div(e))
      )
    ).slow(e).innerJoin();
  }
  function N2(...t) {
    let e = Z2(...t);
    return t.length > 1 && (e = e._fast(t.length), e._steps = t.length), t.length == 1 && t[0].__steps_source && (t._steps = t[0]._steps), e;
  }
  function Q2(...t) {
    return N2(...t);
  }
  function Nn(...t) {
    return N2(...t);
  }
  function xt2(t) {
    return Array.isArray(t) ? t.length == 0 ? [q2, 0] : t.length == 1 ? xt2(t[0]) : [N2(...t.map((e) => xt2(e)[0])), t.length] : [d(t), 1];
  }
  var Sh = w2((t, e) => d(e).mask(t));
  var Ah = w2((t, e) => d(e).struct(t));
  var Th = w2((t, e) => d(e).superimpose(...t));
  var Ch = w2((t, e) => d(e).withValue(t));
  var xh = w2((t, e) => d(e).bind(t));
  var Bh = w2((t, e) => d(e).innerBind(t));
  var Oh = w2((t, e) => d(e).outerBind(t));
  var zh = w2((t, e) => d(e).squeezeBind(t));
  var Mh = w2((t, e) => d(e).stepBind(t));
  var Ph = w2((t, e) => d(e).polyBind(t));
  var Eh = w2((t, e) => d(e).set(t));
  var jh = w2((t, e) => d(e).keep(t));
  var Jh = w2((t, e) => d(e).keepif(t));
  var $h = w2((t, e) => d(e).add(t));
  var Nh = w2((t, e) => d(e).sub(t));
  var Lh = w2((t, e) => d(e).mul(t));
  var Rh = w2((t, e) => d(e).div(t));
  var Wh = w2((t, e) => d(e).mod(t));
  var Fh = w2((t, e) => d(e).pow(t));
  var Ih = w2((t, e) => d(e).band(t));
  var Vh = w2((t, e) => d(e).bor(t));
  var Hh = w2((t, e) => d(e).bxor(t));
  var Dh = w2((t, e) => d(e).blshift(t));
  var Gh = w2((t, e) => d(e).brshift(t));
  var Qh = w2((t, e) => d(e).lt(t));
  var Uh = w2((t, e) => d(e).gt(t));
  var Xh = w2((t, e) => d(e).lte(t));
  var Kh = w2((t, e) => d(e).gte(t));
  var Yh = w2((t, e) => d(e).eq(t));
  var Zh = w2((t, e) => d(e).eqt(t));
  var td = w2((t, e) => d(e).ne(t));
  var ed = w2((t, e) => d(e).net(t));
  var nd = w2((t, e) => d(e).and(t));
  var sd = w2((t, e) => d(e).or(t));
  var rd = w2((t, e) => d(e).func(t));
  function l(t, e, n = true, s2 = false, r = (o) => o.innerJoin()) {
    if (Array.isArray(t)) {
      const u2 = {};
      for (const p of t)
        u2[p] = l(p, e, n, s2, r);
      return u2;
    }
    const o = e.length;
    var i;
    n ? i = function(...u2) {
      u2 = u2.map(d);
      const p = u2[u2.length - 1];
      let h;
      if (o === 1)
        h = e(p);
      else {
        const y2 = u2.slice(0, -1);
        if (y2.every((g2) => g2.__pure != null)) {
          const g2 = y2.map((_2) => _2.__pure), v = y2.filter((_2) => _2.__pure_loc).map((_2) => _2.__pure_loc);
          h = e(...g2, p), h = h.withContext((_2) => {
            const O2 = (_2.locations || []).concat(v);
            return { ..._2, locations: O2 };
          });
        } else {
          const [g2, ...v] = y2;
          let _2 = (...O2) => e(...O2, p);
          _2 = w2(_2, null, o - 1), h = r(v.reduce((O2, A3) => O2.appLeft(A3), g2.fmap(_2)));
        }
      }
      return s2 && (h._steps = p._steps), h;
    } : i = function(...u2) {
      u2 = u2.map(d);
      const p = e(...u2);
      return s2 && (p._steps = u2[u2.length - 1]._steps), p;
    }, f2.prototype[t] = function(...u2) {
      if (o === 2 && u2.length !== 1)
        u2 = [Q2(...u2)];
      else if (o !== u2.length + 1)
        throw new Error(`.${t}() expects ${o - 1} inputs but got ${u2.length}.`);
      return u2 = u2.map(d), i(...u2, this);
    }, o > 1 && (f2.prototype["_" + t] = function(...u2) {
      const p = e(...u2, this);
      return s2 && p.setSteps(this._steps), p;
    });
    const a2 = w2(i, null, o);
    return le[t] = a2, a2;
  }
  function et2(t, e, n = true, s2 = false, r = (o) => o.stepJoin()) {
    return l(t, e, n, s2, r);
  }
  var od = l("round", function(t) {
    return t.asNumber().fmap((e) => Math.round(e));
  });
  var cd = l("floor", function(t) {
    return t.asNumber().fmap((e) => Math.floor(e));
  });
  var id = l("ceil", function(t) {
    return t.asNumber().fmap((e) => Math.ceil(e));
  });
  var ud = l("toBipolar", function(t) {
    return t.fmap((e) => e * 2 - 1);
  });
  var ad = l("fromBipolar", function(t) {
    return t.fmap((e) => (e + 1) / 2);
  });
  var ld = l("range", function(t, e, n) {
    return n.mul(e - t).add(t);
  });
  var pd = l("rangex", function(t, e, n) {
    return n._range(Math.log(t), Math.log(e)).fmap(Math.exp);
  });
  var fd = l("range2", function(t, e, n) {
    return n.fromBipolar()._range(t, e);
  });
  var hd = l(
    "ratio",
    (t) => t.fmap((e) => Array.isArray(e) ? e.slice(1).reduce((n, s2) => n / s2, e[0]) : e)
  );
  var dd = l("compress", function(t, e, n) {
    return t = m(t), e = m(e), t.gt(e) || t.gt(1) || e.gt(1) || t.lt(0) || e.lt(0) ? q2 : n._fastGap(m(1).div(e.sub(t)))._late(t);
  });
  var { compressSpan: md, compressspan: yd } = l(["compressSpan", "compressspan"], function(t, e) {
    return e._compress(t.begin, t.end);
  });
  var { fastGap: wd, fastgap: gd } = l(["fastGap", "fastgap"], function(t, e) {
    const n = function(r) {
      const o = r.begin.sam(), i = r.begin.sub(o).mul(t).min(1), a2 = r.end.sub(o).mul(t).min(1);
      if (!(i >= 1))
        return new B2(o.add(i), o.add(a2));
    }, s2 = function(r) {
      const o = r.part.begin, i = r.part.end, a2 = o.sam(), u2 = o.sub(a2).div(t).min(1), p = i.sub(a2).div(t).min(1), h = new B2(a2.add(u2), a2.add(p)), y2 = r.whole ? new B2(
        h.begin.sub(o.sub(r.whole.begin).div(t)),
        h.end.add(r.whole.end.sub(i).div(t))
      ) : void 0;
      return new S2(y2, h, r.value, r.context);
    };
    return e.withQuerySpanMaybe(n).withHap(s2).splitQueries();
  });
  var bd = l("focus", function(t, e, n) {
    return t = m(t), e = m(e), n._early(t.sam())._fast(m(1).div(e.sub(t)))._late(t);
  });
  var { focusSpan: _d, focusspan: vd } = l(["focusSpan", "focusspan"], function(t, e) {
    return e._focus(t.begin, t.end);
  });
  var kd = l("ply", function(t, e) {
    const n = e.fmap((s2) => C2(s2)._fast(t)).squeezeJoin();
    return J2 && (n._steps = m(t).mulmaybe(e._steps)), n;
  });
  var { fast: qd, density: Sd } = l(
    ["fast", "density"],
    function(t, e) {
      return t === 0 ? q2 : (t = m(t), e.withQueryTime((s2) => s2.mul(t)).withHapTime((s2) => s2.div(t)).setSteps(e._steps));
    },
    true,
    true
  );
  var Ad = l("hurry", function(t, e) {
    return e._fast(t).mul(C2({ speed: t }));
  });
  var { slow: Td, sparsity: Cd } = l(["slow", "sparsity"], function(t, e) {
    return t === 0 ? q2 : e._fast(m(1).div(t));
  });
  var xd = l("inside", function(t, e, n) {
    return e(n._slow(t))._fast(t);
  });
  var Bd = l("outside", function(t, e, n) {
    return e(n._fast(t))._slow(t);
  });
  var Od = l("lastOf", function(t, e, n) {
    const s2 = Array(t - 1).fill(n);
    return s2.push(e(n)), fe2(...s2);
  });
  var { firstOf: zd, every: Md } = l(["firstOf", "every"], function(t, e, n) {
    const s2 = Array(t - 1).fill(n);
    return s2.unshift(e(n)), fe2(...s2);
  });
  var Pd = l("apply", function(t, e) {
    return t(e);
  });
  var Ed = l("cpm", function(t, e) {
    return e._fast(t / 60 / 1);
  });
  var jd = l(
    "early",
    function(t, e) {
      return t = m(t), e.withQueryTime((n) => n.add(t)).withHapTime((n) => n.sub(t));
    },
    true,
    true
  );
  var Ln = l(
    "late",
    function(t, e) {
      return t = m(t), e._early(m(0).sub(t));
    },
    true,
    true
  );
  var Jd = l("zoom", function(t, e, n) {
    if (e = m(e), t = m(t), t.gte(e))
      return R;
    const s2 = e.sub(t), r = J2 ? n._steps?.mulmaybe(s2) : void 0;
    return n.withQuerySpan((o) => o.withCycle((i) => i.mul(s2).add(t))).withHapSpan((o) => o.withCycle((i) => i.sub(t).div(s2))).splitQueries().setSteps(r);
  });
  var { zoomArc: $d, zoomarc: Nd } = l(["zoomArc", "zoomarc"], function(t, e) {
    return e.zoom(t.begin, t.end);
  });
  var Ld = l(
    "bite",
    (t, e, n) => e.fmap((s2) => (r) => {
      const o = m(s2).div(r).mod(1), i = o.add(m(1).div(r));
      return n.zoom(o, i);
    }).appLeft(t).squeezeJoin(),
    false
  );
  var Rd = l(
    "linger",
    function(t, e) {
      return t == 0 ? q2 : t < 0 ? e._zoom(t.add(1), 1)._slow(t) : e._zoom(0, t)._slow(t);
    },
    true,
    true
  );
  var { segment: Wd, seg: Fd } = l(["segment", "seg"], function(t, e) {
    return e.struct(C2(true)._fast(t)).setSteps(t);
  });
  var Id = l("swingBy", (t, e, n) => n.inside(e, Ln(Nn(0, t / 2))));
  var Vd = l("swing", (t, e) => e.swingBy(1 / 3, t));
  var { invert: Hd, inv: Dd } = l(
    ["invert", "inv"],
    function(t) {
      return t.fmap((e) => !e);
    },
    true,
    true
  );
  var Gd = l("when", function(t, e, n) {
    return t ? e(n) : n;
  });
  var Qd = l("off", function(t, e, n) {
    return z(n, e(n.late(t)));
  });
  var Ud = l("brak", function(t) {
    return t.when(Z2(false, true), (e) => N2(e, q2)._late(0.25));
  });
  var Rn = l(
    "rev",
    function(t) {
      const e = function(n) {
        const s2 = n.span, r = s2.begin.sam(), o = s2.begin.nextSam(), i = function(u2) {
          const p = u2.withTime((y2) => r.add(o.sub(y2))), h = p.begin;
          return p.begin = p.end, p.end = h, p;
        };
        return t.query(n.setSpan(i(s2))).map((u2) => u2.withSpan(i));
      };
      return new f2(e).splitQueries();
    },
    false,
    true
  );
  var Xd = l("revv", function(t) {
    const e = (n) => new B2(m(0).sub(n.end), m(0).sub(n.begin));
    return t.withQuerySpan(e).withHapSpan(e);
  });
  var Kd = l("pressBy", function(t, e) {
    return e.fmap((n) => C2(n).compress(t, 1)).squeezeJoin();
  });
  var Yd = l("press", function(t) {
    return t._pressBy(0.5);
  });
  f2.prototype.hush = function() {
    return q2;
  };
  var Zd = l(
    "palindrome",
    function(t) {
      return t.lastOf(2, Rn);
    },
    true,
    true
  );
  var { juxBy: tm, juxby: em } = l(["juxBy", "juxby"], function(t, e, n) {
    t /= 2;
    const s2 = function(i, a2, u2) {
      return a2 in i ? i[a2] : u2;
    }, r = n.withValue((i) => Object.assign({}, i, { pan: s2(i, "pan", 0.5) - t })), o = e(n.withValue((i) => Object.assign({}, i, { pan: s2(i, "pan", 0.5) + t })));
    return z(r, o).setSteps(J2 ? Y2(r._steps, o._steps) : void 0);
  });
  var nm = l("jux", function(t, e) {
    return e._juxBy(1, t, e);
  });
  var { echoWith: sm, echowith: rm, stutWith: om, stutwith: cm } = l(
    ["echoWith", "echowith", "stutWith", "stutwith"],
    function(t, e, n, s2) {
      return z(..._t2(0, t - 1).map((r) => n(s2.late(m(e).mul(r)), r)));
    }
  );
  var im = l("echo", function(t, e, n, s2) {
    return s2._echoWith(t, e, (r, o) => r.gain(Math.pow(n, o)));
  });
  var um = l("stut", function(t, e, n, s2) {
    return s2._echoWith(t, n, (r, o) => r.gain(Math.pow(e, o)));
  });
  var Wn = l("applyN", function(t, e, n) {
    let s2 = n;
    for (let r = 0; r < t; r++)
      s2 = e(s2);
    return s2;
  });
  var am = l(["plyWith", "plywith"], function(t, e, n) {
    const s2 = n.fmap((r) => mt2(..._t2(0, t - 1).map((o) => Wn(o, e, r)))._fast(t)).squeezeJoin();
    return J2 && (s2._steps = m(t).mulmaybe(n._steps)), s2;
  });
  var lm = l(["plyForEach", "plyforeach"], function(t, e, n) {
    const s2 = n.fmap((r) => mt2(mt2(C2(r), ..._t2(1, t - 1).map((o) => e(C2(r), o))))._fast(t)).squeezeJoin();
    return J2 && (s2._steps = m(t).mulmaybe(n._steps)), s2;
  });
  var jt2 = function(t, e, n = false) {
    return t = m(t), Z2(
      ..._t2(0, t.sub(1)).map(
        (s2) => n ? e.late(m(s2).div(t)) : e.early(m(s2).div(t))
      )
    );
  };
  var pm = l(
    "iter",
    function(t, e) {
      return jt2(t, e, false);
    },
    true,
    true
  );
  var { iterBack: fm, iterback: hm } = l(
    ["iterBack", "iterback"],
    function(t, e) {
      return jt2(t, e, true);
    },
    true,
    true
  );
  var { repeatCycles: dm } = l(
    "repeatCycles",
    function(t, e) {
      return new f2(function(n) {
        const s2 = n.span.begin.sam(), r = s2.div(t).sam(), o = s2.sub(r);
        return n = n.withSpan((i) => i.withTime((a2) => a2.sub(o))), e.query(n).map((i) => i.withSpan((a2) => a2.withTime((u2) => u2.add(o))));
      }).splitQueries();
    },
    true,
    true
  );
  var Jt2 = function(t, e, n, s2 = false, r = false) {
    const o = Array(t - 1).fill(false);
    o.unshift(true);
    const i = jt2(t, Q2(...o), !s2);
    return r || (n = n.repeatCycles(t)), n.when(i, e);
  };
  var { chunk: mm, slowchunk: ym, slowChunk: wm } = l(
    ["chunk", "slowchunk", "slowChunk"],
    function(t, e, n) {
      return Jt2(t, e, n, false, false);
    },
    true,
    true
  );
  var { chunkBack: gm, chunkback: bm } = l(
    ["chunkBack", "chunkback"],
    function(t, e, n) {
      return Jt2(t, e, n, true);
    },
    true,
    true
  );
  var { fastchunk: _m, fastChunk: vm } = l(
    ["fastchunk", "fastChunk"],
    function(t, e, n) {
      return Jt2(t, e, n, false, true);
    },
    true,
    true
  );
  var { chunkinto: km, chunkInto: qm } = l(["chunkinto", "chunkInto"], function(t, e, n) {
    return n.into(N2(true, ...Array(t - 1).fill(false))._iterback(t), e);
  });
  var { chunkbackinto: Sm, chunkBackInto: Am } = l(["chunkbackinto", "chunkBackInto"], function(t, e, n) {
    return n.into(
      N2(true, ...Array(t - 1).fill(false))._iter(t)._early(1),
      e
    );
  });
  var Tm = l(
    "bypass",
    function(t, e) {
      return t = !!parseInt(t), t ? q2 : e;
    },
    true,
    true
  );
  var { ribbon: Cm, rib: xm } = l(
    ["ribbon", "rib"],
    (t, e, n) => n.early(t).restart(C2(1).slow(e))
  );
  var Bm = l("hsla", (t, e, n, s2, r) => r.color(`hsla(${t}turn,${e * 100}%,${n * 100}%,${s2})`));
  var Om = l("hsl", (t, e, n, s2) => s2.color(`hsl(${t}turn,${e * 100}%,${n * 100}%)`));
  f2.prototype.tag = function(t) {
    return this.withContext((e) => ({ ...e, tags: (e.tags || []).concat([t]) }));
  };
  var zm = l("filter", (t, e) => e.withHaps((n) => n.filter(t)));
  var Mm = l("filterWhen", (t, e) => e.filter((n) => t(n.whole.begin)));
  var Pm = l(
    "within",
    (t, e, n, s2) => z(
      n(s2.filterWhen((r) => r.cyclePos() >= t && r.cyclePos() <= e)),
      s2.filterWhen((r) => r.cyclePos() < t || r.cyclePos() > e)
    )
  );
  f2.prototype.stepJoin = function() {
    const t = this, e = $2(...Yt2(Zt2(t.queryArc(0, 1))))._steps, n = function(s2) {
      const o = t.early(s2.span.begin.sam()).query(s2.setSpan(new B2(m(0), m(1))));
      return $2(...Yt2(Zt2(o))).query(s2);
    };
    return new f2(n, e);
  };
  f2.prototype.stepBind = function(t) {
    return this.fmap(t).stepJoin();
  };
  function Yt2(t) {
    const e = t.filter((o, i) => i.hasSteps).reduce((o, i) => o.add(i), m(0)), n = lt2(t.map((o, i) => i._steps)).reduce(
      (o, i) => o.add(i),
      m(0)
    ), s2 = e.eq(0) ? void 0 : n.div(e);
    function r(o, i) {
      return i._steps === void 0 ? [o.mulmaybe(s2), i] : [i._steps, i];
    }
    return t.map((o) => r(...o));
  }
  function Zt2(t) {
    const e = G(t.map((r) => [r.part.begin, r.part.end])), n = yn([m(0), m(1), ...e]);
    return un(n).map((r) => [
      r[1].sub(r[0]),
      z(...Fn(new B2(...r), t).map((o) => o.value.withHap((i) => i.setContext(i.combineContext(o)))))
    ]);
  }
  function Fn(t, e) {
    return lt2(e.map((n) => In(t, n)));
  }
  function In(t, e) {
    const n = t.intersection(e.part);
    if (n != null)
      return new S2(e.whole, n, e.value, e.context);
  }
  var Vn = l("pace", function(t, e) {
    return e._steps === void 0 ? e : e._steps.eq(m(0)) ? R : e._fast(m(t).div(e._steps)).setSteps(t);
  });
  function Hn(t, ...e) {
    const n = e.map((r) => xt2(r));
    if (n.length == 0)
      return q2;
    t == 0 && (t = n[0][1]);
    const s2 = [];
    for (const r of n)
      r[1] != 0 && (t == r[1] ? s2.push(r[0]) : s2.push(r[0]._fast(m(t).div(m(r[1])))));
    return z(...s2);
  }
  function $t2(...t) {
    if (Array.isArray(t[0]))
      return Hn(0, ...t);
    if (t = t.filter((s2) => s2.hasSteps), t.length == 0)
      return q2;
    const e = Y2(...t.map((s2) => s2._steps));
    if (e.eq(m(0)))
      return R;
    const n = z(...t.map((s2) => s2.pace(e)));
    return n._steps = e, n;
  }
  function $2(...t) {
    if (t.length === 0)
      return R;
    const e = (i) => Array.isArray(i) ? i : [i._steps ?? 1, i];
    if (t = t.map(e), t.find((i) => i[0] === void 0)) {
      const i = t.map((u2) => u2[0]).filter((u2) => u2 !== void 0);
      if (i.length === 0)
        return N2(...t.map((u2) => u2[1]));
      if (i.length === t.length)
        return R;
      const a2 = i.reduce((u2, p) => u2.add(p), m(0)).div(i.length);
      for (let u2 of t)
        u2[0] === void 0 && (u2[0] = a2);
    }
    if (t.length == 1)
      return d(t[0][1]).withSteps((a2) => t[0][0]);
    const n = t.map((i) => i[0]).reduce((i, a2) => i.add(a2), m(0));
    let s2 = m(0);
    const r = [];
    for (const [i, a2] of t) {
      if (m(i).eq(0))
        continue;
      const u2 = s2.add(i);
      r.push(d(a2)._compress(s2.div(n), u2.div(n))), s2 = u2;
    }
    const o = z(...r);
    return o._steps = n, o;
  }
  function Dn(...t) {
    t = t.map((r) => Array.isArray(r) ? r.map(d) : [d(r)]);
    const e = Y2(...t.map((r) => m(r.length)));
    let n = [];
    for (let r = 0; r < e; ++r)
      n.push(...t.map((o) => o.length == 0 ? q2 : o[r % o.length]));
    n = n.filter((r) => r.hasSteps && r._steps > 0);
    const s2 = n.reduce((r, o) => r.add(o._steps), m(0));
    return n = $2(...n), n._steps = s2, n;
  }
  var Gn = et2("take", function(t, e) {
    if (!e.hasSteps || e._steps.lte(0) || (t = m(t), t.eq(0)))
      return R;
    const n = t < 0;
    n && (t = t.abs());
    const s2 = t.div(e._steps);
    return s2.lte(0) ? R : s2.gte(1) ? e : n ? e.zoom(m(1).sub(s2), 1) : e.zoom(0, s2);
  });
  var Qn = et2("drop", function(t, e) {
    return e.hasSteps ? (t = m(t), t.lt(0) ? e.take(e._steps.add(t)) : e.take(m(0).sub(e._steps.sub(t)))) : R;
  });
  var Un = et2("extend", function(t, e) {
    return e.fast(t).expand(t);
  });
  var Em = et2("replicate", function(t, e) {
    return e.repeatCycles(t).fast(t).expand(t);
  });
  var Xn = et2("expand", function(t, e) {
    return e.withSteps((n) => n.mul(m(t)));
  });
  var Kn = et2("contract", function(t, e) {
    return e.withSteps((n) => n.div(m(t)));
  });
  f2.prototype.shrinklist = function(t) {
    const e = this;
    if (!e.hasSteps)
      return [e];
    let [n, s2] = Array.isArray(t) ? t : [t, e._steps];
    if (n = m(n), s2 === 0 || n === 0)
      return [e];
    const r = n > 0, o = [];
    if (r) {
      const i = m(1).div(e._steps).mul(n);
      for (let a2 = 0; a2 < s2; ++a2) {
        const u2 = i.mul(a2);
        if (u2.gt(1))
          break;
        o.push([u2, 1]);
      }
    } else {
      n = m(0).sub(n);
      const i = m(1).div(e._steps).mul(n);
      for (let a2 = 0; a2 < s2; ++a2) {
        const u2 = m(1).sub(i.mul(a2));
        if (u2.lt(0))
          break;
        o.push([m(0), u2]);
      }
    }
    return o.map((i) => e.zoom(...i));
  };
  var Yn = (t, e) => e.shrinklist(t);
  var Zn = l(
    "shrink",
    function(t, e) {
      if (!e.hasSteps)
        return R;
      const n = e.shrinklist(t), s2 = $2(...n);
      return s2._steps = n.reduce((r, o) => r.add(o._steps), m(0)), s2;
    },
    true,
    false,
    (t) => t.stepJoin()
  );
  var jm = l(
    "grow",
    function(t, e) {
      if (!e.hasSteps)
        return R;
      const n = e.shrinklist(m(0).sub(t));
      n.reverse();
      const s2 = $2(...n);
      return s2._steps = n.reduce((r, o) => r.add(o._steps), m(0)), s2;
    },
    true,
    false,
    (t) => t.stepJoin()
  );
  var ts = function(t, ...e) {
    return t.tour(...e);
  };
  f2.prototype.tour = function(...t) {
    return $2(
      ...[].concat(
        ...t.map((e, n) => [...t.slice(0, t.length - n), this, ...t.slice(t.length - n)]),
        this,
        ...t
      )
    );
  };
  var es = function(...t) {
    t = t.filter((s2) => s2.hasSteps);
    const e = Z2(...t.map((s2) => s2._slow(s2._steps))), n = Y2(...t.map((s2) => s2._steps));
    return e._fast(n).setSteps(n);
  };
  var Jm = $2;
  var ns = $2;
  var $m = $2;
  var Nm = Dn;
  var Lm = $t2;
  f2.prototype.s_polymeter = f2.prototype.polymeter;
  var Rm = Zn;
  f2.prototype.s_taper = f2.prototype.shrink;
  var Wm = Yn;
  f2.prototype.s_taperlist = f2.prototype.shrinklist;
  var Fm = Gn;
  f2.prototype.s_add = f2.prototype.take;
  var Im = Qn;
  f2.prototype.s_sub = f2.prototype.drop;
  var Vm = Xn;
  f2.prototype.s_expand = f2.prototype.expand;
  var Hm = Un;
  f2.prototype.s_extend = f2.prototype.extend;
  var Dm = Kn;
  f2.prototype.s_contract = f2.prototype.contract;
  var Gm = ts;
  f2.prototype.s_tour = f2.prototype.tour;
  var Qm = es;
  f2.prototype.s_zip = f2.prototype.zip;
  var Um = Vn;
  f2.prototype.steps = f2.prototype.pace;
  var Xm = l("chop", function(t, e) {
    const s2 = Array.from({ length: t }, (i, a2) => a2).map((i) => ({ begin: i / t, end: (i + 1) / t })), r = function(i, a2) {
      if ("begin" in i && "end" in i && i.begin !== void 0 && i.end !== void 0) {
        const u2 = i.end - i.begin;
        a2 = { begin: i.begin + a2.begin * u2, end: i.begin + a2.end * u2 };
      }
      return Object.assign({}, i, a2);
    }, o = function(i) {
      return Q2(s2.map((a2) => r(i, a2)));
    };
    return e.squeezeBind(o).setSteps(J2 ? m(t).mulmaybe(e._steps) : void 0);
  });
  var Km = l("striate", function(t, e) {
    const s2 = Array.from({ length: t }, (o, i) => i).map((o) => ({ begin: o / t, end: (o + 1) / t })), r = Z2(...s2);
    return e.set(r)._fast(t).setSteps(J2 ? m(t).mulmaybe(e._steps) : void 0);
  });
  var he2 = function(t, e, n = 0.5) {
    return e.speed(1 / t * n).unit("c").slow(t);
  };
  var ss = l(
    "slice",
    function(t, e, n) {
      return t.innerBind(
        (s2) => e.outerBind(
          (r) => n.outerBind((o) => {
            o = o instanceof Object ? o : { s: o };
            const i = Array.isArray(s2) ? s2[r] : r / s2, a2 = Array.isArray(s2) ? s2[r + 1] : (r + 1) / s2;
            return C2({ begin: i, end: a2, _slices: s2, ...o });
          })
        )
      ).setSteps(e._steps);
    },
    false
    // turns off auto-patternification
  );
  f2.prototype.onTriggerTime = function(t) {
    return this.onTrigger((e, n, s2, r) => {
      const o = r - n;
      window.setTimeout(() => {
        t(e);
      }, o * 1e3);
    }, false);
  };
  var Ym = l(
    "splice",
    function(t, e, n) {
      const s2 = ss(t, e, n);
      return new f2((r) => {
        const o = r.controls._cps || 1;
        return s2.query(r).map(
          (a2) => a2.withValue((u2) => ({
            speed: o / u2._slices / a2.whole.duration * (u2.speed || 1),
            unit: "c",
            ...u2
          }))
        );
      }).setSteps(e._steps);
    },
    false
    // turns off auto-patternification
  );
  var { loopAt: Zm, loopat: ty } = l(["loopAt", "loopat"], function(t, e) {
    const n = e._steps ? e._steps.div(t) : void 0;
    return new f2((s2) => he2(t, e, s2.controls._cps).query(s2), n);
  });
  var ey = l(
    "fit",
    (t) => t.withHaps(
      (e, n) => e.map(
        (s2) => s2.withValue((r) => {
          const o = ("end" in r ? r.end : 1) - ("begin" in r ? r.begin : 0);
          return {
            ...r,
            speed: (n.controls._cps || 1) / s2.whole.duration * o,
            unit: "c"
          };
        })
      )
    )
  );
  var { loopAtCps: ny, loopatcps: sy } = l(["loopAtCps", "loopatcps"], function(t, e, n) {
    return he2(t, n, e);
  });
  var ry = (t) => C2(1).withValue(() => d(t())).innerJoin();
  var te2 = (t) => t < 0.5 ? 1 : 1 - (t - 0.5) / 0.5;
  var rs = (t, e, n) => {
    e = d(e), t = d(t), n = d(n);
    let s2 = e.fmap((o) => ({ gain: te2(o) })), r = e.fmap((o) => ({ gain: te2(1 - o) }));
    return z(t.mul(s2), n.mul(r));
  };
  f2.prototype.xfade = function(t, e) {
    return rs(this, t, e);
  };
  var os = (t) => (e, n, s2) => {
    e = m(e).mod(n), n = m(n);
    const r = e.div(n), o = e.add(1).div(n);
    return t(s2.fmap((i) => C2(i)._compress(r, o)));
  };
  var { beat: oy } = l(
    ["beat"],
    os((t) => t.innerJoin())
  );
  var de2 = (t, e, n) => {
    n = m(n);
    const s2 = m(1).div(t.length), r = (a2) => {
      const u2 = [];
      for (const [p, h] of a2.entries())
        h && u2.push([m(p).div(a2.length), h]);
      return u2;
    }, o = Pt2(
      ([a2, u2], [p, h]) => {
        const y2 = n.mul(p - a2).add(a2), g2 = y2.add(s2);
        return new B2(y2, g2);
      },
      r(t),
      r(e)
    );
    function i(a2) {
      const u2 = a2.span.begin.sam(), p = a2.span.cycleArc(), h = [];
      for (const y2 of o) {
        const g2 = y2.intersection(p);
        g2 !== void 0 && h.push(
          new S2(
            y2.withTime((v) => v.add(u2)),
            g2.withTime((v) => v.add(u2)),
            true
          )
        );
      }
      return h;
    }
    return new f2(i).splitQueries();
  };
  var cy = (t, e, n) => (t = d(t), e = d(e), n = d(n), t.innerBind((s2) => e.innerBind((r) => n.innerBind((o) => de2(s2, r, o)))));
  var U2 = function(t) {
    const e = function(n, s2) {
      const r = d(n).fmap((o) => Array.isArray(o) ? [...o, t] : [o, 1, t]);
      return s2 ? s2.distort(r) : C2({}).distort(r);
    };
    return f2.prototype[t] = function(n) {
      return e(n, this);
    }, e;
  };
  var iy = U2("soft");
  var uy = U2("hard");
  var ay = U2("cubic");
  var ly = U2("diode");
  var py = U2("asym");
  var fy = U2("fold");
  var hy = U2("sinefold");
  var dy = U2("chebyshev");
  var me2 = (t) => {
    let n = C2(w2((...s2) => s2, null, t.length));
    for (const s2 of t) n = n.appBoth(d(s2));
    return n;
  };
  var vt2 = (t) => Array.isArray(t) ? me2(t) : d(t);
  f2.prototype.partials = function(t) {
    return this.withValue((e) => (n) => ({ ...e, partials: n })).appLeft(vt2(t));
  };
  var my = (t) => vt2(t).as("partials");
  f2.prototype.phases = function(t) {
    return this.withValue((e) => (n) => ({ ...e, phases: n })).appLeft(vt2(t));
  };
  var yy = (t) => vt2(t).as("phases");
  f2.prototype.FX = function(...t) {
    return t = t.map(d), this.withValue((e) => (n) => {
      const s2 = e.FX ?? [];
      return { ...e, FX: s2.concat(n) };
    }).appLeft(me2(t));
  };
  var cs = (t) => {
    let n = C2(w2((...s2) => s2, null, t.length));
    for (const s2 of t) n = n.appLeft(s2);
    return n;
  };
  f2.prototype.worklet = function(t, ...e) {
    return e = e.map(d), this.outerBind((n) => cs(e).withValue((s2) => {
      const r = n.workletInputs ?? [];
      return { ...n, workletSrc: t, workletInputs: r.concat(s2) };
    }));
  };
  var wy = (...t) => C2({}).worklet(...t);
  function Nt2(t) {
    let e = Array.isArray(t);
    t = e ? t : [t];
    const n = t[0], s2 = (o) => {
      let i;
      if (typeof o == "object" && o.value !== void 0 && (i = { ...o }, o = o.value, delete i.value), e && Array.isArray(o)) {
        const a2 = i || {};
        return o.forEach((u2, p) => {
          p < t.length && (a2[t[p]] = u2);
        }), a2;
      } else return i ? (i[n] = o, i) : { [n]: o };
    }, r = function(o, i) {
      return i ? typeof o > "u" ? i.fmap(s2) : i.set(d(o).withValue(s2)) : d(o).withValue(s2);
    };
    return f2.prototype[n] = function(o) {
      return r(o, this);
    }, r;
  }
  var at2 = /* @__PURE__ */ new Map();
  function is(t) {
    return at2.has(t);
  }
  function c2(t, ...e) {
    const n = Array.isArray(t) ? t[0] : t;
    let s2 = {};
    return s2[n] = Nt2(t), at2.set(n, n), e.forEach((r) => {
      s2[r] = s2[n], at2.set(r, n), f2.prototype[r] = f2.prototype[n];
    }), s2;
  }
  function V2(t, e, ...n) {
    t = Array.isArray(t) ? t : [t];
    let s2 = {};
    for (let r = 1; r <= e; r++) {
      let o = [...n], i = [...t];
      if (r === 1) {
        const u2 = o.map((h) => `${h}1`), p = i.map((h) => `${h}1`);
        o = o.concat(u2).concat(p);
      } else
        o = o.map((u2) => `${u2}${r}`), i = i.map((u2) => `${u2}${r}`);
      const a2 = c2(i, ...o);
      s2 = { ...s2, ...a2 };
    }
    return s2;
  }
  var { s: us, sound: as } = c2(["s", "n", "gain"], "sound");
  var { wt: ls, wavetablePosition: ps } = c2("wt", "wavetablePosition");
  var { wtenv: fs } = c2("wtenv");
  var { wtattack: hs, wtatt: ds } = c2("wtattack", "wtatt");
  var { wtdecay: ms, wtdec: ys } = c2("wtdecay", "wtdec");
  var { wtsustain: ws, wtsus: gs } = c2("wtsustain", "wtsus");
  var { wtrelease: bs, wtrel: _s } = c2("wtrelease", "wtrel");
  var { wtrate: vs } = c2("wtrate");
  var { wtsync: ks } = c2("wtsync");
  var { wtdepth: qs } = c2("wtdepth");
  var { wtshape: Ss } = c2("wtshape");
  var { wtdc: As } = c2("wtdc");
  var { wtskew: Ts } = c2("wtskew");
  var { warp: Cs, wavetableWarp: xs } = c2("warp", "wavetableWarp");
  var { warpattack: Bs, warpatt: Os } = c2("warpattack", "warpatt");
  var { warpdecay: zs, warpdec: Ms } = c2("warpdecay", "warpdec");
  var { warpsustain: Ps, warpsus: Es } = c2("warpsustain", "warpsus");
  var { warprelease: js, warprel: Js } = c2("warprelease", "warprel");
  var { warprate: $s } = c2("warprate");
  var { warpdepth: Ns } = c2("warpdepth");
  var { warpshape: Ls } = c2("warpshape");
  var { warpdc: Rs } = c2("warpdc");
  var { warpskew: Ws } = c2("warpskew");
  var { warpmode: Fs, wavetableWarpMode: Is } = c2("warpmode", "wavetableWarpMode");
  var { wtphaserand: Vs, wavetablePhaseRand: Hs } = c2("wtphaserand", "wavetablePhaseRand");
  var { warpenv: Ds } = c2("warpenv");
  var { warpsync: Gs } = c2("warpsync");
  var { source: Qs, src: Us } = c2("source", "src");
  var { n: Xs } = c2("n");
  var { note: Ks } = c2(["note", "n"]);
  var { accelerate: Ys } = c2("accelerate");
  var { velocity: Zs, vel: tr } = c2("velocity", "vel");
  var { gain: er } = c2("gain");
  var { postgain: nr } = c2("postgain");
  var { amp: sr } = c2("amp");
  var { attack: rr, att: or } = c2("attack", "att");
  var { fmh: cr, fmh1: ir, fmh2: ur, fmh3: ar, fmh4: lr, fmh5: pr, fmh6: fr, fmh7: hr, fmh8: dr } = V2(["fmh", "fmi"], 8, "fmh");
  var { fmi: mr, fmi1: yr, fmi2: wr, fmi3: gr, fmi4: br, fmi5: _r, fmi6: vr, fmi7: kr, fmi8: qr, fm: Sr, fm1: Ar, fm2: Tr, fm3: Cr, fm4: xr, fm5: Br, fm6: Or, fm7: zr, fm8: Mr } = V2(["fmi", "fmh"], 8, "fm");
  var { fmenv: Pr, fmenv1: Er, fmenv2: jr, fmenv3: Jr, fmenv4: $r, fmenv5: Nr, fmenv6: Lr, fmenv7: Rr, fmenv8: Wr } = V2(
    "fmenv",
    8
  );
  var {
    fmattack: Fr,
    fmattack1: Ir,
    fmattack2: Vr,
    fmattack3: Hr,
    fmattack4: Dr,
    fmattack5: Gr,
    fmattack6: Qr,
    fmattack7: Ur,
    fmattack8: Xr,
    fmatt: Kr,
    fmatt1: Yr,
    fmatt2: Zr,
    fmatt3: to,
    fmatt4: eo,
    fmatt5: no,
    fmatt6: so,
    fmatt7: ro,
    fmatt8: oo
  } = V2("fmattack", 8, "fmatt");
  var { fmwave: co, fmwave1: io, fmwave2: uo, fmwave3: ao, fmwave4: lo, fmwave5: po, fmwave6: fo, fmwave7: ho, fmwave8: mo } = V2(
    "fmwave",
    8
  );
  var {
    fmdecay: yo,
    fmdecay1: wo,
    fmdecay2: go,
    fmdecay3: bo,
    fmdecay4: _o,
    fmdecay5: vo,
    fmdecay6: ko,
    fmdecay7: qo,
    fmdecay8: So,
    fmdec: Ao,
    fmdec1: To,
    fmdec2: Co,
    fmdec3: xo,
    fmdec4: Bo,
    fmdec5: Oo,
    fmdec6: zo,
    fmdec7: Mo,
    fmdec8: Po
  } = V2("fmdecay", 8, "fmdec");
  var {
    fmsustain: Eo,
    fmsustain1: jo,
    fmsustain2: Jo,
    fmsustain3: $o,
    fmsustain4: No,
    fmsustain5: Lo,
    fmsustain6: Ro,
    fmsustain7: Wo,
    fmsustain8: Fo,
    fmsus: Io,
    fmsus1: Vo,
    fmsus2: Ho,
    fmsus3: Do,
    fmsus4: Go,
    fmsus5: Qo,
    fmsus6: Uo,
    fmsus7: Xo,
    fmsus8: Ko
  } = V2("fmsustain", 8, "fmsus");
  var {
    fmrelease: Yo,
    fmrelease1: Zo,
    fmrelease2: tc,
    fmrelease3: ec,
    fmrelease4: nc,
    fmrelease5: sc,
    fmrelease6: rc,
    fmrelease7: oc,
    fmrelease8: cc,
    fmrel: ic,
    fmrel1: uc,
    fmrel2: ac,
    fmrel3: lc,
    fmrel4: pc,
    fmrel5: fc,
    fmrel6: hc,
    fmrel7: dc,
    fmrel8: mc
  } = V2("fmrelease", 8, "fmrel");
  for (let t = 0; t <= 8; t++)
    for (let e = 0; e <= 8; e++)
      c2(`fmi${t}${e}`, `fm${t}${e}`);
  var { bank: yc } = c2("bank");
  var { chorus: wc } = c2("chorus");
  var { analyze: gc } = c2("analyze");
  var { fft: bc } = c2("fft");
  var { decay: _c, dec: vc } = c2("decay", "dec");
  var { sustain: kc, sus: qc } = c2("sustain", "sus");
  var { release: Sc, rel: Ac } = c2("release", "rel");
  var { hold: Tc } = c2("hold");
  var { bandf: Cc, bpf: xc, bp: Bc } = c2(["bandf", "bandq", "bpenv"], "bpf", "bp");
  var { bandq: Oc, bpq: zc } = c2("bandq", "bpq");
  var { begin: Mc } = c2("begin");
  var { end: Pc } = c2("end");
  var { loop: Ec } = c2("loop");
  var { loopBegin: jc, loopb: Jc } = c2("loopBegin", "loopb");
  var { loopEnd: $c, loope: Nc } = c2("loopEnd", "loope");
  var { crush: Lc } = c2("crush");
  var { coarse: Rc } = c2("coarse");
  var { tremolo: Wc, trem: Fc } = c2(["tremolo", "tremolodepth", "tremoloskew", "tremolophase"], "trem");
  var { tremolosync: Ic } = c2(
    ["tremolosync", "tremolodepth", "tremoloskew", "tremolophase"],
    "tremsync"
  );
  var { tremolodepth: Vc } = c2("tremolodepth", "tremdepth");
  var { tremoloskew: Hc } = c2("tremoloskew", "tremskew");
  var { tremolophase: Dc } = c2("tremolophase", "tremphase");
  var { tremoloshape: Gc } = c2("tremoloshape", "tremshape");
  var { drive: Qc } = c2("drive");
  var { duck: Uc } = c2("duckorbit", "duck");
  var { duckdepth: Xc } = c2("duckdepth");
  var { duckonset: Kc } = c2("duckonset", "duckons");
  var { duckattack: Yc } = c2("duckattack", "duckatt");
  var { byteBeatExpression: Zc, bbexpr: ti2 } = c2("byteBeatExpression", "bbexpr");
  var { byteBeatStartTime: ei2, bbst: ni2 } = c2("byteBeatStartTime", "bbst");
  var { channels: si2, ch: ri2 } = c2("channels", "ch");
  var { pw: oi2 } = c2(["pw", "pwrate", "pwsweep"]);
  var { pwrate: ci2 } = c2("pwrate");
  var { pwsweep: ii2 } = c2("pwsweep");
  var { phaserrate: ui2, ph: ai2, phaser: li2 } = c2(
    ["phaserrate", "phaserdepth", "phasercenter", "phasersweep"],
    "ph",
    "phaser"
  );
  var { phasersweep: pi2, phs: fi2 } = c2("phasersweep", "phs");
  var { phasercenter: hi2, phc: di2 } = c2("phasercenter", "phc");
  var { phaserdepth: mi2, phd: yi2, phasdp: wi2 } = c2("phaserdepth", "phd", "phasdp");
  var { channel: gi2 } = c2("channel");
  var { cut: bi2 } = c2("cut");
  var { cutoff: _i2, ctf: vi2, lpf: ki2, lp: qi2 } = c2(["cutoff", "resonance", "lpenv"], "ctf", "lpf", "lp");
  var { lpenv: Si2, lpe: Ai2 } = c2("lpenv", "lpe");
  var { hpenv: Ti2, hpe: Ci2 } = c2("hpenv", "hpe");
  var { bpenv: xi2, bpe: Bi2 } = c2("bpenv", "bpe");
  var { lpattack: Oi2, lpa: zi2 } = c2("lpattack", "lpa");
  var { hpattack: Mi2, hpa: Pi2 } = c2("hpattack", "hpa");
  var { bpattack: Ei2, bpa: ji2 } = c2("bpattack", "bpa");
  var { lpdecay: Ji2, lpd: $i2 } = c2("lpdecay", "lpd");
  var { hpdecay: Ni2, hpd: Li2 } = c2("hpdecay", "hpd");
  var { bpdecay: Ri2, bpd: Wi2 } = c2("bpdecay", "bpd");
  var { lpsustain: Fi2, lps: Ii2 } = c2("lpsustain", "lps");
  var { hpsustain: Vi2, hps: Hi2 } = c2("hpsustain", "hps");
  var { bpsustain: Di2, bps: Gi2 } = c2("bpsustain", "bps");
  var { lprelease: Qi2, lpr: Ui2 } = c2("lprelease", "lpr");
  var { hprelease: Xi2, hpr: Ki2 } = c2("hprelease", "hpr");
  var { bprelease: Yi2, bpr: Zi2 } = c2("bprelease", "bpr");
  var { ftype: tu } = c2("ftype");
  var { fanchor: eu } = c2("fanchor");
  var { lprate: nu } = c2("lprate");
  var { lpsync: su } = c2("lpsync");
  var { lpdepth: ru } = c2("lpdepth");
  var { lpdepthfrequency: ou, lpdepthfreq: cu } = c2("lpdepthfrequency", "lpdepthfreq");
  var { lpshape: iu } = c2("lpshape");
  var { lpdc: uu } = c2("lpdc");
  var { lpskew: au } = c2("lpskew");
  var { bprate: lu } = c2("bprate");
  var { bpsync: pu } = c2("bpsync");
  var { bpdepth: fu } = c2("bpdepth");
  var { bpdepthfrequency: hu, bpdepthfreq: du } = c2("bpdepthfrequency", "bpdepthfreq");
  var { bpshape: mu } = c2("bpshape");
  var { bpdc: yu } = c2("bpdc");
  var { bpskew: wu } = c2("bpskew");
  var { hprate: gu } = c2("hprate");
  var { hpsync: bu } = c2("hpsync");
  var { hpdepth: _u } = c2("hpdepth");
  var { hpdepthfrequency: vu, hpdepthfreq: ku } = c2("hpdepthfrequency", "hpdepthfreq");
  var { hpshape: qu } = c2("hpshape");
  var { hpdc: Su } = c2("hpdc");
  var { hpskew: Au } = c2("hpskew");
  var { vib: Tu, vibrato: Cu, v: xu } = c2(["vib", "vibmod"], "vibrato", "v");
  var { noise: Bu } = c2("noise");
  var { vibmod: Ou, vmod: zu } = c2(["vibmod", "vib"], "vmod");
  var { hcutoff: Mu, hpf: Pu, hp: Eu } = c2(["hcutoff", "hresonance", "hpenv"], "hpf", "hp");
  var { hresonance: ju, hpq: Ju } = c2("hresonance", "hpq");
  var { resonance: $u, lpq: Nu } = c2("resonance", "lpq");
  var { djf: Lu } = c2("djf");
  var { delay: Ru } = c2(["delay", "delaytime", "delayfeedback"]);
  var { delayfeedback: Wu, delayfb: Fu, dfb: Iu } = c2("delayfeedback", "delayfb", "dfb");
  var { delayspeed: Vu } = c2("delayspeed");
  var { delaytime: Hu, delayt: Du, dt: Gu } = c2("delaytime", "delayt", "dt");
  var { delaysync: Qu } = c2("delaysync");
  var { lock: Uu } = c2("lock");
  var { detune: Xu, det: Ku } = c2("detune", "det");
  var { unison: Yu } = c2("unison");
  var { spread: Zu } = c2("spread");
  var { dry: ta } = c2("dry");
  var { fadeTime: ea, fadeOutTime: na } = c2("fadeTime", "fadeOutTime");
  var { fadeInTime: sa } = c2("fadeInTime");
  var { freq: ra } = c2("freq");
  var { pattack: oa, patt: ca } = c2("pattack", "patt");
  var { pdecay: ia, pdec: ua } = c2("pdecay", "pdec");
  var { psustain: aa, psus: la } = c2("psustain", "psus");
  var { prelease: pa, prel: fa } = c2("prelease", "prel");
  var { penv: ha } = c2("penv");
  var { pcurve: da } = c2("pcurve");
  var { panchor: ma } = c2("panchor");
  var { gate: ya, gat: wa } = c2("gate", "gat");
  var { leslie: ga } = c2("leslie");
  var { lrate: ba } = c2("lrate");
  var { lsize: _a } = c2("lsize");
  var { activeLabel: va } = c2("activeLabel");
  var { label: ka } = c2(["label", "activeLabel"]);
  var { degree: qa } = c2("degree");
  var { mtranspose: Sa } = c2("mtranspose");
  var { ctranspose: Aa } = c2("ctranspose");
  var { harmonic: Ta } = c2("harmonic");
  var { stepsPerOctave: Ca } = c2("stepsPerOctave");
  var { octaveR: xa } = c2("octaveR");
  var { nudge: Ba } = c2("nudge");
  var { octave: Oa, oct: za } = c2("octave", "oct");
  var { orbit: Ma } = c2("orbit", "o");
  var { bus: Pa } = c2("bus");
  var { busgain: Ea, bgain: ja } = c2("busgain", "bgain");
  var { overgain: Ja } = c2("overgain");
  var { overshape: $a } = c2("overshape");
  var { pan: Na } = c2("pan");
  var { panspan: La } = c2("panspan");
  var { pansplay: Ra } = c2("pansplay");
  var { panwidth: Wa } = c2("panwidth");
  var { panorient: Fa } = c2("panorient");
  var { slide: Ia } = c2("slide");
  var { semitone: Va } = c2("semitone");
  var { voice: Ha } = c2("voice");
  var { chord: Da } = c2("chord");
  var { dictionary: Ga, dict: Qa } = c2("dictionary", "dict");
  var { anchor: Ua } = c2("anchor");
  var { offset: Xa } = c2("offset");
  var { octaves: Ka } = c2("octaves");
  var { mode: Ya } = c2(["mode", "anchor"]);
  var { room: Za } = c2(["room", "size"]);
  var { roomlp: tl, rlp: el } = c2("roomlp", "rlp");
  var { roomdim: nl, rdim: sl } = c2("roomdim", "rdim");
  var { roomfade: rl, rfade: ol } = c2("roomfade", "rfade");
  var { ir: cl, iresponse: il } = c2(["ir", "i"], "iresponse");
  var { irspeed: ul } = c2("irspeed");
  var { irbegin: al } = c2("irbegin");
  var { roomsize: ll, size: pl, sz: fl, rsize: hl } = c2("roomsize", "size", "sz", "rsize");
  var { shape: dl } = c2(["shape", "shapevol"]);
  var { distort: ml, dist: yl } = c2(["distort", "distortvol", "distorttype"], "dist");
  var { distortvol: wl } = c2("distortvol", "distvol");
  var { distorttype: gl } = c2("distorttype", "disttype");
  var { compressor: bl } = c2([
    "compressor",
    "compressorRatio",
    "compressorKnee",
    "compressorAttack",
    "compressorRelease"
  ]);
  var { compressorKnee: _l } = c2("compressorKnee");
  var { compressorRatio: vl } = c2("compressorRatio");
  var { compressorAttack: kl } = c2("compressorAttack");
  var { compressorRelease: ql } = c2("compressorRelease");
  var { speed: ye2 } = c2("speed");
  var { stretch: Sl } = c2("stretch");
  var { unit: Al } = c2("unit");
  var { squiz: Tl } = c2("squiz");
  var { vowel: Cl } = c2("vowel");
  var { waveloss: xl } = c2("waveloss");
  var { density: Bl } = c2("density");
  var { expression: Ol } = c2("expression");
  var { sustainpedal: zl } = c2("sustainpedal");
  var { fshift: Ml } = c2("fshift");
  var { fshiftnote: Pl } = c2("fshiftnote");
  var { fshiftphase: El } = c2("fshiftphase");
  var { triode: jl } = c2("triode");
  var { krush: Jl } = c2("krush");
  var { kcutoff: $l } = c2("kcutoff");
  var { octer: Nl } = c2("octer");
  var { octersub: Ll } = c2("octersub");
  var { octersubsub: Rl } = c2("octersubsub");
  var { ring: Wl } = c2("ring");
  var { ringf: Fl } = c2("ringf");
  var { ringdf: Il } = c2("ringdf");
  var { freeze: Vl } = c2("freeze");
  var { xsdelay: Hl } = c2("xsdelay");
  var { tsdelay: Dl } = c2("tsdelay");
  var { real: Gl } = c2("real");
  var { imag: Ql } = c2("imag");
  var { enhance: Ul } = c2("enhance");
  var { comb: Xl } = c2("comb");
  var { smear: Kl } = c2("smear");
  var { scram: Yl } = c2("scram");
  var { binshift: Zl } = c2("binshift");
  var { hbrick: tp } = c2("hbrick");
  var { lbrick: ep } = c2("lbrick");
  var { frameRate: np } = c2("frameRate");
  var { frames: sp } = c2("frames");
  var { hours: rp } = c2("hours");
  var { minutes: op } = c2("minutes");
  var { seconds: cp } = c2("seconds");
  var { songPtr: ip } = c2("songPtr");
  var { uid: up } = c2("uid");
  var { val: ap } = c2("val");
  var { cps: lp } = c2("cps");
  var { clip: pp, legato: fp } = c2("clip", "legato");
  var { duration: hp, dur: dp } = c2("duration", "dur");
  var { zrand: mp } = c2("zrand");
  var { curve: yp } = c2("curve");
  var { deltaSlide: wp } = c2("deltaSlide");
  var { pitchJump: gp } = c2("pitchJump");
  var { pitchJumpTime: bp } = c2("pitchJumpTime");
  var { znoise: _p } = c2("znoise");
  var { zmod: vp } = c2("zmod");
  var { zcrush: kp } = c2("zcrush");
  var { zdelay: qp } = c2("zdelay");
  var { zzfx: Sp } = c2("zzfx");
  var { color: Ap, colour: Tp } = c2(["color", "colour"]);
  var Cp = (...t) => t.reduce((e, n) => Object.assign(e, { [n]: Nt2(n) }), {});
  var xp = l("adsr", (t, e) => {
    t = Array.isArray(t) ? t : [t];
    const [n, s2, r, o] = t;
    return e.set({ attack: n, decay: s2, sustain: r, release: o });
  });
  var Bp = l("ad", (t, e) => {
    t = Array.isArray(t) ? t : [t];
    const [n, s2 = n] = t;
    return e.attack(n).decay(s2);
  });
  var Op = l("ds", (t, e) => {
    t = Array.isArray(t) ? t : [t];
    const [n, s2 = 0] = t;
    return e.set({ decay: n, sustain: s2 });
  });
  var zp = l("ar", (t, e) => {
    t = Array.isArray(t) ? t : [t];
    const [n, s2 = n] = t;
    return e.set({ attack: n, release: s2 });
  });
  var { midichan: Mp } = c2("midichan");
  var { midimap: Pp } = c2("midimap");
  var { midiport: Ep } = c2("midiport");
  var { midicmd: jp } = c2("midicmd");
  var Jp = l("control", (t, e) => {
    if (!Array.isArray(t))
      throw new Error("control expects an array of [ccn, ccv]");
    const [n, s2] = t;
    return e.ccn(n).ccv(s2);
  });
  var { ccn: $p } = c2("ccn");
  var { ccv: Np } = c2("ccv");
  var { ctlNum: Lp } = c2("ctlNum");
  var { nrpnn: Rp } = c2("nrpnn");
  var { nrpv: Wp } = c2("nrpv");
  var { progNum: Fp } = c2("progNum");
  var Ip = l("sysex", (t, e) => {
    if (!Array.isArray(t))
      throw new Error("sysex expects an array of [id, data]");
    const [n, s2] = t;
    return e.sysexid(n).sysexdata(s2);
  });
  var { sysexid: Vp } = c2("sysexid");
  var { sysexdata: Hp } = c2("sysexdata");
  var { midibend: Dp } = c2("midibend");
  var { miditouch: Gp } = c2("miditouch");
  var { polyTouch: Qp } = c2("polyTouch");
  var { oschost: Up } = c2("oschost");
  var { oscport: Xp } = c2("oscport");
  var yt2 = (t) => at2.has(t) ? at2.get(t) : t;
  var Kp = l("as", (t, e) => (t = Array.isArray(t) ? t : [t], e.fmap((n) => {
    n = Array.isArray(n) ? n : [n];
    const s2 = [];
    for (let r = 0; r < t.length; ++r)
      n[r] !== void 0 && s2.push([yt2(t[r]), n[r]]);
    return Object.fromEntries(s2);
  })));
  var Yp = l(
    "scrub",
    (t, e) => t.outerBind((n) => {
      Array.isArray(n) || (n = [n]);
      const [s2, r = 1] = n;
      return e.begin(s2).mul(ye2(r)).clip(1);
    }),
    false
  );
  var Bt2 = /* @__PURE__ */ new Map();
  var Zp = (t, e, ...n) => {
    const s2 = Bt2.get(t) ?? /* @__PURE__ */ new Map(), r = /* @__PURE__ */ new Set([e, ...n]);
    for (const o of r)
      s2.set(String(o).toLowerCase(), e);
    Bt2.set(t, s2);
  };
  var Lt2 = (t, e = []) => {
    for (const [n, ...s2] of e)
      Zp(t, n, ...s2);
  };
  var tf = (t, e) => {
    const n = Bt2.get(t);
    return n ? n.get(String(e).toLowerCase()) ?? e : e;
  };
  Lt2("lfo", [
    ["control", "c"],
    ["subControl", "sc"],
    ["rate", "r"],
    ["depth", "dep", "dr"],
    ["depthabs", "da"],
    ["dcoffset", "dc"],
    ["shape", "sh"],
    ["skew", "sk"],
    ["curve", "cu"],
    ["sync", "s"],
    ["fxi"]
  ]);
  Lt2("env", [
    ["control", "c"],
    ["subControl", "sc"],
    ["attack", "att", "a"],
    ["decay", "dec", "d"],
    ["sustain", "sus", "s"],
    ["release", "rel", "r"],
    ["depth", "dep", "dr"],
    ["depthabs", "da"],
    ["acurve", "ac"],
    ["dcurve", "dc"],
    ["rcurve", "rc"],
    ["fxi"]
  ]);
  Lt2("bmod", [
    ["bus", "b"],
    ["control", "c"],
    ["subControl", "sc"],
    ["depth", "dep", "dr"],
    ["depthabs", "da"],
    ["dc"],
    ["fxi"]
  ]);
  f2.prototype.modulate = function(t, e, n) {
    e = { control: void 0, ...e };
    const s2 = ["lfo", "env", "bmod"];
    if (!s2.includes(t))
      return E2(`[core] Modulation type ${t} not found. Please use one of 'lfo', 'env', 'bmod'`), this;
    let r = this, o;
    r = r.fmap((i) => (a2) => ({ v: i, id: a2 })).appLeft(d(n));
    for (const [i, a2] of Object.entries(e)) {
      const u2 = tf(t, i), p = d(a2);
      r = r.fmap(({ v: h, id: y2 }) => (g2) => {
        if (o === void 0) {
          let _2 = yt2(Object.keys(h).at(-1));
          s2.includes(_2) && (_2 = `${_2}_${[...h[_2].__ids].at(-1)}`), o = _2;
        }
        h[t] ?? (h[t] = { __ids: /* @__PURE__ */ new Set() });
        const v = h[t];
        return y2 ?? (y2 = v.__ids.size), v[y2] ?? (v[y2] = { control: o }), v.__ids.add(y2), g2 === void 0 ? { v: h, id: y2 } : (u2 === "control" || u2 === "subControl" ? v[y2][u2] = yt2(g2) : v[y2][u2] = g2, { v: h, id: y2 });
      }).appLeft(p);
    }
    return r.fmap(({ v: i }) => i);
  };
  f2.prototype.lfo = function(t, e) {
    return this.modulate("lfo", t, e);
  };
  var ef = (t) => C2({}).lfo(t);
  f2.prototype.env = function(t, e) {
    return this.modulate("env", t, e);
  };
  var nf = (t) => C2({}).env(t);
  f2.prototype.bmod = function(t, e) {
    return this.modulate("bmod", t, e);
  };
  var sf = (t) => C2({}).bmod(t);
  var { transient: rf } = c2(["transient", "transsustain"]);
  var { FXrelease: of, FXrel: cf, FXr: uf, fxr: af } = c2("FXrelease", "FXrel", "FXr", "fxr");
  var gy = /* @__PURE__ */ Object.freeze(/* @__PURE__ */ Object.defineProperty({
    __proto__: null,
    FXr: uf,
    FXrel: cf,
    FXrelease: of,
    accelerate: Ys,
    activeLabel: va,
    ad: Bp,
    adsr: xp,
    amp: sr,
    analyze: gc,
    anchor: Ua,
    ar: zp,
    as: Kp,
    att: or,
    attack: rr,
    bandf: Cc,
    bandq: Oc,
    bank: yc,
    bbexpr: ti2,
    bbst: ni2,
    begin: Mc,
    bgain: ja,
    binshift: Zl,
    bmod: sf,
    bp: Bc,
    bpa: ji2,
    bpattack: Ei2,
    bpd: Wi2,
    bpdc: yu,
    bpdecay: Ri2,
    bpdepth: fu,
    bpdepthfreq: du,
    bpdepthfrequency: hu,
    bpe: Bi2,
    bpenv: xi2,
    bpf: xc,
    bpq: zc,
    bpr: Zi2,
    bprate: lu,
    bprelease: Yi2,
    bps: Gi2,
    bpshape: mu,
    bpskew: wu,
    bpsustain: Di2,
    bpsync: pu,
    bus: Pa,
    busgain: Ea,
    byteBeatExpression: Zc,
    byteBeatStartTime: ei2,
    ccn: $p,
    ccv: Np,
    ch: ri2,
    channel: gi2,
    channels: si2,
    chord: Da,
    chorus: wc,
    clip: pp,
    coarse: Rc,
    color: Ap,
    colour: Tp,
    comb: Xl,
    compressor: bl,
    compressorAttack: kl,
    compressorKnee: _l,
    compressorRatio: vl,
    compressorRelease: ql,
    control: Jp,
    cps: lp,
    createParam: Nt2,
    createParams: Cp,
    crush: Lc,
    ctf: vi2,
    ctlNum: Lp,
    ctranspose: Aa,
    curve: yp,
    cut: bi2,
    cutoff: _i2,
    dec: vc,
    decay: _c,
    degree: qa,
    delay: Ru,
    delayfb: Fu,
    delayfeedback: Wu,
    delayspeed: Vu,
    delaysync: Qu,
    delayt: Du,
    delaytime: Hu,
    deltaSlide: wp,
    density: Bl,
    det: Ku,
    detune: Xu,
    dfb: Iu,
    dict: Qa,
    dictionary: Ga,
    dist: yl,
    distort: ml,
    distorttype: gl,
    distortvol: wl,
    djf: Lu,
    drive: Qc,
    dry: ta,
    ds: Op,
    dt: Gu,
    duck: Uc,
    duckattack: Yc,
    duckdepth: Xc,
    duckonset: Kc,
    dur: dp,
    duration: hp,
    end: Pc,
    enhance: Ul,
    env: nf,
    expression: Ol,
    fadeInTime: sa,
    fadeOutTime: na,
    fadeTime: ea,
    fanchor: eu,
    fft: bc,
    fm: Sr,
    fm1: Ar,
    fm2: Tr,
    fm3: Cr,
    fm4: xr,
    fm5: Br,
    fm6: Or,
    fm7: zr,
    fm8: Mr,
    fmatt: Kr,
    fmatt1: Yr,
    fmatt2: Zr,
    fmatt3: to,
    fmatt4: eo,
    fmatt5: no,
    fmatt6: so,
    fmatt7: ro,
    fmatt8: oo,
    fmattack: Fr,
    fmattack1: Ir,
    fmattack2: Vr,
    fmattack3: Hr,
    fmattack4: Dr,
    fmattack5: Gr,
    fmattack6: Qr,
    fmattack7: Ur,
    fmattack8: Xr,
    fmdec: Ao,
    fmdec1: To,
    fmdec2: Co,
    fmdec3: xo,
    fmdec4: Bo,
    fmdec5: Oo,
    fmdec6: zo,
    fmdec7: Mo,
    fmdec8: Po,
    fmdecay: yo,
    fmdecay1: wo,
    fmdecay2: go,
    fmdecay3: bo,
    fmdecay4: _o,
    fmdecay5: vo,
    fmdecay6: ko,
    fmdecay7: qo,
    fmdecay8: So,
    fmenv: Pr,
    fmenv1: Er,
    fmenv2: jr,
    fmenv3: Jr,
    fmenv4: $r,
    fmenv5: Nr,
    fmenv6: Lr,
    fmenv7: Rr,
    fmenv8: Wr,
    fmh: cr,
    fmh1: ir,
    fmh2: ur,
    fmh3: ar,
    fmh4: lr,
    fmh5: pr,
    fmh6: fr,
    fmh7: hr,
    fmh8: dr,
    fmi: mr,
    fmi1: yr,
    fmi2: wr,
    fmi3: gr,
    fmi4: br,
    fmi5: _r,
    fmi6: vr,
    fmi7: kr,
    fmi8: qr,
    fmrel: ic,
    fmrel1: uc,
    fmrel2: ac,
    fmrel3: lc,
    fmrel4: pc,
    fmrel5: fc,
    fmrel6: hc,
    fmrel7: dc,
    fmrel8: mc,
    fmrelease: Yo,
    fmrelease1: Zo,
    fmrelease2: tc,
    fmrelease3: ec,
    fmrelease4: nc,
    fmrelease5: sc,
    fmrelease6: rc,
    fmrelease7: oc,
    fmrelease8: cc,
    fmsus: Io,
    fmsus1: Vo,
    fmsus2: Ho,
    fmsus3: Do,
    fmsus4: Go,
    fmsus5: Qo,
    fmsus6: Uo,
    fmsus7: Xo,
    fmsus8: Ko,
    fmsustain: Eo,
    fmsustain1: jo,
    fmsustain2: Jo,
    fmsustain3: $o,
    fmsustain4: No,
    fmsustain5: Lo,
    fmsustain6: Ro,
    fmsustain7: Wo,
    fmsustain8: Fo,
    fmwave: co,
    fmwave1: io,
    fmwave2: uo,
    fmwave3: ao,
    fmwave4: lo,
    fmwave5: po,
    fmwave6: fo,
    fmwave7: ho,
    fmwave8: mo,
    frameRate: np,
    frames: sp,
    freeze: Vl,
    freq: ra,
    fshift: Ml,
    fshiftnote: Pl,
    fshiftphase: El,
    ftype: tu,
    fxr: af,
    gain: er,
    gat: wa,
    gate: ya,
    getControlName: yt2,
    harmonic: Ta,
    hbrick: tp,
    hcutoff: Mu,
    hold: Tc,
    hours: rp,
    hp: Eu,
    hpa: Pi2,
    hpattack: Mi2,
    hpd: Li2,
    hpdc: Su,
    hpdecay: Ni2,
    hpdepth: _u,
    hpdepthfreq: ku,
    hpdepthfrequency: vu,
    hpe: Ci2,
    hpenv: Ti2,
    hpf: Pu,
    hpq: Ju,
    hpr: Ki2,
    hprate: gu,
    hprelease: Xi2,
    hps: Hi2,
    hpshape: qu,
    hpskew: Au,
    hpsustain: Vi2,
    hpsync: bu,
    hresonance: ju,
    imag: Ql,
    ir: cl,
    irbegin: al,
    iresponse: il,
    irspeed: ul,
    isControlName: is,
    kcutoff: $l,
    krush: Jl,
    label: ka,
    lbrick: ep,
    legato: fp,
    leslie: ga,
    lfo: ef,
    lock: Uu,
    loop: Ec,
    loopBegin: jc,
    loopEnd: $c,
    loopb: Jc,
    loope: Nc,
    lp: qi2,
    lpa: zi2,
    lpattack: Oi2,
    lpd: $i2,
    lpdc: uu,
    lpdecay: Ji2,
    lpdepth: ru,
    lpdepthfreq: cu,
    lpdepthfrequency: ou,
    lpe: Ai2,
    lpenv: Si2,
    lpf: ki2,
    lpq: Nu,
    lpr: Ui2,
    lprate: nu,
    lprelease: Qi2,
    lps: Ii2,
    lpshape: iu,
    lpskew: au,
    lpsustain: Fi2,
    lpsync: su,
    lrate: ba,
    lsize: _a,
    midibend: Dp,
    midichan: Mp,
    midicmd: jp,
    midimap: Pp,
    midiport: Ep,
    miditouch: Gp,
    minutes: op,
    mode: Ya,
    mtranspose: Sa,
    n: Xs,
    noise: Bu,
    note: Ks,
    nrpnn: Rp,
    nrpv: Wp,
    nudge: Ba,
    oct: za,
    octave: Oa,
    octaveR: xa,
    octaves: Ka,
    octer: Nl,
    octersub: Ll,
    octersubsub: Rl,
    offset: Xa,
    orbit: Ma,
    oschost: Up,
    oscport: Xp,
    overgain: Ja,
    overshape: $a,
    pan: Na,
    panchor: ma,
    panorient: Fa,
    panspan: La,
    pansplay: Ra,
    panwidth: Wa,
    patt: ca,
    pattack: oa,
    pcurve: da,
    pdec: ua,
    pdecay: ia,
    penv: ha,
    ph: ai2,
    phasdp: wi2,
    phaser: li2,
    phasercenter: hi2,
    phaserdepth: mi2,
    phaserrate: ui2,
    phasersweep: pi2,
    phc: di2,
    phd: yi2,
    phs: fi2,
    pitchJump: gp,
    pitchJumpTime: bp,
    polyTouch: Qp,
    postgain: nr,
    prel: fa,
    prelease: pa,
    progNum: Fp,
    psus: la,
    psustain: aa,
    pw: oi2,
    pwrate: ci2,
    pwsweep: ii2,
    rdim: sl,
    real: Gl,
    registerControl: c2,
    registerMultiControl: V2,
    rel: Ac,
    release: Sc,
    resonance: $u,
    rfade: ol,
    ring: Wl,
    ringdf: Il,
    ringf: Fl,
    rlp: el,
    room: Za,
    roomdim: nl,
    roomfade: rl,
    roomlp: tl,
    roomsize: ll,
    rsize: hl,
    s: us,
    scram: Yl,
    scrub: Yp,
    seconds: cp,
    semitone: Va,
    shape: dl,
    size: pl,
    slide: Ia,
    smear: Kl,
    songPtr: ip,
    sound: as,
    source: Qs,
    speed: ye2,
    spread: Zu,
    squiz: Tl,
    src: Us,
    stepsPerOctave: Ca,
    stretch: Sl,
    sus: qc,
    sustain: kc,
    sustainpedal: zl,
    sysex: Ip,
    sysexdata: Hp,
    sysexid: Vp,
    sz: fl,
    transient: rf,
    trem: Fc,
    tremolo: Wc,
    tremolodepth: Vc,
    tremolophase: Dc,
    tremoloshape: Gc,
    tremoloskew: Hc,
    tremolosync: Ic,
    triode: jl,
    tsdelay: Dl,
    uid: up,
    unison: Yu,
    unit: Al,
    v: xu,
    val: ap,
    vel: tr,
    velocity: Zs,
    vib: Tu,
    vibmod: Ou,
    vibrato: Cu,
    vmod: zu,
    voice: Ha,
    vowel: Cl,
    warp: Cs,
    warpatt: Os,
    warpattack: Bs,
    warpdc: Rs,
    warpdec: Ms,
    warpdecay: zs,
    warpdepth: Ns,
    warpenv: Ds,
    warpmode: Fs,
    warprate: $s,
    warprel: Js,
    warprelease: js,
    warpshape: Ls,
    warpskew: Ws,
    warpsus: Es,
    warpsustain: Ps,
    warpsync: Gs,
    waveloss: xl,
    wavetablePhaseRand: Hs,
    wavetablePosition: ps,
    wavetableWarp: xs,
    wavetableWarpMode: Is,
    wt: ls,
    wtatt: ds,
    wtattack: hs,
    wtdc: As,
    wtdec: ys,
    wtdecay: ms,
    wtdepth: qs,
    wtenv: fs,
    wtphaserand: Vs,
    wtrate: vs,
    wtrel: _s,
    wtrelease: bs,
    wtshape: Ss,
    wtskew: Ts,
    wtsus: gs,
    wtsustain: ws,
    wtsync: ks,
    xsdelay: Hl,
    zcrush: kp,
    zdelay: qp,
    zmod: vp,
    znoise: _p,
    zrand: mp,
    zzfx: Sp
  }, Symbol.toStringTag, { value: "Module" }));
  var lf = function(t, e) {
    const [n, s2] = t, [r, o] = e, [i, a2] = ue2(s2, r);
    return [
      [s2, n - s2],
      [Pt2((u2, p) => u2.concat(p), i, o), a2]
    ];
  };
  var pf = function(t, e) {
    const [n, s2] = t, [r, o] = e, [i, a2] = ue2(n, o);
    return [
      [n, s2 - n],
      [Pt2((p, h) => p.concat(h), r, i), a2]
    ];
  };
  var we2 = function(t, e) {
    const [n, s2] = t;
    return Math.min(n, s2) <= 1 ? [t, e] : we2(...n > s2 ? lf(t, e) : pf(t, e));
  };
  var ge2 = function(t, e) {
    const n = t < 0, s2 = Math.abs(t), r = e - s2, o = Array(s2).fill([1]), i = Array(r).fill([0]), a2 = we2([s2, r], [o, i]), u2 = G(a2[1][0]).concat(G(a2[1][1]));
    return n ? u2.map((p) => 1 - p) : u2;
  };
  var kt2 = function(t, e, n) {
    const s2 = ge2(t, e);
    return n ? rn(s2, -n) : s2;
  };
  var by = l("euclid", function(t, e, n) {
    return n.struct(kt2(t, e, 0));
  });
  var _y = l("bjork", function(t, e) {
    Array.isArray(t) || (t = [t]);
    const [n, s2 = n, r = 0] = t;
    return e.struct(kt2(n, s2, r));
  });
  var { euclidrot: vy, euclidRot: ky } = l(["euclidrot", "euclidRot"], function(t, e, n, s2) {
    return s2.struct(kt2(t, e, n));
  });
  var be2 = function(t, e, n, s2) {
    if (t < 1)
      return q2;
    const o = kt2(t, e, 0).join("").split("1").slice(1).map((i) => [i.length + 1, true]);
    return s2.struct(ns(...o)).late(m(n).div(e));
  };
  var qy = l(["euclidLegato"], function(t, e, n) {
    return be2(t, e, 0, n);
  });
  var Sy = l(["euclidLegatoRot"], function(t, e, n, s2) {
    return be2(t, e, n, s2);
  });
  var { euclidish: Ay, eish: Ty } = l(["euclidish", "eish"], function(t, e, n, s2) {
    const r = de2(ge2(t, e), new Array(t).fill(1), n);
    return s2.struct(r).setSteps(e);
  });
  function ff(t, e, n = 0.05, s2 = 0.1, r = 0.1, o = globalThis.setInterval, i = globalThis.clearInterval, a2 = true) {
    let u2 = 0, p = 0, h = 10 ** 4, y2 = 0.01;
    const g2 = (x2) => n = x2(n);
    r = r || s2 / 2;
    const v = () => {
      const x2 = t(), D2 = x2 + s2 + r;
      for (p === 0 && (p = x2 + y2); p < D2; )
        p = a2 ? Math.round(p * h) / h : p, e(p, n, u2, x2), p += n, u2++;
    };
    let _2;
    const O2 = () => {
      A3(), v(), _2 = o(v, s2 * 1e3);
    }, A3 = () => {
      _2 !== void 0 && i(_2), _2 = void 0;
    };
    return { setDuration: g2, start: O2, stop: () => {
      u2 = 0, p = 0, A3();
    }, pause: () => A3(), duration: n, interval: s2, getPhase: () => p, minLatency: y2 };
  }
  var hf = class {
    constructor({
      interval: e,
      onTrigger: n,
      onToggle: s2,
      onError: r,
      getTime: o,
      latency: i = 0.1,
      setInterval: a2,
      clearInterval: u2,
      beforeStart: p
    }) {
      this.started = false, this.beforeStart = p, this.cps = 0.5, this.num_ticks_since_cps_change = 0, this.lastTick = 0, this.lastBegin = 0, this.lastEnd = 0, this.getTime = o, this.num_cycles_at_cps_change = 0, this.seconds_at_cps_change, this.onToggle = s2, this.latency = i, this.clock = ff(
        o,
        // called slightly before each cycle
        (h, y2, g2, v) => {
          this.num_ticks_since_cps_change === 0 && (this.num_cycles_at_cps_change = this.lastEnd, this.seconds_at_cps_change = h), this.num_ticks_since_cps_change++;
          const O2 = this.num_ticks_since_cps_change * y2 * this.cps;
          try {
            const A3 = this.lastEnd;
            this.lastBegin = A3;
            const I2 = this.num_cycles_at_cps_change + O2;
            if (this.lastEnd = I2, this.lastTick = h, h < v) {
              console.log("skip query: too late");
              return;
            }
            this.pattern.queryArc(A3, I2, { _cps: this.cps, cyclist: "cyclist" }).forEach((P3) => {
              if (P3.hasOnset()) {
                const x2 = (P3.whole.begin - this.num_cycles_at_cps_change) / this.cps + this.seconds_at_cps_change + i, D2 = P3.duration / this.cps, nt2 = x2 - h;
                n?.(P3, nt2, D2, this.cps, x2), P3.value.cps !== void 0 && this.cps != P3.value.cps && (this.cps = P3.value.cps, this.num_ticks_since_cps_change = 0);
              }
            });
          } catch (A3) {
            zt2(A3), r?.(A3);
          }
        },
        e,
        // duration of each cycle
        0.1,
        0.1,
        a2,
        u2
      );
    }
    now() {
      if (!this.started)
        return 0;
      const e = this.getTime() - this.lastTick - this.clock.duration;
      return this.lastBegin + e * this.cps;
    }
    setStarted(e) {
      this.started = e, this.onToggle?.(e);
    }
    async start() {
      if (await this.beforeStart?.(), this.num_ticks_since_cps_change = 0, this.num_cycles_at_cps_change = 0, !this.pattern)
        throw new Error("Scheduler: no pattern set! call .setPattern first.");
      E2("[cyclist] start"), this.clock.start(), this.setStarted(true);
    }
    pause() {
      E2("[cyclist] pause"), this.clock.pause(), this.setStarted(false);
    }
    stop() {
      E2("[cyclist] stop"), this.clock.stop(), this.lastEnd = 0, this.setStarted(false);
    }
    async setPattern(e, n = false) {
      this.pattern = e, n && !this.started && await this.start();
    }
    setCps(e = 0.5) {
      this.cps !== e && (this.cps = e, this.num_ticks_since_cps_change = 0);
    }
    log(e, n, s2) {
      const r = s2.filter((o) => o.hasOnset());
      console.log(`${e.toFixed(4)} - ${n.toFixed(4)} ${Array(r.length).fill("I").join("")}`);
    }
  };
  var ct2 = {};
  var df = function() {
    mf();
  };
  var mf = function() {
    ct2 = {};
  };
  var Cy = l(
    "timeline",
    function(t, e) {
      t = d(t);
      const n = function(s2) {
        const r = !!s2.controls.cyclist, o = t.query(s2), i = [];
        for (const a2 of o) {
          const u2 = a2.value;
          let p;
          if (u2 === 0)
            p = 0;
          else if (u2 in ct2)
            p = ct2[u2];
          else {
            const y2 = a2.wholeOrPart();
            !r || s2.span.begin.lt(y2.midpoint()) ? p = y2.begin : p = y2.end;
          }
          r && (ct2[u2] = p, u2 !== 0 && delete ct2[-u2]);
          const h = e.late(p).query(s2.setSpan(a2.part)).map((y2) => y2.setContext(y2.combineContext(a2)));
          i.push(...h);
        }
        return i;
      };
      return new f2(n, e._steps);
    },
    false
  );
  var F2 = function(t, e, n = true) {
    const s2 = Array.isArray(t), r = Object.keys(t).length;
    return t = bn(t, d), r === 0 ? q2 : e.fmap((o) => {
      let i = o;
      return s2 && (i = n ? Math.round(i) % r : an(Math.round(i), 0, t.length - 1)), t[i];
    });
  };
  var yf = function(t, e) {
    return Array.isArray(e) && ([e, t] = [t, e]), wf(t, e);
  };
  var wf = l("pick", function(t, e) {
    return F2(t, e, false).innerJoin();
  });
  var gf = l("pickmod", function(t, e) {
    return F2(t, e, true).innerJoin();
  });
  var xy = l("pickF", function(t, e, n) {
    return n.apply(yf(t, e));
  });
  var By = l("pickmodF", function(t, e, n) {
    return n.apply(gf(t, e));
  });
  var Oy = l("pickOut", function(t, e) {
    return F2(t, e, false).outerJoin();
  });
  var zy = l("pickmodOut", function(t, e) {
    return F2(t, e, true).outerJoin();
  });
  var My = l("pickRestart", function(t, e) {
    return F2(t, e, false).restartJoin();
  });
  var Py = l("pickmodRestart", function(t, e) {
    return F2(t, e, true).restartJoin();
  });
  var Ey = l("pickReset", function(t, e) {
    return F2(t, e, false).resetJoin();
  });
  var jy = l("pickmodReset", function(t, e) {
    return F2(t, e, true).resetJoin();
  });
  var { inhabit: Jy, pickSqueeze: $y } = l(["inhabit", "pickSqueeze"], function(t, e) {
    return F2(t, e, false).squeezeJoin();
  });
  var { inhabitmod: Ny, pickmodSqueeze: Ly } = l(["inhabitmod", "pickmodSqueeze"], function(t, e) {
    return F2(t, e, true).squeezeJoin();
  });
  var Ry = (t, e) => (e = e.map(d), e.length == 0 ? q2 : t.fmap((n) => {
    const s2 = bt2(Math.round(n), e.length);
    return e[s2];
  }).squeezeJoin());
  var bf = class {
    constructor({ onTrigger: e, onToggle: n, getTime: s2 }) {
      this.started = false, this.cps = 0.5, this.getTime = s2, this.time_at_last_tick_message = 0, this.collator = new _n({ getTargetClockTime: s2 }), this.onToggle = n, this.latency = 0.1, this.cycle = 0, this.id = Math.round(Date.now() * Math.random()), this.worker = new SharedWorker(new URL(
        /* @vite-ignore */
        "" + new URL("assets/clockworker-ZDiUtESR.js", import_meta.url).href,
        import_meta.url
      )), this.worker.port.start(), this.channel = new BroadcastChannel("strudeltick");
      const r = (i) => {
        const { cps: a2, begin: u2, end: p, cycle: h, time: y2 } = i;
        this.cps = a2, this.cycle = h;
        const g2 = this.collator.calculateOffset(y2) + y2;
        o(u2, p, g2), this.time_at_last_tick_message = g2;
      }, o = (i, a2, u2) => {
        if (this.started === false)
          return;
        this.pattern.queryArc(i, a2, { _cps: this.cps, cyclist: "neocyclist" }).forEach((h) => {
          if (h.hasOnset()) {
            const g2 = Kt2(h.whole.begin - this.cycle, this.cps) + u2 + this.latency, v = Kt2(h.duration, this.cps);
            e?.(h, 0, v, this.cps, g2);
          }
        });
      };
      this.channel.onmessage = (i) => {
        if (!this.started)
          return;
        const { payload: a2, type: u2 } = i.data;
        switch (u2) {
          case "tick":
            r(a2);
        }
      };
    }
    sendMessage(e, n) {
      this.worker.port.postMessage({ type: e, payload: n, id: this.id });
    }
    now() {
      const e = (this.getTime() - this.time_at_last_tick_message) * this.cps;
      return this.cycle + e;
    }
    setCps(e = 1) {
      this.sendMessage("cpschange", { cps: e });
    }
    setCycle(e) {
      this.sendMessage("setcycle", { cycle: e });
    }
    setStarted(e) {
      this.sendMessage("toggle", { started: e }), this.started = e, this.onToggle?.(e);
    }
    start() {
      E2("[cyclist] start"), this.setStarted(true);
    }
    stop() {
      E2("[cyclist] stop"), this.collator.reset(), this.setStarted(false);
    }
    setPattern(e, n = false) {
      this.pattern = e, n && !this.started && this.start();
    }
    log(e, n, s2) {
      const r = s2.filter((o) => o.hasOnset());
      console.log(`${e.toFixed(4)} - ${n.toFixed(4)} ${Array(r.length).fill("I").join("")}`);
    }
  };
  var Ot2;
  var _e2;
  var ve2;
  var ke2;
  var qe2;
  function Wy() {
    if (!Ot2)
      throw new Error("no time set! use setTime to define a time source");
    return Ot2();
  }
  function ee2(t) {
    Ot2 = t;
  }
  function _f(t) {
    _e2 = t;
  }
  function Fy() {
    return _e2?.();
  }
  function vf(t) {
    ve2 = t;
  }
  function Iy() {
    return ve2;
  }
  function kf(t) {
    ke2 = t;
  }
  function Vy() {
    return ke2;
  }
  function qf(t) {
    qe2 = !!t;
  }
  function Hy() {
    return qe2;
  }
  function Dy({
    defaultOutput: t,
    onEvalError: e,
    beforeEval: n,
    beforeStart: s2,
    afterEval: r,
    getTime: o,
    transpiler: i,
    onToggle: a2,
    editPattern: u2,
    onUpdateState: p,
    sync: h = false,
    setInterval: y2,
    clearInterval: g2,
    id: v,
    mondo: _2 = false
  }) {
    const O2 = new en({ localScope: true }), A3 = {
      schedulerError: void 0,
      evalError: void 0,
      code: "// LOADING",
      activeCode: "// LOADING",
      pattern: void 0,
      miniLocations: [],
      widgets: [],
      pending: false,
      started: false
    }, I2 = {
      id: v
    }, H2 = (b2) => {
      Object.assign(A3, b2), A3.isDirty = A3.code !== A3.activeCode, A3.error = A3.evalError || A3.schedulerError, p?.(A3);
    }, P3 = {
      onTrigger: Sf({ defaultOutput: t, getTime: o }),
      getTime: o,
      onToggle: (b2) => {
        H2({ started: b2 }), qf(b2), a2?.(b2), b2 || df();
      },
      setInterval: y2,
      clearInterval: g2,
      beforeStart: s2
    }, x2 = h && typeof SharedWorker < "u" ? new bf(P3) : new hf(P3);
    kf(P3.onTrigger), _f(() => x2.cps);
    let D2 = {}, nt2 = 0, tt2;
    const Vt2 = function() {
      return D2 = {}, nt2 = 0, tt2 = void 0, q2;
    }, Je2 = (b2) => O2.evaluate(b2).compile({ log: false });
    function Ht2(b2) {
      return b2._Pattern ? b2.__pure : b2;
    }
    const Dt2 = async (b2, k3 = true) => (b2 = u2?.(b2) || b2, await x2.setPattern(b2, k3), vf(b2), b2);
    ee2(() => x2.now());
    const $e2 = () => x2.stop(), Ne2 = () => x2.start(), Le2 = () => x2.pause(), Re2 = () => x2.toggle(), St2 = (b2) => (x2.setCps(Ht2(b2)), q2), Gt2 = (b2) => (x2.setCps(Ht2(b2) / 60), q2);
    let ft2 = [];
    const We2 = function(b2) {
      return ft2.push(b2), q2;
    }, Fe2 = function(b2) {
      return tt2 = b2, q2;
    }, Ie2 = () => {
      f2.prototype.p = function(k3) {
        return typeof k3 == "string" && (k3.startsWith("_") || k3.endsWith("_")) ? q2 : (k3.includes("$") && (k3 = `${k3}${nt2}`, nt2++), D2[k3] = this, this);
      }, f2.prototype.q = function(k3) {
        return q2;
      };
      try {
        for (let k3 = 1; k3 < 10; ++k3)
          Object.defineProperty(f2.prototype, `d${k3}`, {
            get() {
              return this.p(k3);
            },
            configurable: true
          }), Object.defineProperty(f2.prototype, `p${k3}`, {
            get() {
              return this.p(k3);
            },
            configurable: true
          }), f2.prototype[`q${k3}`] = q2;
      } catch (k3) {
        console.warn("injectPatternMethods: error:", k3);
      }
      const b2 = l("cpm", function(k3, At2) {
        return At2._fast(k3 / 60 / x2.cps);
      });
      return xn({
        all: We2,
        each: Fe2,
        hush: Vt2,
        cpm: b2,
        setCps: St2,
        setcps: St2,
        setCpm: Gt2,
        setcpm: Gt2,
        compileKabel: Je2
      });
    };
    return { scheduler: x2, evaluate: async (b2, k3 = true, At2 = true) => {
      if (!b2)
        throw new Error("no code to evaluate");
      try {
        H2({ code: b2, pending: true }), await Ie2(), ee2(() => x2.now()), await n?.({ code: b2 }), ft2 = [], At2 && Vt2(), _2 && (b2 = `mondolang\`${b2}\``);
        let { pattern: M2, meta: Tt2 } = await On(b2, i, I2);
        if (Object.keys(D2).length) {
          let X = [], ht2 = false;
          for (const [st2, Ve2] of Object.entries(D2)) {
            const Qt2 = st2.length > 1 && st2.startsWith("S");
            if (Qt2 && ht2 === false && (X = [], ht2 = true), !ht2 || ht2 && Qt2) {
              const He2 = Ve2.withState((De2) => De2.setControls({ id: st2 }));
              X.push(He2);
            }
          }
          tt2 && (X = X.map((st2) => tt2(st2))), M2 = z(...X);
        } else tt2 && (M2 = tt2(M2));
        if (ft2.length)
          for (const X of ft2)
            M2 = X(M2);
        return pe2(M2) || (M2 = q2), E2("[eval] code updated"), M2 = await Dt2(M2, k3), H2({
          miniLocations: Tt2?.miniLocations || [],
          widgets: Tt2?.widgets || [],
          activeCode: b2,
          pattern: M2,
          evalError: void 0,
          schedulerError: void 0,
          pending: false
        }), r?.({ code: b2, pattern: M2, meta: Tt2 }), M2;
      } catch (M2) {
        E2(`[eval] error: ${M2.message}`, "error"), console.error(M2), H2({ evalError: M2, pending: false }), e?.(M2);
      }
    }, start: Ne2, stop: $e2, pause: Le2, setCps: St2, setPattern: Dt2, setCode: (b2) => H2({ code: b2 }), toggle: Re2, state: A3 };
  }
  var Sf = ({ getTime: t, defaultOutput: e }) => async (n, s2, r, o, i) => {
    try {
      (!n.context.onTrigger || !n.context.dominantTrigger) && await e(n, s2, r, o, i), n.context.onTrigger && await n.context.onTrigger(n, t(), o, i);
    } catch (a2) {
      zt2(a2, "getTrigger");
    }
  };
  function Gy(t) {
    return new f2((e) => [new S2(void 0, e.span, t)]);
  }
  var j2 = (t) => {
    const e = (n) => [new S2(void 0, n.span, t(n.span.begin, n.controls))];
    return new f2(e);
  };
  var qt2 = j2((t) => t % 1);
  var Se2 = qt2.toBipolar();
  var Rt2 = j2((t) => 1 - t % 1);
  var Ae2 = Rt2.toBipolar();
  var Te2 = j2((t) => Math.sin(Math.PI * 2 * t));
  var Af = Te2.fromBipolar();
  var Qy = Af._early(m(1).div(4));
  var Uy = Te2._early(m(1).div(4));
  var Tf = j2((t) => Math.floor(t * 2 % 2));
  var Xy = Tf.toBipolar();
  var Ky = N2(qt2, Rt2);
  var Yy = N2(Se2, Ae2);
  var Zy = N2(Rt2, qt2);
  var tw = N2(Ae2, Se2);
  var ew = j2(ot2);
  var Wt2 = 0;
  var Ft2 = 0;
  typeof window < "u" && document.addEventListener("mousemove", (t) => {
    Wt2 = t.clientY / document.body.clientHeight, Ft2 = t.clientX / document.body.clientWidth;
  });
  var nw = j2(() => Wt2);
  var sw = j2(() => Wt2);
  var rw = j2(() => Ft2);
  var ow = j2(() => Ft2);
  var Cf = (t) => (t |= 0, t ^= t >>> 16, t = Math.imul(t, 2246822507), t ^= t >>> 13, t = Math.imul(t, 3266489909), t ^= t >>> 16, t >>> 0);
  var xf = (t) => Math.floor(t * 536870912);
  var Bf = (t, e = 0, n = 0) => {
    const s2 = t >>> 0 >>> 0, r = Math.floor(t / 4294967296) >>> 0;
    let o = s2 ^ Math.imul(r ^ 2246822507, 3266489909);
    return o ^= Math.imul(e ^ 2135587861, 2654435769), o ^= Math.imul(n ^ 374761393, 668265261), o >>> 0;
  };
  var ne2 = (t, e = 0, n = 0) => Cf(Bf(t, e, n)) / 4294967296;
  var Of = (t, e, n = 0) => {
    const s2 = xf(t);
    if (e === 1)
      return ne2(s2, 0, n);
    const r = new Array(e);
    for (let o = 0; o < e; o++) r[o] = ne2(s2, o, n);
    return r;
  };
  var Ce2 = (t) => {
    const e = t << 13 ^ t, n = e >> 17 ^ e;
    return n << 5 ^ n;
  };
  var zf = (t) => t - Math.trunc(t);
  var Mf = (t) => Ce2(Math.trunc(zf(t / 300) * 536870912));
  var se = (t) => t % 536870912 / 536870912;
  var Pf = (t, e) => {
    if (e === 1)
      return Math.abs(se(t));
    const n = [];
    for (let s2 = 0; s2 < e; s2++)
      n.push(se(t)), t = Ce2(t);
    return n;
  };
  var Ef = (t, e) => Pf(Mf(t), e);
  var xe2 = "legacy";
  var K2 = (t, e = 1, n = 0) => xe2 === "legacy" ? Ef(t + n, e) : Of(t, e, n);
  var cw = (t = "legacy") => xe2 = t;
  var jf = (t) => qt2.range(0, t).round().segment(t);
  var iw = (t) => {
    const e = d(t).log2(0).floor().add(1);
    return Jf(t, e);
  };
  var Jf = (t, e = 16) => {
    e = d(e);
    const n = jf(e).mul(-1).add(e.sub(1));
    return d(t).segment(e).brshift(n).band(C2(1));
  };
  var uw = (t) => {
    const e = d(t).log2(0).floor().add(1);
    return $f(t, e);
  };
  var $f = (t, e = 16) => d(t).withValue((n) => (s2) => {
    const r = [];
    for (let o = s2 - 1; o >= 0; o--)
      r.push(n >> o & 1);
    return r;
  }).appLeft(d(e));
  var aw = (t) => j2((e) => (n) => K2(e, n).map(Math.abs)).appLeft(d(t));
  var Nf = (t) => j2((e, n) => {
    const r = K2(e.floor().add(0.5), t, n.randSeed).map((i, a2) => [i, a2]).sort((i, a2) => (i[0] > a2[0]) - (i[0] < a2[0])).map((i) => i[1]), o = e.cyclePos().mul(t).floor() % t;
    return r[o];
  })._segment(t);
  var Be2 = (t, e, n) => {
    const s2 = [...Array(e).keys()].map((r) => n.zoom(m(r).div(e), m(r + 1).div(e)));
    return t.fmap((r) => s2[r].repeatCycles(e)._fast(e)).innerJoin();
  };
  var lw = l("shuffle", (t, e) => Be2(Nf(t), t, e));
  var pw = l("scramble", (t, e) => Be2(ze2(t)._segment(t), t, e));
  var Lf = (t, e) => new f2((n) => {
    let { randSeed: s2, ...r } = n.controls;
    return s2 = t(s2), e.query(n.setControls({ ...r, randSeed: s2 }));
  }, e._steps);
  var fw = l("seed", (t, e) => Lf(() => t, e));
  var W2 = j2((t, e) => K2(t, 1, e.randSeed));
  var hw = W2.toBipolar();
  var Oe2 = (t) => W2.fmap((e) => e < t);
  var dw = (t) => d(t).fmap(Oe2).innerJoin();
  var mw = Oe2(0.5);
  var ze2 = (t) => W2.fmap((e) => Math.trunc(e * t));
  var yw = (t) => d(t).fmap(ze2).innerJoin();
  var Me2 = (t, e) => (e = e.map(d), e.length == 0 ? q2 : t.range(0, e.length).fmap((n) => {
    const s2 = Math.min(Math.max(Math.floor(n), 0), e.length - 1);
    return e[s2];
  }));
  var It2 = (t, e) => Me2(t, e).outerJoin();
  var Pe2 = (t, e) => Me2(t, e).innerJoin();
  var Rf = (...t) => It2(W2, t);
  var ww = (...t) => Pe2(W2, t);
  var gw = Rf;
  f2.prototype.choose = function(...t) {
    return It2(this, t);
  };
  f2.prototype.choose2 = function(...t) {
    return It2(this.fromBipolar(), t);
  };
  var Wf = (...t) => Pe2(W2.segment(1), t);
  var bw = Wf;
  var Ee2 = function(t, ...e) {
    const n = e.map((a2) => d(a2[0])), s2 = [];
    let r = C2(0);
    for (const a2 of e)
      r = r.add(a2[1]), s2.push(r);
    const o = En(s2), i = function(a2) {
      const u2 = r.mul(a2);
      return o.fmap((p) => (h) => n[p.findIndex((y2) => y2 > h, p)]).appLeft(u2);
    };
    return t.bind(i);
  };
  var Ff = (...t) => Ee2(...t).outerJoin();
  var _w = (...t) => Ff(W2, ...t);
  var If = (...t) => Ee2(W2.segment(1), ...t).innerJoin();
  var vw = If;
  function Vf(t, e = 0) {
    let n = Math.floor(t), s2 = n + 1;
    const r = (p) => 6 * p ** 5 - 15 * p ** 4 + 10 * p ** 3, o = (p) => (h) => (y2) => h + r(p) * (y2 - h), i = K2(n, 1, e), a2 = K2(s2, 1, e);
    return o(t - n)(i)(a2);
  }
  function Hf(t, e = 0) {
    const n = Math.floor(t), s2 = n + 1, r = K2(n, 1, e), o = K2(s2, 1, e), i = r + o, a2 = (t - n) / (s2 - n);
    return ((p, h, y2) => p + y2 * (h - p))(r, i, a2) / 2;
  }
  var kw = j2((t, e) => Vf(t, e.randSeed));
  var qw = j2((t, e) => Hf(t, e.randSeed));
  var Sw = l(
    "degradeByWith",
    (t, e, n) => n.fmap((s2) => (r) => s2).appLeft(t.filterValues((s2) => s2 > e)),
    true,
    true
  );
  var Aw = l(
    "degradeBy",
    function(t, e) {
      return e._degradeByWith(W2, t);
    },
    true,
    true
  );
  var Tw = l("degrade", (t) => t._degradeBy(0.5), true, true);
  var Cw = l(
    "undegradeBy",
    function(t, e) {
      return e._degradeByWith(
        W2.fmap((n) => 1 - n),
        t
      );
    },
    true,
    true
  );
  var xw = l("undegrade", (t) => t._undegradeBy(0.5), true, true);
  var Bw = l("sometimesBy", function(t, e, n) {
    return d(t).fmap((s2) => z(n._degradeBy(s2), e(n._undegradeBy(1 - s2)))).innerJoin();
  });
  var Ow = l("sometimes", function(t, e) {
    return e._sometimesBy(0.5, t);
  });
  var zw = l("someCyclesBy", function(t, e, n) {
    return d(t).fmap(
      (s2) => z(
        n._degradeByWith(W2._segment(1), s2),
        e(n._degradeByWith(W2.fmap((r) => 1 - r)._segment(1), 1 - s2))
      )
    ).innerJoin();
  });
  var Mw = l("someCycles", function(t, e) {
    return e._someCyclesBy(0.5, t);
  });
  var Pw = l("often", function(t, e) {
    return e.sometimesBy(0.75, t);
  });
  var Ew = l("rarely", function(t, e) {
    return e.sometimesBy(0.25, t);
  });
  var jw = l("almostNever", function(t, e) {
    return e.sometimesBy(0.1, t);
  });
  var Jw = l("almostAlways", function(t, e) {
    return e.sometimesBy(0.9, t);
  });
  var $w = l("never", function(t, e) {
    return e;
  });
  var Nw = l("always", function(t, e) {
    return t(e);
  });
  function je2(t) {
    Array.isArray(t) === false && (t = [t]);
    const e = qn();
    return t.every((n) => {
      const s2 = kn.get(n) ?? n;
      return e[s2];
    });
  }
  var Lw = l("whenKey", function(t, e, n) {
    return n.when(je2(t), e);
  });
  var Rw = l("keyDown", function(t) {
    return t.fmap(je2);
  });
  var Ww = new f2(function(t) {
    return [new S2(void 0, t.span, t.span.duration)];
  });
  var Df = new f2(function(t) {
    return [new S2(void 0, t.span, m(1).div(t.span.duration))];
  });
  var Fw = Df;
  var Iw = new f2(function(t) {
    const e = m(1).div(t.span.duration);
    return [new S2(void 0, t.span, Math.log(e) / Math.log(2) + 1)];
  });
  var wt2;
  try {
    wt2 = window?.speechSynthesis;
  } catch {
    console.warn("cannot use window: not in browser?");
  }
  var re2 = wt2?.getVoices();
  function Gf(t, e, n) {
    wt2.cancel();
    const s2 = new SpeechSynthesisUtterance(t);
    s2.lang = e, re2 = wt2.getVoices();
    const r = re2.filter((o) => o.lang.includes(e));
    typeof n == "number" ? s2.voice = r[n % r.length] : typeof n == "string" && (s2.voice = r.find((o) => o.name === o)), speechSynthesis.speak(s2);
  }
  var Vw = l("speak", function(t, e, n) {
    return n.onTrigger((s2) => {
      Gf(s2.value, t, e);
    });
  });
  var Hw = function(t, e = {}) {
    const n = document.getElementById("code"), s2 = "background-image:url(" + t + ");background-size:contain;";
    n.style = s2;
    const { className: r } = n, o = (u2, p) => {
      ({
        style: () => n.style = s2 + ";" + p,
        className: () => n.className = p + " " + r
      })[u2]();
    }, i = Object.entries(e).filter(([u2, p]) => typeof p == "function");
    Object.entries(e).filter(([u2, p]) => typeof p == "string").forEach(([u2, p]) => o(u2, p)), i.length;
  };
  var Dw = () => {
    const t = document.getElementById("code");
    t && (t.style = "");
  };
  E2("\u{1F300} @strudel/core loaded \u{1F300}");
  globalThis._strudelLoaded && console.warn(
    `@strudel/core was loaded more than once...
This might happen when you have multiple versions of strudel installed. 
Please check with "npm ls @strudel/core".`
  );
  globalThis._strudelLoaded = true;

  // node_modules/@strudel/mini/dist/index.mjs
  var dist_exports2 = {};
  __export(dist_exports2, {
    StartRules: () => Gr2,
    SyntaxError: () => uu2,
    getLeafLocation: () => ee3,
    getLeafLocations: () => Yr2,
    getLeaves: () => Xr2,
    h: () => Jr2,
    m: () => Hr2,
    mini: () => te3,
    mini2ast: () => Au2,
    miniAllStrings: () => Qr2,
    minify: () => Kr2,
    parse: () => Mr2,
    patternifyAST: () => nu2
  });
  function Or2(t, i) {
    function e() {
      this.constructor = t;
    }
    e.prototype = i.prototype, t.prototype = new e();
  }
  function uu2(t, i, e, f3) {
    var l2 = Error.call(this, t);
    return Object.setPrototypeOf && Object.setPrototypeOf(l2, uu2.prototype), l2.expected = i, l2.found = e, l2.location = f3, l2.name = "SyntaxError", l2;
  }
  Or2(uu2, Error);
  function Cu2(t, i, e) {
    return e = e || " ", t.length > i ? t : (i -= t.length, e += e.repeat(i), t + e.slice(0, i));
  }
  uu2.prototype.format = function(t) {
    var i = "Error: " + this.message;
    if (this.location) {
      var e = null, f3;
      for (f3 = 0; f3 < t.length; f3++)
        if (t[f3].source === this.location.source) {
          e = t[f3].text.split(/\r\n|\n|\r/g);
          break;
        }
      var l2 = this.location.start, a2 = this.location.source && typeof this.location.source.offset == "function" ? this.location.source.offset(l2) : l2, D2 = this.location.source + ":" + a2.line + ":" + a2.column;
      if (e) {
        var v = this.location.end, g2 = Cu2("", a2.line.toString().length, " "), c3 = e[l2.line - 1], F3 = l2.line === v.line ? v.column : c3.length + 1, p = F3 - l2.column || 1;
        i += `
 --> ` + D2 + `
` + g2 + ` |
` + a2.line + " | " + c3 + `
` + g2 + " | " + Cu2("", l2.column - 1, " ") + Cu2("", p, "^");
      } else
        i += `
 at ` + D2;
    }
    return i;
  };
  uu2.buildMessage = function(t, i) {
    var e = {
      literal: function(c3) {
        return '"' + l2(c3.text) + '"';
      },
      class: function(c3) {
        var F3 = c3.parts.map(function(p) {
          return Array.isArray(p) ? a2(p[0]) + "-" + a2(p[1]) : a2(p);
        });
        return "[" + (c3.inverted ? "^" : "") + F3.join("") + "]";
      },
      any: function() {
        return "any character";
      },
      end: function() {
        return "end of input";
      },
      other: function(c3) {
        return c3.description;
      }
    };
    function f3(c3) {
      return c3.charCodeAt(0).toString(16).toUpperCase();
    }
    function l2(c3) {
      return c3.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\0/g, "\\0").replace(/\t/g, "\\t").replace(/\n/g, "\\n").replace(/\r/g, "\\r").replace(/[\x00-\x0F]/g, function(F3) {
        return "\\x0" + f3(F3);
      }).replace(/[\x10-\x1F\x7F-\x9F]/g, function(F3) {
        return "\\x" + f3(F3);
      });
    }
    function a2(c3) {
      return c3.replace(/\\/g, "\\\\").replace(/\]/g, "\\]").replace(/\^/g, "\\^").replace(/-/g, "\\-").replace(/\0/g, "\\0").replace(/\t/g, "\\t").replace(/\n/g, "\\n").replace(/\r/g, "\\r").replace(/[\x00-\x0F]/g, function(F3) {
        return "\\x0" + f3(F3);
      }).replace(/[\x10-\x1F\x7F-\x9F]/g, function(F3) {
        return "\\x" + f3(F3);
      });
    }
    function D2(c3) {
      return e[c3.type](c3);
    }
    function v(c3) {
      var F3 = c3.map(D2), p, w3;
      if (F3.sort(), F3.length > 0) {
        for (p = 1, w3 = 1; p < F3.length; p++)
          F3[p - 1] !== F3[p] && (F3[w3] = F3[p], w3++);
        F3.length = w3;
      }
      switch (F3.length) {
        case 1:
          return F3[0];
        case 2:
          return F3[0] + " or " + F3[1];
        default:
          return F3.slice(0, -1).join(", ") + ", or " + F3[F3.length - 1];
      }
    }
    function g2(c3) {
      return c3 ? '"' + l2(c3) + '"' : "end of input";
    }
    return "Expected " + v(t) + " but " + g2(i) + " found.";
  };
  function Mr2(t, i) {
    i = i !== void 0 ? i : {};
    var e = {}, f3 = i.grammarSource, l2 = { start: Uu2 }, a2 = Uu2, D2 = ".", v = "-", g2 = "0", c3 = ",", F3 = "|", p = "[", w3 = "]", P3 = "{", R2 = "}", su2 = "%", iu2 = "<", re3 = ">", ne3 = "!", se2 = "(", ie3 = ")", fe3 = "/", oe3 = "*", ae2 = "?", le2 = ":", Eu2 = "..", ce3 = "^", vu2 = "struct", $u2 = "target", mu2 = "euclid", _u2 = "slow", yu2 = "rotL", wu2 = "rotR", bu2 = "fast", xu2 = "scale", Iu2 = "//", ku2 = "cat", Ae3 = "$", Nu2 = "setcps", Pu2 = "setbpm", qu2 = "hush", pe3 = /^[1-9]/, ge3 = /^[eE]/, Fe2 = /^[+\-]/, he3 = /^[0-9]/, ju2 = /^[ \n\r\t\xA0]/, Be3 = /^["']/, Ce3 = /^[#\--.0-9A-Z\^-_a-z~\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376-\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0559\u0560-\u0588\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u066E-\u066F\u0671-\u06D3\u06D5\u06E5-\u06E6\u06EE-\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4-\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u08A0-\u08B4\u08B6-\u08BD\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098C\u098F-\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC-\u09DD\u09DF-\u09E1\u09F0-\u09F1\u09FC\u0A05-\u0A0A\u0A0F-\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32-\u0A33\u0A35-\u0A36\u0A38-\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2-\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0-\u0AE1\u0AF9\u0B05-\u0B0C\u0B0F-\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32-\u0B33\u0B35-\u0B39\u0B3D\u0B5C-\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99-\u0B9A\u0B9C\u0B9E-\u0B9F\u0BA3-\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C60-\u0C61\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0-\u0CE1\u0CF1-\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D5F-\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32-\u0E33\u0E40-\u0E46\u0E81-\u0E82\u0E84\u0E87-\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA-\u0EAB\u0EAD-\u0EB0\u0EB2-\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065-\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1878\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE-\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1C80-\u1C88\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF1\u1CF5-\u1CF6\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2-\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FEF\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A-\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7B9\uA7F7-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA8FD-\uA8FE\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uA9E0-\uA9E4\uA9E6-\uA9EF\uA9FA-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5-\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB65\uAB70-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40-\uFB41\uFB43-\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]/, De2 = /^[@_]/, Su2 = /^[^\n]/, de3 = pu2("number"), Ru2 = _2(".", false), Ee3 = O2([["1", "9"]], false, false), ve3 = O2(["e", "E"], false, false), $e2 = O2(["+", "-"], false, false), me3 = _2("-", false), _e3 = _2("0", false), ye3 = O2([["0", "9"]], false, false), we3 = pu2("whitespace"), Lu2 = O2([" ", `
`, "\r", "	", "\xA0"], false, false), be3 = _2(",", false), xe3 = _2("|", false), Ie2 = O2(['"', "'"], false, false), ke3 = pu2('a letter, a number, "-", "#", ".", "^", "_"'), Ne2 = O2(["#", ["-", "."], ["0", "9"], ["A", "Z"], ["^", "_"], ["a", "z"], "~", "\xAA", "\xB5", "\xBA", ["\xC0", "\xD6"], ["\xD8", "\xF6"], ["\xF8", "\u02C1"], ["\u02C6", "\u02D1"], ["\u02E0", "\u02E4"], "\u02EC", "\u02EE", ["\u0370", "\u0374"], ["\u0376", "\u0377"], ["\u037A", "\u037D"], "\u037F", "\u0386", ["\u0388", "\u038A"], "\u038C", ["\u038E", "\u03A1"], ["\u03A3", "\u03F5"], ["\u03F7", "\u0481"], ["\u048A", "\u052F"], ["\u0531", "\u0556"], "\u0559", ["\u0560", "\u0588"], ["\u05D0", "\u05EA"], ["\u05EF", "\u05F2"], ["\u0620", "\u064A"], ["\u066E", "\u066F"], ["\u0671", "\u06D3"], "\u06D5", ["\u06E5", "\u06E6"], ["\u06EE", "\u06EF"], ["\u06FA", "\u06FC"], "\u06FF", "\u0710", ["\u0712", "\u072F"], ["\u074D", "\u07A5"], "\u07B1", ["\u07CA", "\u07EA"], ["\u07F4", "\u07F5"], "\u07FA", ["\u0800", "\u0815"], "\u081A", "\u0824", "\u0828", ["\u0840", "\u0858"], ["\u0860", "\u086A"], ["\u08A0", "\u08B4"], ["\u08B6", "\u08BD"], ["\u0904", "\u0939"], "\u093D", "\u0950", ["\u0958", "\u0961"], ["\u0971", "\u0980"], ["\u0985", "\u098C"], ["\u098F", "\u0990"], ["\u0993", "\u09A8"], ["\u09AA", "\u09B0"], "\u09B2", ["\u09B6", "\u09B9"], "\u09BD", "\u09CE", ["\u09DC", "\u09DD"], ["\u09DF", "\u09E1"], ["\u09F0", "\u09F1"], "\u09FC", ["\u0A05", "\u0A0A"], ["\u0A0F", "\u0A10"], ["\u0A13", "\u0A28"], ["\u0A2A", "\u0A30"], ["\u0A32", "\u0A33"], ["\u0A35", "\u0A36"], ["\u0A38", "\u0A39"], ["\u0A59", "\u0A5C"], "\u0A5E", ["\u0A72", "\u0A74"], ["\u0A85", "\u0A8D"], ["\u0A8F", "\u0A91"], ["\u0A93", "\u0AA8"], ["\u0AAA", "\u0AB0"], ["\u0AB2", "\u0AB3"], ["\u0AB5", "\u0AB9"], "\u0ABD", "\u0AD0", ["\u0AE0", "\u0AE1"], "\u0AF9", ["\u0B05", "\u0B0C"], ["\u0B0F", "\u0B10"], ["\u0B13", "\u0B28"], ["\u0B2A", "\u0B30"], ["\u0B32", "\u0B33"], ["\u0B35", "\u0B39"], "\u0B3D", ["\u0B5C", "\u0B5D"], ["\u0B5F", "\u0B61"], "\u0B71", "\u0B83", ["\u0B85", "\u0B8A"], ["\u0B8E", "\u0B90"], ["\u0B92", "\u0B95"], ["\u0B99", "\u0B9A"], "\u0B9C", ["\u0B9E", "\u0B9F"], ["\u0BA3", "\u0BA4"], ["\u0BA8", "\u0BAA"], ["\u0BAE", "\u0BB9"], "\u0BD0", ["\u0C05", "\u0C0C"], ["\u0C0E", "\u0C10"], ["\u0C12", "\u0C28"], ["\u0C2A", "\u0C39"], "\u0C3D", ["\u0C58", "\u0C5A"], ["\u0C60", "\u0C61"], "\u0C80", ["\u0C85", "\u0C8C"], ["\u0C8E", "\u0C90"], ["\u0C92", "\u0CA8"], ["\u0CAA", "\u0CB3"], ["\u0CB5", "\u0CB9"], "\u0CBD", "\u0CDE", ["\u0CE0", "\u0CE1"], ["\u0CF1", "\u0CF2"], ["\u0D05", "\u0D0C"], ["\u0D0E", "\u0D10"], ["\u0D12", "\u0D3A"], "\u0D3D", "\u0D4E", ["\u0D54", "\u0D56"], ["\u0D5F", "\u0D61"], ["\u0D7A", "\u0D7F"], ["\u0D85", "\u0D96"], ["\u0D9A", "\u0DB1"], ["\u0DB3", "\u0DBB"], "\u0DBD", ["\u0DC0", "\u0DC6"], ["\u0E01", "\u0E30"], ["\u0E32", "\u0E33"], ["\u0E40", "\u0E46"], ["\u0E81", "\u0E82"], "\u0E84", ["\u0E87", "\u0E88"], "\u0E8A", "\u0E8D", ["\u0E94", "\u0E97"], ["\u0E99", "\u0E9F"], ["\u0EA1", "\u0EA3"], "\u0EA5", "\u0EA7", ["\u0EAA", "\u0EAB"], ["\u0EAD", "\u0EB0"], ["\u0EB2", "\u0EB3"], "\u0EBD", ["\u0EC0", "\u0EC4"], "\u0EC6", ["\u0EDC", "\u0EDF"], "\u0F00", ["\u0F40", "\u0F47"], ["\u0F49", "\u0F6C"], ["\u0F88", "\u0F8C"], ["\u1000", "\u102A"], "\u103F", ["\u1050", "\u1055"], ["\u105A", "\u105D"], "\u1061", ["\u1065", "\u1066"], ["\u106E", "\u1070"], ["\u1075", "\u1081"], "\u108E", ["\u10A0", "\u10C5"], "\u10C7", "\u10CD", ["\u10D0", "\u10FA"], ["\u10FC", "\u1248"], ["\u124A", "\u124D"], ["\u1250", "\u1256"], "\u1258", ["\u125A", "\u125D"], ["\u1260", "\u1288"], ["\u128A", "\u128D"], ["\u1290", "\u12B0"], ["\u12B2", "\u12B5"], ["\u12B8", "\u12BE"], "\u12C0", ["\u12C2", "\u12C5"], ["\u12C8", "\u12D6"], ["\u12D8", "\u1310"], ["\u1312", "\u1315"], ["\u1318", "\u135A"], ["\u1380", "\u138F"], ["\u13A0", "\u13F5"], ["\u13F8", "\u13FD"], ["\u1401", "\u166C"], ["\u166F", "\u167F"], ["\u1681", "\u169A"], ["\u16A0", "\u16EA"], ["\u16EE", "\u16F8"], ["\u1700", "\u170C"], ["\u170E", "\u1711"], ["\u1720", "\u1731"], ["\u1740", "\u1751"], ["\u1760", "\u176C"], ["\u176E", "\u1770"], ["\u1780", "\u17B3"], "\u17D7", "\u17DC", ["\u1820", "\u1878"], ["\u1880", "\u1884"], ["\u1887", "\u18A8"], "\u18AA", ["\u18B0", "\u18F5"], ["\u1900", "\u191E"], ["\u1950", "\u196D"], ["\u1970", "\u1974"], ["\u1980", "\u19AB"], ["\u19B0", "\u19C9"], ["\u1A00", "\u1A16"], ["\u1A20", "\u1A54"], "\u1AA7", ["\u1B05", "\u1B33"], ["\u1B45", "\u1B4B"], ["\u1B83", "\u1BA0"], ["\u1BAE", "\u1BAF"], ["\u1BBA", "\u1BE5"], ["\u1C00", "\u1C23"], ["\u1C4D", "\u1C4F"], ["\u1C5A", "\u1C7D"], ["\u1C80", "\u1C88"], ["\u1C90", "\u1CBA"], ["\u1CBD", "\u1CBF"], ["\u1CE9", "\u1CEC"], ["\u1CEE", "\u1CF1"], ["\u1CF5", "\u1CF6"], ["\u1D00", "\u1DBF"], ["\u1E00", "\u1F15"], ["\u1F18", "\u1F1D"], ["\u1F20", "\u1F45"], ["\u1F48", "\u1F4D"], ["\u1F50", "\u1F57"], "\u1F59", "\u1F5B", "\u1F5D", ["\u1F5F", "\u1F7D"], ["\u1F80", "\u1FB4"], ["\u1FB6", "\u1FBC"], "\u1FBE", ["\u1FC2", "\u1FC4"], ["\u1FC6", "\u1FCC"], ["\u1FD0", "\u1FD3"], ["\u1FD6", "\u1FDB"], ["\u1FE0", "\u1FEC"], ["\u1FF2", "\u1FF4"], ["\u1FF6", "\u1FFC"], "\u2071", "\u207F", ["\u2090", "\u209C"], "\u2102", "\u2107", ["\u210A", "\u2113"], "\u2115", ["\u2119", "\u211D"], "\u2124", "\u2126", "\u2128", ["\u212A", "\u212D"], ["\u212F", "\u2139"], ["\u213C", "\u213F"], ["\u2145", "\u2149"], "\u214E", ["\u2160", "\u2188"], ["\u2C00", "\u2C2E"], ["\u2C30", "\u2C5E"], ["\u2C60", "\u2CE4"], ["\u2CEB", "\u2CEE"], ["\u2CF2", "\u2CF3"], ["\u2D00", "\u2D25"], "\u2D27", "\u2D2D", ["\u2D30", "\u2D67"], "\u2D6F", ["\u2D80", "\u2D96"], ["\u2DA0", "\u2DA6"], ["\u2DA8", "\u2DAE"], ["\u2DB0", "\u2DB6"], ["\u2DB8", "\u2DBE"], ["\u2DC0", "\u2DC6"], ["\u2DC8", "\u2DCE"], ["\u2DD0", "\u2DD6"], ["\u2DD8", "\u2DDE"], "\u2E2F", ["\u3005", "\u3007"], ["\u3021", "\u3029"], ["\u3031", "\u3035"], ["\u3038", "\u303C"], ["\u3041", "\u3096"], ["\u309D", "\u309F"], ["\u30A1", "\u30FA"], ["\u30FC", "\u30FF"], ["\u3105", "\u312F"], ["\u3131", "\u318E"], ["\u31A0", "\u31BA"], ["\u31F0", "\u31FF"], ["\u3400", "\u4DB5"], ["\u4E00", "\u9FEF"], ["\uA000", "\uA48C"], ["\uA4D0", "\uA4FD"], ["\uA500", "\uA60C"], ["\uA610", "\uA61F"], ["\uA62A", "\uA62B"], ["\uA640", "\uA66E"], ["\uA67F", "\uA69D"], ["\uA6A0", "\uA6EF"], ["\uA717", "\uA71F"], ["\uA722", "\uA788"], ["\uA78B", "\uA7B9"], ["\uA7F7", "\uA801"], ["\uA803", "\uA805"], ["\uA807", "\uA80A"], ["\uA80C", "\uA822"], ["\uA840", "\uA873"], ["\uA882", "\uA8B3"], ["\uA8F2", "\uA8F7"], "\uA8FB", ["\uA8FD", "\uA8FE"], ["\uA90A", "\uA925"], ["\uA930", "\uA946"], ["\uA960", "\uA97C"], ["\uA984", "\uA9B2"], "\uA9CF", ["\uA9E0", "\uA9E4"], ["\uA9E6", "\uA9EF"], ["\uA9FA", "\uA9FE"], ["\uAA00", "\uAA28"], ["\uAA40", "\uAA42"], ["\uAA44", "\uAA4B"], ["\uAA60", "\uAA76"], "\uAA7A", ["\uAA7E", "\uAAAF"], "\uAAB1", ["\uAAB5", "\uAAB6"], ["\uAAB9", "\uAABD"], "\uAAC0", "\uAAC2", ["\uAADB", "\uAADD"], ["\uAAE0", "\uAAEA"], ["\uAAF2", "\uAAF4"], ["\uAB01", "\uAB06"], ["\uAB09", "\uAB0E"], ["\uAB11", "\uAB16"], ["\uAB20", "\uAB26"], ["\uAB28", "\uAB2E"], ["\uAB30", "\uAB5A"], ["\uAB5C", "\uAB65"], ["\uAB70", "\uABE2"], ["\uAC00", "\uD7A3"], ["\uD7B0", "\uD7C6"], ["\uD7CB", "\uD7FB"], ["\uF900", "\uFA6D"], ["\uFA70", "\uFAD9"], ["\uFB00", "\uFB06"], ["\uFB13", "\uFB17"], "\uFB1D", ["\uFB1F", "\uFB28"], ["\uFB2A", "\uFB36"], ["\uFB38", "\uFB3C"], "\uFB3E", ["\uFB40", "\uFB41"], ["\uFB43", "\uFB44"], ["\uFB46", "\uFBB1"], ["\uFBD3", "\uFD3D"], ["\uFD50", "\uFD8F"], ["\uFD92", "\uFDC7"], ["\uFDF0", "\uFDFB"], ["\uFE70", "\uFE74"], ["\uFE76", "\uFEFC"], ["\uFF21", "\uFF3A"], ["\uFF41", "\uFF5A"], ["\uFF66", "\uFFBE"], ["\uFFC2", "\uFFC7"], ["\uFFCA", "\uFFCF"], ["\uFFD2", "\uFFD7"], ["\uFFDA", "\uFFDC"]], false, false), Ou2 = _2("[", false), Mu2 = _2("]", false), Pe3 = _2("{", false), qe3 = _2("}", false), je3 = _2("%", false), Se3 = _2("<", false), Re2 = _2(">", false), Le2 = O2(["@", "_"], false, false), Oe3 = _2("!", false), Me3 = _2("(", false), ze3 = _2(")", false), Te3 = _2("/", false), Ze3 = _2("*", false), We2 = _2("?", false), Ue3 = _2(":", false), Ve2 = _2("..", false), Xe3 = _2("^", false), Ge2 = _2("struct", false), Ye3 = _2("target", false), He2 = _2("euclid", false), Je2 = _2("slow", false), Ke3 = _2("rotL", false), Qe3 = _2("rotR", false), ut3 = _2("fast", false), et3 = _2("scale", false), tt2 = _2("//", false), zu2 = O2([`
`], true, false), rt3 = _2("cat", false), nt2 = _2("$", false), st2 = _2("setcps", false), it3 = _2("setbpm", false), ft2 = _2("hush", false), ot3 = function() {
      return parseFloat(Xt3());
    }, at3 = function(u2) {
      const r = u2.join("");
      return r === "." || r === "_";
    }, lt3 = function(u2) {
      return new Sr2(u2.join(""));
    }, ct3 = function(u2) {
      return u2;
    }, At2 = function(u2, r) {
      return u2.arguments_.stepsPerCycle = r, u2;
    }, pt3 = function(u2) {
      return u2;
    }, gt3 = function(u2) {
      return u2.arguments_.alignment = "polymeter_slowcat", u2;
    }, Ft3 = function(u2) {
      return (r) => r.options_.weight = (r.options_.weight ?? 1) + (u2 ?? 2) - 1;
    }, ht2 = function(u2) {
      return (r) => {
        const s2 = (r.options_.reps ?? 1) + (u2 ?? 2) - 1;
        r.options_.reps = s2, r.options_.ops = r.options_.ops.filter((o) => o.type_ !== "replicate"), r.options_.ops.push({ type_: "replicate", arguments_: { amount: s2 } }), r.options_.weight = s2;
      };
    }, Bt3 = function(u2, r, s2) {
      return (o) => o.options_.ops.push({ type_: "bjorklund", arguments_: { pulse: u2, step: r, rotation: s2 } });
    }, Ct3 = function(u2) {
      return (r) => r.options_.ops.push({ type_: "stretch", arguments_: { amount: u2, type: "slow" } });
    }, Dt2 = function(u2) {
      return (r) => r.options_.ops.push({ type_: "stretch", arguments_: { amount: u2, type: "fast" } });
    }, dt3 = function(u2) {
      return (r) => r.options_.ops.push({ type_: "degradeBy", arguments_: { amount: u2, seed: Bu2++ } });
    }, Et3 = function(u2) {
      return (r) => r.options_.ops.push({ type_: "tail", arguments_: { element: u2 } });
    }, vt3 = function(u2) {
      return (r) => r.options_.ops.push({ type_: "range", arguments_: { element: u2 } });
    }, $t3 = function(u2, r) {
      const s2 = new Lr2(u2, { ops: [], weight: 1, reps: 1 });
      for (const o of r)
        o(s2);
      return s2;
    }, mt3 = function(u2, r) {
      return new lu2(r, "fastcat", void 0, !!u2);
    }, _t3 = function(u2) {
      return { alignment: "stack", list: u2 };
    }, yt3 = function(u2) {
      return { alignment: "rand", list: u2, seed: Bu2++ };
    }, wt3 = function(u2) {
      return { alignment: "feet", list: u2, seed: Bu2++ };
    }, bt3 = function(u2, r) {
      return r && r.list.length > 0 ? new lu2([u2, ...r.list], r.alignment, r.seed) : u2;
    }, xt3 = function(u2, r) {
      return new lu2(r ? [u2, ...r.list] : [u2], "polymeter");
    }, It3 = function(u2) {
      return u2;
    }, kt3 = function(u2) {
      return { name: "struct", args: { mini: u2 } };
    }, Nt3 = function(u2) {
      return { name: "target", args: { name: u2 } };
    }, Pt3 = function(u2, r, s2) {
      return { name: "bjorklund", args: { pulse: u2, step: parseInt(r) } };
    }, qt3 = function(u2) {
      return { name: "stretch", args: { amount: u2 } };
    }, jt3 = function(u2) {
      return { name: "shift", args: { amount: "-" + u2 } };
    }, St2 = function(u2) {
      return { name: "shift", args: { amount: u2 } };
    }, Rt3 = function(u2) {
      return { name: "stretch", args: { amount: "1/" + u2 } };
    }, Lt3 = function(u2) {
      return { name: "scale", args: { scale: u2.join("") } };
    }, Tu2 = function(u2, r) {
      return r;
    }, Ot3 = function(u2, r) {
      return r.unshift(u2), new lu2(r, "slowcat");
    }, Mt3 = function(u2) {
      return u2;
    }, zt3 = function(u2, r) {
      return new Rr2(u2.name, u2.args, r);
    }, Tt2 = function(u2) {
      return u2;
    }, Zt3 = function(u2) {
      return u2;
    }, Wt3 = function(u2) {
      return new hu2("setcps", { value: u2 });
    }, Ut3 = function(u2) {
      return new hu2("setcps", { value: u2 / 120 / 2 });
    }, Vt2 = function() {
      return new hu2("hush");
    }, n = i.peg$currPos | 0, $3 = n, V3 = [{ line: 1, column: 1 }], q3 = n, fu2 = i.peg$maxFailExpected || [], h = i.peg$silentFails | 0, eu2;
    if (i.startRule) {
      if (!(i.startRule in l2))
        throw new Error(`Can't start parsing from rule "` + i.startRule + '".');
      a2 = l2[i.startRule];
    }
    function Xt3() {
      return t.substring($3, n);
    }
    function Zu2() {
      return gu2($3, n);
    }
    function _2(u2, r) {
      return { type: "literal", text: u2, ignoreCase: r };
    }
    function O2(u2, r, s2) {
      return { type: "class", parts: u2, inverted: r, ignoreCase: s2 };
    }
    function Gt2() {
      return { type: "end" };
    }
    function pu2(u2) {
      return { type: "other", description: u2 };
    }
    function Wu2(u2) {
      var r = V3[u2], s2;
      if (r)
        return r;
      if (u2 >= V3.length)
        s2 = V3.length - 1;
      else
        for (s2 = u2; !V3[--s2]; )
          ;
      for (r = V3[s2], r = {
        line: r.line,
        column: r.column
      }; s2 < u2; )
        t.charCodeAt(s2) === 10 ? (r.line++, r.column = 1) : r.column++, s2++;
      return V3[u2] = r, r;
    }
    function gu2(u2, r, s2) {
      var o = Wu2(u2), B3 = Wu2(r), x2 = {
        source: f3,
        start: {
          offset: u2,
          line: o.line,
          column: o.column
        },
        end: {
          offset: r,
          line: B3.line,
          column: B3.column
        }
      };
      return x2;
    }
    function d2(u2) {
      n < q3 || (n > q3 && (q3 = n, fu2 = []), fu2.push(u2));
    }
    function Yt3(u2, r, s2) {
      return new uu2(
        uu2.buildMessage(u2, r),
        u2,
        r,
        s2
      );
    }
    function Uu2() {
      var u2;
      return u2 = jr2(), u2;
    }
    function M2() {
      var u2, r;
      return h++, u2 = n, er2(), r = ou2(), r !== e ? (ur2(), Qt2(), $3 = u2, u2 = ot3()) : (n = u2, u2 = e), h--, u2 === e && h === 0 && d2(de3), u2;
    }
    function Ht2() {
      var u2;
      return t.charCodeAt(n) === 46 ? (u2 = D2, n++) : (u2 = e, h === 0 && d2(Ru2)), u2;
    }
    function Jt3() {
      var u2;
      return u2 = t.charAt(n), pe3.test(u2) ? n++ : (u2 = e, h === 0 && d2(Ee3)), u2;
    }
    function Kt3() {
      var u2;
      return u2 = t.charAt(n), ge3.test(u2) ? n++ : (u2 = e, h === 0 && d2(ve3)), u2;
    }
    function Qt2() {
      var u2, r, s2, o, B3;
      if (u2 = n, r = Kt3(), r !== e) {
        if (s2 = t.charAt(n), Fe2.test(s2) ? n++ : (s2 = e, h === 0 && d2($e2)), s2 === e && (s2 = null), o = [], B3 = X(), B3 !== e)
          for (; B3 !== e; )
            o.push(B3), B3 = X();
        else
          o = e;
        o !== e ? (r = [r, s2, o], u2 = r) : (n = u2, u2 = e);
      } else
        n = u2, u2 = e;
      return u2;
    }
    function ur2() {
      var u2, r, s2, o;
      if (u2 = n, r = Ht2(), r !== e) {
        if (s2 = [], o = X(), o !== e)
          for (; o !== e; )
            s2.push(o), o = X();
        else
          s2 = e;
        s2 !== e ? (r = [r, s2], u2 = r) : (n = u2, u2 = e);
      } else
        n = u2, u2 = e;
      return u2;
    }
    function ou2() {
      var u2, r, s2, o;
      if (u2 = tr2(), u2 === e)
        if (u2 = n, r = Jt3(), r !== e) {
          for (s2 = [], o = X(); o !== e; )
            s2.push(o), o = X();
          r = [r, s2], u2 = r;
        } else
          n = u2, u2 = e;
      return u2;
    }
    function er2() {
      var u2;
      return t.charCodeAt(n) === 45 ? (u2 = v, n++) : (u2 = e, h === 0 && d2(me3)), u2;
    }
    function tr2() {
      var u2;
      return t.charCodeAt(n) === 48 ? (u2 = g2, n++) : (u2 = e, h === 0 && d2(_e3)), u2;
    }
    function X() {
      var u2;
      return u2 = t.charAt(n), he3.test(u2) ? n++ : (u2 = e, h === 0 && d2(ye3)), u2;
    }
    function E3() {
      var u2, r;
      for (h++, u2 = [], r = t.charAt(n), ju2.test(r) ? n++ : (r = e, h === 0 && d2(Lu2)); r !== e; )
        u2.push(r), r = t.charAt(n), ju2.test(r) ? n++ : (r = e, h === 0 && d2(Lu2));
      return h--, r = e, h === 0 && d2(we3), u2;
    }
    function G2() {
      var u2, r, s2, o;
      return u2 = n, r = E3(), t.charCodeAt(n) === 44 ? (s2 = c3, n++) : (s2 = e, h === 0 && d2(be3)), s2 !== e ? (o = E3(), r = [r, s2, o], u2 = r) : (n = u2, u2 = e), u2;
    }
    function Vu2() {
      var u2, r, s2, o;
      return u2 = n, r = E3(), t.charCodeAt(n) === 124 ? (s2 = F3, n++) : (s2 = e, h === 0 && d2(xe3)), s2 !== e ? (o = E3(), r = [r, s2, o], u2 = r) : (n = u2, u2 = e), u2;
    }
    function Xu2() {
      var u2, r, s2, o;
      return u2 = n, r = E3(), t.charCodeAt(n) === 46 ? (s2 = D2, n++) : (s2 = e, h === 0 && d2(Ru2)), s2 !== e ? (o = E3(), r = [r, s2, o], u2 = r) : (n = u2, u2 = e), u2;
    }
    function Y3() {
      var u2;
      return u2 = t.charAt(n), Be3.test(u2) ? n++ : (u2 = e, h === 0 && d2(Ie2)), u2;
    }
    function au2() {
      var u2;
      return h++, u2 = t.charAt(n), Ce3.test(u2) ? n++ : (u2 = e, h === 0 && d2(Ne2)), h--, u2 === e && h === 0 && d2(ke3), u2;
    }
    function Gu2() {
      var u2, r, s2, o;
      if (u2 = n, E3(), r = [], s2 = au2(), s2 !== e)
        for (; s2 !== e; )
          r.push(s2), s2 = au2();
      else
        r = e;
      return r !== e ? (s2 = E3(), $3 = n, o = at3(r), o ? o = e : o = void 0, o !== e ? ($3 = u2, u2 = lt3(r)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function rr2() {
      var u2, r, s2, o;
      return u2 = n, E3(), t.charCodeAt(n) === 91 ? (r = p, n++) : (r = e, h === 0 && d2(Ou2)), r !== e ? (E3(), s2 = Ju2(), s2 !== e ? (E3(), t.charCodeAt(n) === 93 ? (o = w3, n++) : (o = e, h === 0 && d2(Mu2)), o !== e ? (E3(), $3 = u2, u2 = ct3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function nr2() {
      var u2, r, s2, o, B3;
      return u2 = n, E3(), t.charCodeAt(n) === 123 ? (r = P3, n++) : (r = e, h === 0 && d2(Pe3)), r !== e ? (E3(), s2 = Ku2(), s2 !== e ? (E3(), t.charCodeAt(n) === 125 ? (o = R2, n++) : (o = e, h === 0 && d2(qe3)), o !== e ? (B3 = sr2(), B3 === e && (B3 = null), E3(), $3 = u2, u2 = At2(s2, B3)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function sr2() {
      var u2, r, s2;
      return u2 = n, t.charCodeAt(n) === 37 ? (r = su2, n++) : (r = e, h === 0 && d2(je3)), r !== e ? (s2 = H2(), s2 !== e ? ($3 = u2, u2 = pt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function ir2() {
      var u2, r, s2, o;
      return u2 = n, E3(), t.charCodeAt(n) === 60 ? (r = iu2, n++) : (r = e, h === 0 && d2(Se3)), r !== e ? (E3(), s2 = Ku2(), s2 !== e ? (E3(), t.charCodeAt(n) === 62 ? (o = re3, n++) : (o = e, h === 0 && d2(Re2)), o !== e ? (E3(), $3 = u2, u2 = gt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function H2() {
      var u2;
      return u2 = Gu2(), u2 === e && (u2 = rr2(), u2 === e && (u2 = nr2(), u2 === e && (u2 = ir2()))), u2;
    }
    function Yu2() {
      var u2;
      return u2 = fr2(), u2 === e && (u2 = ar2(), u2 === e && (u2 = lr2(), u2 === e && (u2 = cr2(), u2 === e && (u2 = or2(), u2 === e && (u2 = Ar2(), u2 === e && (u2 = pr2(), u2 === e && (u2 = gr2()))))))), u2;
    }
    function fr2() {
      var u2, r, s2;
      return u2 = n, E3(), r = t.charAt(n), De2.test(r) ? n++ : (r = e, h === 0 && d2(Le2)), r !== e ? (s2 = M2(), s2 === e && (s2 = null), $3 = u2, u2 = Ft3(s2)) : (n = u2, u2 = e), u2;
    }
    function or2() {
      var u2, r, s2;
      return u2 = n, E3(), t.charCodeAt(n) === 33 ? (r = ne3, n++) : (r = e, h === 0 && d2(Oe3)), r !== e ? (s2 = M2(), s2 === e && (s2 = null), $3 = u2, u2 = ht2(s2)) : (n = u2, u2 = e), u2;
    }
    function ar2() {
      var u2, r, s2, o, B3, x2, j3;
      return u2 = n, t.charCodeAt(n) === 40 ? (r = se2, n++) : (r = e, h === 0 && d2(Me3)), r !== e ? (E3(), s2 = tu2(), s2 !== e ? (E3(), o = G2(), o !== e ? (E3(), B3 = tu2(), B3 !== e ? (E3(), G2(), E3(), x2 = tu2(), x2 === e && (x2 = null), E3(), t.charCodeAt(n) === 41 ? (j3 = ie3, n++) : (j3 = e, h === 0 && d2(ze3)), j3 !== e ? ($3 = u2, u2 = Bt3(s2, B3, x2)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function lr2() {
      var u2, r, s2;
      return u2 = n, t.charCodeAt(n) === 47 ? (r = fe3, n++) : (r = e, h === 0 && d2(Te3)), r !== e ? (s2 = H2(), s2 !== e ? ($3 = u2, u2 = Ct3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function cr2() {
      var u2, r, s2;
      return u2 = n, t.charCodeAt(n) === 42 ? (r = oe3, n++) : (r = e, h === 0 && d2(Ze3)), r !== e ? (s2 = H2(), s2 !== e ? ($3 = u2, u2 = Dt2(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function Ar2() {
      var u2, r, s2;
      return u2 = n, t.charCodeAt(n) === 63 ? (r = ae2, n++) : (r = e, h === 0 && d2(We2)), r !== e ? (s2 = M2(), s2 === e && (s2 = null), $3 = u2, u2 = dt3(s2)) : (n = u2, u2 = e), u2;
    }
    function pr2() {
      var u2, r, s2;
      return u2 = n, t.charCodeAt(n) === 58 ? (r = le2, n++) : (r = e, h === 0 && d2(Ue3)), r !== e ? (s2 = H2(), s2 !== e ? ($3 = u2, u2 = Et3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function gr2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 2) === Eu2 ? (r = Eu2, n += 2) : (r = e, h === 0 && d2(Ve2)), r !== e ? (s2 = H2(), s2 !== e ? ($3 = u2, u2 = vt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function tu2() {
      var u2, r, s2, o;
      if (u2 = n, r = H2(), r !== e) {
        for (s2 = [], o = Yu2(); o !== e; )
          s2.push(o), o = Yu2();
        $3 = u2, u2 = $t3(r, s2);
      } else
        n = u2, u2 = e;
      return u2;
    }
    function T2() {
      var u2, r, s2, o;
      if (u2 = n, t.charCodeAt(n) === 94 ? (r = ce3, n++) : (r = e, h === 0 && d2(Xe3)), r === e && (r = null), s2 = [], o = tu2(), o !== e)
        for (; o !== e; )
          s2.push(o), o = tu2();
      else
        s2 = e;
      return s2 !== e ? ($3 = u2, u2 = mt3(r, s2)) : (n = u2, u2 = e), u2;
    }
    function Hu2() {
      var u2, r, s2, o, B3;
      if (u2 = n, r = [], s2 = n, o = G2(), o !== e ? (B3 = T2(), B3 !== e ? s2 = B3 : (n = s2, s2 = e)) : (n = s2, s2 = e), s2 !== e)
        for (; s2 !== e; )
          r.push(s2), s2 = n, o = G2(), o !== e ? (B3 = T2(), B3 !== e ? s2 = B3 : (n = s2, s2 = e)) : (n = s2, s2 = e);
      else
        r = e;
      return r !== e && ($3 = u2, r = _t3(r)), u2 = r, u2;
    }
    function Fr2() {
      var u2, r, s2, o, B3;
      if (u2 = n, r = [], s2 = n, o = Vu2(), o !== e ? (B3 = T2(), B3 !== e ? s2 = B3 : (n = s2, s2 = e)) : (n = s2, s2 = e), s2 !== e)
        for (; s2 !== e; )
          r.push(s2), s2 = n, o = Vu2(), o !== e ? (B3 = T2(), B3 !== e ? s2 = B3 : (n = s2, s2 = e)) : (n = s2, s2 = e);
      else
        r = e;
      return r !== e && ($3 = u2, r = yt3(r)), u2 = r, u2;
    }
    function hr2() {
      var u2, r, s2, o, B3;
      if (u2 = n, r = [], s2 = n, o = Xu2(), o !== e ? (B3 = T2(), B3 !== e ? s2 = B3 : (n = s2, s2 = e)) : (n = s2, s2 = e), s2 !== e)
        for (; s2 !== e; )
          r.push(s2), s2 = n, o = Xu2(), o !== e ? (B3 = T2(), B3 !== e ? s2 = B3 : (n = s2, s2 = e)) : (n = s2, s2 = e);
      else
        r = e;
      return r !== e && ($3 = u2, r = wt3(r)), u2 = r, u2;
    }
    function Ju2() {
      var u2, r, s2;
      return u2 = n, r = T2(), r !== e ? (s2 = Hu2(), s2 === e && (s2 = Fr2(), s2 === e && (s2 = hr2())), s2 === e && (s2 = null), $3 = u2, u2 = bt3(r, s2)) : (n = u2, u2 = e), u2;
    }
    function Ku2() {
      var u2, r, s2;
      return u2 = n, r = T2(), r !== e ? (s2 = Hu2(), s2 === e && (s2 = null), $3 = u2, u2 = xt3(r, s2)) : (n = u2, u2 = e), u2;
    }
    function Br2() {
      var u2, r, s2, o;
      return u2 = n, E3(), r = Y3(), r !== e ? (E3(), s2 = Ju2(), s2 !== e ? (E3(), o = Y3(), o !== e ? ($3 = u2, u2 = It3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function Cr2() {
      var u2;
      return u2 = yr2(), u2 === e && (u2 = vr2(), u2 === e && (u2 = _r2(), u2 === e && (u2 = dr2(), u2 === e && (u2 = Er2(), u2 === e && (u2 = Dr2(), u2 === e && (u2 = mr2(), u2 === e && (u2 = $r2()))))))), u2;
    }
    function Dr2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 6) === vu2 ? (r = vu2, n += 6) : (r = e, h === 0 && d2(Ge2)), r !== e ? (E3(), s2 = J3(), s2 !== e ? ($3 = u2, u2 = kt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function dr2() {
      var u2, r, s2, o, B3;
      return u2 = n, t.substr(n, 6) === $u2 ? (r = $u2, n += 6) : (r = e, h === 0 && d2(Ye3)), r !== e ? (E3(), s2 = Y3(), s2 !== e ? (o = Gu2(), o !== e ? (B3 = Y3(), B3 !== e ? ($3 = u2, u2 = Nt3(o)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function Er2() {
      var u2, r, s2, o;
      return u2 = n, t.substr(n, 6) === mu2 ? (r = mu2, n += 6) : (r = e, h === 0 && d2(He2)), r !== e ? (E3(), s2 = ou2(), s2 !== e ? (E3(), o = ou2(), o !== e ? (E3(), ou2(), $3 = u2, u2 = Pt3(s2, o)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function vr2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 4) === _u2 ? (r = _u2, n += 4) : (r = e, h === 0 && d2(Je2)), r !== e ? (E3(), s2 = M2(), s2 !== e ? ($3 = u2, u2 = qt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function $r2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 4) === yu2 ? (r = yu2, n += 4) : (r = e, h === 0 && d2(Ke3)), r !== e ? (E3(), s2 = M2(), s2 !== e ? ($3 = u2, u2 = jt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function mr2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 4) === wu2 ? (r = wu2, n += 4) : (r = e, h === 0 && d2(Qe3)), r !== e ? (E3(), s2 = M2(), s2 !== e ? ($3 = u2, u2 = St2(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function _r2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 4) === bu2 ? (r = bu2, n += 4) : (r = e, h === 0 && d2(ut3)), r !== e ? (E3(), s2 = M2(), s2 !== e ? ($3 = u2, u2 = Rt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function yr2() {
      var u2, r, s2, o, B3;
      if (u2 = n, t.substr(n, 5) === xu2 ? (r = xu2, n += 5) : (r = e, h === 0 && d2(et3)), r !== e)
        if (E3(), s2 = Y3(), s2 !== e) {
          if (o = [], B3 = au2(), B3 !== e)
            for (; B3 !== e; )
              o.push(B3), B3 = au2();
          else
            o = e;
          o !== e ? (B3 = Y3(), B3 !== e ? ($3 = u2, u2 = Lt3(o)) : (n = u2, u2 = e)) : (n = u2, u2 = e);
        } else
          n = u2, u2 = e;
      else
        n = u2, u2 = e;
      return u2;
    }
    function Fu2() {
      var u2, r, s2, o;
      if (u2 = n, t.substr(n, 2) === Iu2 ? (r = Iu2, n += 2) : (r = e, h === 0 && d2(tt2)), r !== e) {
        for (s2 = [], o = t.charAt(n), Su2.test(o) ? n++ : (o = e, h === 0 && d2(zu2)); o !== e; )
          s2.push(o), o = t.charAt(n), Su2.test(o) ? n++ : (o = e, h === 0 && d2(zu2));
        r = [r, s2], u2 = r;
      } else
        n = u2, u2 = e;
      return u2;
    }
    function wr2() {
      var u2, r, s2, o, B3, x2, j3, K3;
      if (u2 = n, t.substr(n, 3) === ku2 ? (r = ku2, n += 3) : (r = e, h === 0 && d2(rt3)), r !== e)
        if (E3(), t.charCodeAt(n) === 91 ? (s2 = p, n++) : (s2 = e, h === 0 && d2(Ou2)), s2 !== e)
          if (E3(), o = J3(), o !== e) {
            for (B3 = [], x2 = n, j3 = G2(), j3 !== e ? (K3 = J3(), K3 !== e ? ($3 = x2, x2 = Tu2(o, K3)) : (n = x2, x2 = e)) : (n = x2, x2 = e); x2 !== e; )
              B3.push(x2), x2 = n, j3 = G2(), j3 !== e ? (K3 = J3(), K3 !== e ? ($3 = x2, x2 = Tu2(o, K3)) : (n = x2, x2 = e)) : (n = x2, x2 = e);
            x2 = E3(), t.charCodeAt(n) === 93 ? (j3 = w3, n++) : (j3 = e, h === 0 && d2(Mu2)), j3 !== e ? ($3 = u2, u2 = Ot3(o, B3)) : (n = u2, u2 = e);
          } else
            n = u2, u2 = e;
        else
          n = u2, u2 = e;
      else
        n = u2, u2 = e;
      return u2;
    }
    function br2() {
      var u2;
      return u2 = wr2(), u2 === e && (u2 = Br2()), u2;
    }
    function J3() {
      var u2, r, s2, o, B3;
      if (u2 = n, r = br2(), r !== e) {
        for (E3(), s2 = [], o = Fu2(); o !== e; )
          s2.push(o), o = Fu2();
        $3 = u2, u2 = Mt3(r);
      } else
        n = u2, u2 = e;
      return u2 === e && (u2 = n, r = Cr2(), r !== e ? (E3(), t.charCodeAt(n) === 36 ? (s2 = Ae3, n++) : (s2 = e, h === 0 && d2(nt2)), s2 !== e ? (o = E3(), B3 = J3(), B3 !== e ? ($3 = u2, u2 = zt3(r, B3)) : (n = u2, u2 = e)) : (n = u2, u2 = e)) : (n = u2, u2 = e)), u2;
    }
    function xr2() {
      var u2, r;
      return u2 = n, r = J3(), r !== e && ($3 = u2, r = Tt2(r)), u2 = r, u2 === e && (u2 = Fu2()), u2;
    }
    function Ir2() {
      var u2;
      return u2 = xr2(), u2;
    }
    function kr2() {
      var u2, r;
      return u2 = n, E3(), r = Nr2(), r === e && (r = Pr2(), r === e && (r = qr2())), r !== e ? (E3(), $3 = u2, u2 = Zt3(r)) : (n = u2, u2 = e), u2;
    }
    function Nr2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 6) === Nu2 ? (r = Nu2, n += 6) : (r = e, h === 0 && d2(st2)), r !== e ? (E3(), s2 = M2(), s2 !== e ? ($3 = u2, u2 = Wt3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function Pr2() {
      var u2, r, s2;
      return u2 = n, t.substr(n, 6) === Pu2 ? (r = Pu2, n += 6) : (r = e, h === 0 && d2(it3)), r !== e ? (E3(), s2 = M2(), s2 !== e ? ($3 = u2, u2 = Ut3(s2)) : (n = u2, u2 = e)) : (n = u2, u2 = e), u2;
    }
    function qr2() {
      var u2, r;
      return u2 = n, t.substr(n, 4) === qu2 ? (r = qu2, n += 4) : (r = e, h === 0 && d2(ft2)), r !== e && ($3 = u2, r = Vt2()), u2 = r, u2;
    }
    function jr2() {
      var u2;
      return u2 = Ir2(), u2 === e && (u2 = kr2()), u2;
    }
    var Sr2 = function(u2) {
      this.type_ = "atom", this.source_ = u2, this.location_ = Zu2();
    }, lu2 = function(u2, r, s2, o) {
      this.type_ = "pattern", this.arguments_ = { alignment: r, _steps: o }, s2 !== void 0 && (this.arguments_.seed = s2), this.source_ = u2;
    }, Rr2 = function(u2, r, s2) {
      this.type_ = u2, this.arguments_ = r, this.source_ = s2;
    }, Lr2 = function(u2, r) {
      this.type_ = "element", this.source_ = u2, this.options_ = r, this.location_ = Zu2();
    }, hu2 = function(u2, r) {
      this.type_ = "command", this.name_ = u2, this.options_ = r;
    }, Bu2 = 0;
    if (eu2 = a2(), i.peg$library)
      return (
        /** @type {any} */
        {
          peg$result: eu2,
          peg$currPos: n,
          peg$FAILED: e,
          peg$maxFailExpected: fu2,
          peg$maxFailPos: q3
        }
      );
    if (eu2 !== e && n === t.length)
      return eu2;
    throw eu2 !== e && n < t.length && d2(Gt2()), Yt3(
      fu2,
      q3 < t.length ? t.charAt(q3) : null,
      q3 < t.length ? gu2(q3, q3 + 1) : gu2(q3, q3)
    );
  }
  var Gr2 = [
    "start"
  ];
  typeof BigInt > "u" && (BigInt = function(t) {
    if (isNaN(t)) throw new Error("");
    return t;
  });
  var C3 = BigInt(0);
  var m2 = BigInt(1);
  var ru2 = BigInt(2);
  var Du2 = BigInt(5);
  var N3 = BigInt(10);
  var zr2 = 2e3;
  var A2 = {
    s: m2,
    n: C3,
    d: m2
  };
  function z2(t, i) {
    try {
      t = BigInt(t);
    } catch {
      throw Z3();
    }
    return t * i;
  }
  function S3(t) {
    return typeof t == "bigint" ? t : Math.floor(t);
  }
  function I(t, i) {
    if (i === C3)
      throw du2();
    const e = Object.create(b.prototype);
    e.s = t < C3 ? -m2 : m2, t = t < C3 ? -t : t;
    const f3 = U3(t, i);
    return e.n = t / f3, e.d = i / f3, e;
  }
  function Q3(t) {
    const i = {};
    let e = t, f3 = ru2, l2 = Du2 - m2;
    for (; l2 <= e; ) {
      for (; e % f3 === C3; )
        e /= f3, i[f3] = (i[f3] || C3) + m2;
      l2 += m2 + ru2 * f3++;
    }
    return e !== t ? e > 1 && (i[e] = (i[e] || C3) + m2) : i[t] = (i[t] || C3) + m2, i;
  }
  var k2 = function(t, i) {
    let e = C3, f3 = m2, l2 = m2;
    if (t != null) if (i !== void 0) {
      if (typeof t == "bigint")
        e = t;
      else {
        if (isNaN(t))
          throw Z3();
        if (t % 1 !== 0)
          throw Qu2();
        e = BigInt(t);
      }
      if (typeof i == "bigint")
        f3 = i;
      else {
        if (isNaN(i))
          throw Z3();
        if (i % 1 !== 0)
          throw Qu2();
        f3 = BigInt(i);
      }
      l2 = e * f3;
    } else if (typeof t == "object") {
      if ("d" in t && "n" in t)
        e = BigInt(t.n), f3 = BigInt(t.d), "s" in t && (e *= BigInt(t.s));
      else if (0 in t)
        e = BigInt(t[0]), 1 in t && (f3 = BigInt(t[1]));
      else if (typeof t == "bigint")
        e = t;
      else
        throw Z3();
      l2 = e * f3;
    } else if (typeof t == "number") {
      if (isNaN(t))
        throw Z3();
      if (t < 0 && (l2 = -m2, t = -t), t % 1 === 0)
        e = BigInt(t);
      else if (t > 0) {
        let a2 = 1, D2 = 0, v = 1, g2 = 1, c3 = 1, F3 = 1e7;
        for (t >= 1 && (a2 = 10 ** Math.floor(1 + Math.log10(t)), t /= a2); v <= F3 && c3 <= F3; ) {
          let p = (D2 + g2) / (v + c3);
          if (t === p) {
            v + c3 <= F3 ? (e = D2 + g2, f3 = v + c3) : c3 > v ? (e = g2, f3 = c3) : (e = D2, f3 = v);
            break;
          } else
            t > p ? (D2 += g2, v += c3) : (g2 += D2, c3 += v), v > F3 ? (e = g2, f3 = c3) : (e = D2, f3 = v);
        }
        e = BigInt(e) * BigInt(a2), f3 = BigInt(f3);
      }
    } else if (typeof t == "string") {
      let a2 = 0, D2 = C3, v = C3, g2 = C3, c3 = m2, F3 = m2, p = t.replace(/_/g, "").match(/\d+|./g);
      if (p === null)
        throw Z3();
      if (p[a2] === "-" ? (l2 = -m2, a2++) : p[a2] === "+" && a2++, p.length === a2 + 1 ? v = z2(p[a2++], l2) : p[a2 + 1] === "." || p[a2] === "." ? (p[a2] !== "." && (D2 = z2(p[a2++], l2)), a2++, (a2 + 1 === p.length || p[a2 + 1] === "(" && p[a2 + 3] === ")" || p[a2 + 1] === "'" && p[a2 + 3] === "'") && (v = z2(p[a2], l2), c3 = N3 ** BigInt(p[a2].length), a2++), (p[a2] === "(" && p[a2 + 2] === ")" || p[a2] === "'" && p[a2 + 2] === "'") && (g2 = z2(p[a2 + 1], l2), F3 = N3 ** BigInt(p[a2 + 1].length) - m2, a2 += 3)) : p[a2 + 1] === "/" || p[a2 + 1] === ":" ? (v = z2(p[a2], l2), c3 = z2(p[a2 + 2], m2), a2 += 3) : p[a2 + 3] === "/" && p[a2 + 1] === " " && (D2 = z2(p[a2], l2), v = z2(p[a2 + 2], l2), c3 = z2(p[a2 + 4], m2), a2 += 5), p.length <= a2)
        f3 = c3 * F3, l2 = /* void */
        e = g2 + f3 * D2 + F3 * v;
      else
        throw Z3();
    } else if (typeof t == "bigint")
      e = t, l2 = t, f3 = m2;
    else
      throw Z3();
    if (f3 === C3)
      throw du2();
    A2.s = l2 < C3 ? -m2 : m2, A2.n = e < C3 ? -e : e, A2.d = f3 < C3 ? -f3 : f3;
  };
  function Tr2(t, i, e) {
    let f3 = m2;
    for (; i > C3; t = t * t % e, i >>= m2)
      i & m2 && (f3 = f3 * t % e);
    return f3;
  }
  function Zr2(t, i) {
    for (; i % ru2 === C3; i /= ru2)
      ;
    for (; i % Du2 === C3; i /= Du2)
      ;
    if (i === m2)
      return C3;
    let e = N3 % i, f3 = 1;
    for (; e !== m2; f3++)
      if (e = e * N3 % i, f3 > zr2)
        return C3;
    return BigInt(f3);
  }
  function Wr2(t, i, e) {
    let f3 = m2, l2 = Tr2(N3, e, i);
    for (let a2 = 0; a2 < 300; a2++) {
      if (f3 === l2)
        return BigInt(a2);
      f3 = f3 * N3 % i, l2 = l2 * N3 % i;
    }
    return 0;
  }
  function U3(t, i) {
    if (!t)
      return i;
    if (!i)
      return t;
    for (; ; ) {
      if (t %= i, !t)
        return i;
      if (i %= t, !i)
        return t;
    }
  }
  function b(t, i) {
    if (k2(t, i), this instanceof b)
      t = U3(A2.d, A2.n), this.s = A2.s, this.n = A2.n / t, this.d = A2.d / t;
    else
      return I(A2.s * A2.n, A2.d);
  }
  var du2 = function() {
    return new Error("Division by Zero");
  };
  var Z3 = function() {
    return new Error("Invalid argument");
  };
  var Qu2 = function() {
    return new Error("Parameters must be integer");
  };
  b.prototype = {
    s: m2,
    n: C3,
    d: m2,
    /**
     * Calculates the absolute value
     *
     * Ex: new Fraction(-4).abs() => 4
     **/
    abs: function() {
      return I(this.n, this.d);
    },
    /**
     * Inverts the sign of the current fraction
     *
     * Ex: new Fraction(-4).neg() => 4
     **/
    neg: function() {
      return I(-this.s * this.n, this.d);
    },
    /**
     * Adds two rational numbers
     *
     * Ex: new Fraction({n: 2, d: 3}).add("14.9") => 467 / 30
     **/
    add: function(t, i) {
      return k2(t, i), I(
        this.s * this.n * A2.d + A2.s * this.d * A2.n,
        this.d * A2.d
      );
    },
    /**
     * Subtracts two rational numbers
     *
     * Ex: new Fraction({n: 2, d: 3}).add("14.9") => -427 / 30
     **/
    sub: function(t, i) {
      return k2(t, i), I(
        this.s * this.n * A2.d - A2.s * this.d * A2.n,
        this.d * A2.d
      );
    },
    /**
     * Multiplies two rational numbers
     *
     * Ex: new Fraction("-17.(345)").mul(3) => 5776 / 111
     **/
    mul: function(t, i) {
      return k2(t, i), I(
        this.s * A2.s * this.n * A2.n,
        this.d * A2.d
      );
    },
    /**
     * Divides two rational numbers
     *
     * Ex: new Fraction("-17.(345)").inverse().div(3)
     **/
    div: function(t, i) {
      return k2(t, i), I(
        this.s * A2.s * this.n * A2.d,
        this.d * A2.n
      );
    },
    /**
     * Clones the actual object
     *
     * Ex: new Fraction("-17.(345)").clone()
     **/
    clone: function() {
      return I(this.s * this.n, this.d);
    },
    /**
     * Calculates the modulo of two rational numbers - a more precise fmod
     *
     * Ex: new Fraction('4.(3)').mod([7, 8]) => (13/3) % (7/8) = (5/6)
     * Ex: new Fraction(20, 10).mod().equals(0) ? "is Integer"
     **/
    mod: function(t, i) {
      if (t === void 0)
        return I(this.s * this.n % this.d, m2);
      if (k2(t, i), C3 === A2.n * this.d)
        throw du2();
      return I(
        this.s * (A2.d * this.n) % (A2.n * this.d),
        A2.d * this.d
      );
    },
    /**
     * Calculates the fractional gcd of two rational numbers
     *
     * Ex: new Fraction(5,8).gcd(3,7) => 1/56
     */
    gcd: function(t, i) {
      return k2(t, i), I(U3(A2.n, this.n) * U3(A2.d, this.d), A2.d * this.d);
    },
    /**
     * Calculates the fractional lcm of two rational numbers
     *
     * Ex: new Fraction(5,8).lcm(3,7) => 15
     */
    lcm: function(t, i) {
      return k2(t, i), A2.n === C3 && this.n === C3 ? I(C3, m2) : I(A2.n * this.n, U3(A2.n, this.n) * U3(A2.d, this.d));
    },
    /**
     * Gets the inverse of the fraction, means numerator and denominator are exchanged
     *
     * Ex: new Fraction([-3, 4]).inverse() => -4 / 3
     **/
    inverse: function() {
      return I(this.s * this.d, this.n);
    },
    /**
     * Calculates the fraction to some integer exponent
     *
     * Ex: new Fraction(-1,2).pow(-3) => -8
     */
    pow: function(t, i) {
      if (k2(t, i), A2.d === m2)
        return A2.s < C3 ? I((this.s * this.d) ** A2.n, this.n ** A2.n) : I((this.s * this.n) ** A2.n, this.d ** A2.n);
      if (this.s < C3) return null;
      let e = Q3(this.n), f3 = Q3(this.d), l2 = m2, a2 = m2;
      for (let D2 in e)
        if (D2 !== "1") {
          if (D2 === "0") {
            l2 = C3;
            break;
          }
          if (e[D2] *= A2.n, e[D2] % A2.d === C3)
            e[D2] /= A2.d;
          else return null;
          l2 *= BigInt(D2) ** e[D2];
        }
      for (let D2 in f3)
        if (D2 !== "1") {
          if (f3[D2] *= A2.n, f3[D2] % A2.d === C3)
            f3[D2] /= A2.d;
          else return null;
          a2 *= BigInt(D2) ** f3[D2];
        }
      return A2.s < C3 ? I(a2, l2) : I(l2, a2);
    },
    /**
     * Calculates the logarithm of a fraction to a given rational base
     *
     * Ex: new Fraction(27, 8).log(9, 4) => 3/2
     */
    log: function(t, i) {
      if (k2(t, i), this.s <= C3 || A2.s <= C3) return null;
      const e = {}, f3 = Q3(A2.n), l2 = Q3(A2.d), a2 = Q3(this.n), D2 = Q3(this.d);
      for (const c3 in l2)
        f3[c3] = (f3[c3] || C3) - l2[c3];
      for (const c3 in D2)
        a2[c3] = (a2[c3] || C3) - D2[c3];
      for (const c3 in f3)
        c3 !== "1" && (e[c3] = true);
      for (const c3 in a2)
        c3 !== "1" && (e[c3] = true);
      let v = null, g2 = null;
      for (const c3 in e) {
        const F3 = f3[c3] || C3, p = a2[c3] || C3;
        if (F3 === C3) {
          if (p !== C3)
            return null;
          continue;
        }
        let w3 = p, P3 = F3;
        const R2 = U3(w3, P3);
        if (w3 /= R2, P3 /= R2, v === null && g2 === null)
          v = w3, g2 = P3;
        else if (w3 * g2 !== v * P3)
          return null;
      }
      return v !== null && g2 !== null ? I(v, g2) : null;
    },
    /**
     * Check if two rational numbers are the same
     *
     * Ex: new Fraction(19.6).equals([98, 5]);
     **/
    equals: function(t, i) {
      return k2(t, i), this.s * this.n * A2.d === A2.s * A2.n * this.d;
    },
    /**
     * Check if this rational number is less than another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    lt: function(t, i) {
      return k2(t, i), this.s * this.n * A2.d < A2.s * A2.n * this.d;
    },
    /**
     * Check if this rational number is less than or equal another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    lte: function(t, i) {
      return k2(t, i), this.s * this.n * A2.d <= A2.s * A2.n * this.d;
    },
    /**
     * Check if this rational number is greater than another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    gt: function(t, i) {
      return k2(t, i), this.s * this.n * A2.d > A2.s * A2.n * this.d;
    },
    /**
     * Check if this rational number is greater than or equal another
     *
     * Ex: new Fraction(19.6).lt([98, 5]);
     **/
    gte: function(t, i) {
      return k2(t, i), this.s * this.n * A2.d >= A2.s * A2.n * this.d;
    },
    /**
     * Compare two rational numbers
     * < 0 iff this < that
     * > 0 iff this > that
     * = 0 iff this = that
     *
     * Ex: new Fraction(19.6).compare([98, 5]);
     **/
    compare: function(t, i) {
      k2(t, i);
      let e = this.s * this.n * A2.d - A2.s * A2.n * this.d;
      return (C3 < e) - (e < C3);
    },
    /**
     * Calculates the ceil of a rational number
     *
     * Ex: new Fraction('4.(3)').ceil() => (5 / 1)
     **/
    ceil: function(t) {
      return t = N3 ** BigInt(t || 0), I(
        S3(this.s * t * this.n / this.d) + (t * this.n % this.d > C3 && this.s >= C3 ? m2 : C3),
        t
      );
    },
    /**
     * Calculates the floor of a rational number
     *
     * Ex: new Fraction('4.(3)').floor() => (4 / 1)
     **/
    floor: function(t) {
      return t = N3 ** BigInt(t || 0), I(
        S3(this.s * t * this.n / this.d) - (t * this.n % this.d > C3 && this.s < C3 ? m2 : C3),
        t
      );
    },
    /**
     * Rounds a rational numbers
     *
     * Ex: new Fraction('4.(3)').round() => (4 / 1)
     **/
    round: function(t) {
      return t = N3 ** BigInt(t || 0), I(
        S3(this.s * t * this.n / this.d) + this.s * ((this.s >= C3 ? m2 : C3) + ru2 * (t * this.n % this.d) > this.d ? m2 : C3),
        t
      );
    },
    /**
      * Rounds a rational number to a multiple of another rational number
      *
      * Ex: new Fraction('0.9').roundTo("1/8") => 7 / 8
      **/
    roundTo: function(t, i) {
      k2(t, i);
      const e = this.n * A2.d, f3 = this.d * A2.n, l2 = e % f3;
      let a2 = S3(e / f3);
      return l2 + l2 >= f3 && a2++, I(this.s * a2 * A2.n, A2.d);
    },
    /**
     * Check if two rational numbers are divisible
     *
     * Ex: new Fraction(19.6).divisible(1.5);
     */
    divisible: function(t, i) {
      return k2(t, i), !(!(A2.n * this.d) || this.n * A2.d % (A2.n * this.d));
    },
    /**
     * Returns a decimal representation of the fraction
     *
     * Ex: new Fraction("100.'91823'").valueOf() => 100.91823918239183
     **/
    valueOf: function() {
      return Number(this.s * this.n) / Number(this.d);
    },
    /**
     * Creates a string representation of a fraction with all digits
     *
     * Ex: new Fraction("100.'91823'").toString() => "100.(91823)"
     **/
    toString: function(t) {
      let i = this.n, e = this.d;
      t = t || 15;
      let f3 = Zr2(i, e), l2 = Wr2(i, e, f3), a2 = this.s < C3 ? "-" : "";
      if (a2 += S3(i / e), i %= e, i *= N3, i && (a2 += "."), f3) {
        for (let D2 = l2; D2--; )
          a2 += S3(i / e), i %= e, i *= N3;
        a2 += "(";
        for (let D2 = f3; D2--; )
          a2 += S3(i / e), i %= e, i *= N3;
        a2 += ")";
      } else
        for (let D2 = t; i && D2--; )
          a2 += S3(i / e), i %= e, i *= N3;
      return a2;
    },
    /**
     * Returns a string-fraction representation of a Fraction object
     *
     * Ex: new Fraction("1.'3'").toFraction() => "4 1/3"
     **/
    toFraction: function(t) {
      let i = this.n, e = this.d, f3 = this.s < C3 ? "-" : "";
      if (e === m2)
        f3 += i;
      else {
        let l2 = S3(i / e);
        t && l2 > C3 && (f3 += l2, f3 += " ", i %= e), f3 += i, f3 += "/", f3 += e;
      }
      return f3;
    },
    /**
     * Returns a latex representation of a Fraction object
     *
     * Ex: new Fraction("1.'3'").toLatex() => "\frac{4}{3}"
     **/
    toLatex: function(t) {
      let i = this.n, e = this.d, f3 = this.s < C3 ? "-" : "";
      if (e === m2)
        f3 += i;
      else {
        let l2 = S3(i / e);
        t && l2 > C3 && (f3 += l2, i %= e), f3 += "\\frac{", f3 += i, f3 += "}{", f3 += e, f3 += "}";
      }
      return f3;
    },
    /**
     * Returns an array of continued fraction elements
     *
     * Ex: new Fraction("7/8").toContinued() => [0,1,7]
     */
    toContinued: function() {
      let t = this.n, i = this.d, e = [];
      do {
        e.push(S3(t / i));
        let f3 = t % i;
        t = i, i = f3;
      } while (t !== m2);
      return e;
    },
    simplify: function(t) {
      const i = BigInt(1 / (t || 1e-3) | 0), e = this.abs(), f3 = e.toContinued();
      for (let l2 = 1; l2 < f3.length; l2++) {
        let a2 = I(f3[l2 - 1], m2);
        for (let v = l2 - 2; v >= 0; v--)
          a2 = a2.inverse().add(f3[v]);
        let D2 = a2.sub(e);
        if (D2.n * i < D2.d)
          return a2.mul(this.s);
      }
      return this;
    }
  };
  var L3 = class _L {
    constructor(i, e) {
      this.begin = W3(i), this.end = W3(e);
    }
    get spanCycles() {
      const i = [];
      var e = this.begin;
      const f3 = this.end, l2 = f3.sam();
      if (e.equals(f3))
        return [new _L(e, f3)];
      for (; f3.gt(e); ) {
        if (e.sam().equals(l2)) {
          i.push(new _L(e, this.end));
          break;
        }
        const a2 = e.nextSam();
        i.push(new _L(e, a2)), e = a2;
      }
      return i;
    }
    get duration() {
      return this.end.sub(this.begin);
    }
    cycleArc() {
      const i = this.begin.cyclePos(), e = i.add(this.duration);
      return new _L(i, e);
    }
    withTime(i) {
      return new _L(i(this.begin), i(this.end));
    }
    withEnd(i) {
      return new _L(this.begin, i(this.end));
    }
    withCycle(i) {
      const e = this.begin.sam(), f3 = e.add(i(this.begin.sub(e))), l2 = e.add(i(this.end.sub(e)));
      return new _L(f3, l2);
    }
    intersection(i) {
      const e = this.begin.max(i.begin), f3 = this.end.min(i.end);
      if (!e.gt(f3) && !(e.equals(f3) && (e.equals(this.end) && this.begin.lt(this.end) || e.equals(i.end) && i.begin.lt(i.end))))
        return new _L(e, f3);
    }
    intersection_e(i) {
      const e = this.intersection(i);
      if (e == null)
        throw "TimeSpans do not intersect";
      return e;
    }
    midpoint() {
      return this.begin.add(this.duration.div(W3(2)));
    }
    equals(i) {
      return this.begin.equals(i.begin) && this.end.equals(i.end);
    }
    show() {
      return this.begin.show() + " \u2192 " + this.end.show();
    }
  };
  var Ur2 = (t) => t.filter((i) => i != null);
  b.prototype.sam = function() {
    return this.floor();
  };
  b.prototype.nextSam = function() {
    return this.sam().add(1);
  };
  b.prototype.wholeCycle = function() {
    return new L3(this.sam(), this.nextSam());
  };
  b.prototype.cyclePos = function() {
    return this.sub(this.sam());
  };
  b.prototype.lt = function(t) {
    return this.compare(t) < 0;
  };
  b.prototype.gt = function(t) {
    return this.compare(t) > 0;
  };
  b.prototype.lte = function(t) {
    return this.compare(t) <= 0;
  };
  b.prototype.gte = function(t) {
    return this.compare(t) >= 0;
  };
  b.prototype.eq = function(t) {
    return this.compare(t) == 0;
  };
  b.prototype.ne = function(t) {
    return this.compare(t) != 0;
  };
  b.prototype.max = function(t) {
    return this.gt(t) ? this : t;
  };
  b.prototype.maximum = function(...t) {
    return t = t.map((i) => new b(i)), t.reduce((i, e) => e.max(i), this);
  };
  b.prototype.min = function(t) {
    return this.lt(t) ? this : t;
  };
  b.prototype.mulmaybe = function(t) {
    return t !== void 0 ? this.mul(t) : void 0;
  };
  b.prototype.divmaybe = function(t) {
    return t !== void 0 ? this.div(t) : void 0;
  };
  b.prototype.addmaybe = function(t) {
    return t !== void 0 ? this.add(t) : void 0;
  };
  b.prototype.submaybe = function(t) {
    return t !== void 0 ? this.sub(t) : void 0;
  };
  b.prototype.show = function() {
    return this.s * this.n + "/" + this.d;
  };
  b.prototype.or = function(t) {
    return this.eq(0) ? t : this;
  };
  var W3 = (t) => b(t);
  var cu2 = (...t) => {
    if (t = Ur2(t), t.length === 0)
      return;
    const i = t.pop();
    return t.reduce(
      (e, f3) => e === void 0 || f3 === void 0 ? void 0 : e.lcm(f3),
      i
    );
  };
  W3._original = b;
  var ue3 = 3e-4;
  var Vr2 = (t, i) => (e, f3) => {
    const D2 = t.source_[f3].options_?.ops, v = e.__steps_source;
    if (D2)
      for (const g2 of D2)
        switch (g2.type_) {
          case "stretch": {
            const c3 = ["fast", "slow"], { type: F3, amount: p } = g2.arguments_;
            if (!c3.includes(F3))
              throw new Error(`mini: stretch: type must be one of ${c3.join("|")} but got ${F3}`);
            e = d(e)[F3](i(p));
            break;
          }
          case "replicate": {
            const { amount: c3 } = g2.arguments_;
            e = d(e), e = e._repeatCycles(c3)._fast(c3);
            break;
          }
          case "bjorklund": {
            g2.arguments_.rotation ? e = e.euclidRot(i(g2.arguments_.pulse), i(g2.arguments_.step), i(g2.arguments_.rotation)) : e = e.euclid(i(g2.arguments_.pulse), i(g2.arguments_.step));
            break;
          }
          case "degradeBy": {
            e = d(e)._degradeByWith(W2.early(ue3 * g2.arguments_.seed), g2.arguments_.amount ?? 0.5);
            break;
          }
          case "tail": {
            const c3 = i(g2.arguments_.element);
            e = e.fmap((F3) => (p) => Array.isArray(F3) ? [...F3, p] : [F3, p]).appLeft(c3);
            break;
          }
          case "range": {
            const c3 = i(g2.arguments_.element);
            e = d(e);
            const F3 = (w3, P3, R2 = 1) => Array.from(
              { length: Math.abs(P3 - w3) / R2 + 1 },
              (su2, iu2) => w3 < P3 ? w3 + iu2 * R2 : w3 - iu2 * R2
            );
            e = ((w3, P3) => w3.squeezeBind((R2) => P3.bind((su2) => N2(...F3(R2, su2)))))(e, c3);
            break;
          }
          default:
            console.warn(`operator "${g2.type_}" not implemented`);
        }
    return e.__steps_source = e.__steps_source || v, e;
  };
  function nu2(t, i, e, f3 = 0) {
    e?.(t);
    const l2 = (a2) => nu2(a2, i, e, f3);
    switch (t.type_) {
      case "pattern": {
        const a2 = t.source_.map((c3) => l2(c3)).map(Vr2(t, l2)), D2 = t.arguments_.alignment, v = a2.filter((c3) => c3.__steps_source);
        let g2;
        switch (D2) {
          case "stack": {
            g2 = z(...a2), v.length && (g2._steps = cu2(...v.map((c3) => W3(c3._steps))));
            break;
          }
          case "polymeter_slowcat": {
            g2 = z(...a2.map((c3) => c3._slow(c3.__weight))), v.length && (g2._steps = cu2(...v.map((c3) => W3(c3._steps))));
            break;
          }
          case "polymeter": {
            const c3 = t.arguments_.stepsPerCycle ? l2(t.arguments_.stepsPerCycle).fmap((p) => m(p)) : C2(m(a2.length > 0 ? a2[0].__weight : 1)), F3 = a2.map((p) => p.fast(c3.fmap((w3) => w3.div(p.__weight))));
            g2 = z(...F3);
            break;
          }
          case "rand": {
            g2 = Pe2(W2.early(ue3 * t.arguments_.seed).segment(1), a2), v.length && (g2._steps = cu2(...v.map((c3) => W3(c3._steps))));
            break;
          }
          case "feet": {
            g2 = N2(...a2);
            break;
          }
          default: {
            if (t.source_.some((F3) => !!F3.options_?.weight)) {
              const F3 = t.source_.reduce(
                (p, w3) => p.add(w3.options_?.weight || m(1)),
                m(0)
              );
              g2 = ns(
                ...t.source_.map((p, w3) => [p.options_?.weight || m(1), a2[w3]])
              ), g2.__weight = F3, g2._steps = F3, v.length && (g2._steps = g2._steps.mul(cu2(...v.map((p) => W3(p._steps)))));
            } else
              g2 = Q2(...a2), g2._steps = a2.length;
            t.arguments_._steps && (g2.__steps_source = true);
          }
        }
        return v.length && (g2.__steps_source = true), g2;
      }
      case "element":
        return l2(t.source_);
      case "atom": {
        if (t.source_ === "~" || t.source_ === "-")
          return q2;
        if (!t.location_)
          return console.warn("no location for", t), t.source_;
        const a2 = isNaN(Number(t.source_)) ? t.source_ : Number(t.source_);
        if (f3 === -1)
          return C2(a2);
        const [D2, v] = ee3(i, t, f3);
        return C2(a2).withLoc(D2, v);
      }
      case "stretch":
        return l2(t.source_).slow(l2(t.arguments_.amount));
      default:
        return console.warn(`node type "${t.type_}" not implemented -> returning silence`), q2;
    }
  }
  var ee3 = (t, i, e = 0) => {
    const { start: f3, end: l2 } = i.location_, a2 = t?.split("").slice(f3.offset, l2.offset).join(""), [D2 = 0, v = 0] = a2 ? a2.split(i.source_).map((g2) => g2.split("").filter((c3) => c3 === " ").length) : [];
    return [f3.offset + D2 + e, l2.offset - v + e];
  };
  var Au2 = (t, i = 0, e = t) => {
    try {
      return Mr2(t);
    } catch (f3) {
      const l2 = [f3.location.start.offset + i, f3.location.end.offset + i], a2 = e.slice(0, l2[0]).split(`
`).length;
      throw new Error(`[mini] parse error at line ${a2}: ${f3.message}`);
    }
  };
  var Xr2 = (t, i, e) => {
    const f3 = Au2(t, i, e);
    let l2 = [];
    return nu2(
      f3,
      t,
      (a2) => {
        a2.type_ === "atom" && l2.push(a2);
      },
      -1
    ), l2;
  };
  var Yr2 = (t, i = 0, e) => Xr2(t, i, e).map((f3) => ee3(t, f3, i));
  var te3 = (...t) => {
    const i = t.map((e) => {
      const f3 = `"${e}"`, l2 = Au2(f3);
      return nu2(l2, f3);
    });
    return Q2(...i);
  };
  var Hr2 = (t, i) => {
    const e = `"${t}"`, f3 = Au2(e);
    return nu2(f3, e, null, i);
  };
  var Jr2 = (t) => {
    const i = Au2(t);
    return nu2(i, t);
  };
  function Kr2(t) {
    return typeof t == "string" ? te3(t) : d(t);
  }
  function Qr2() {
    mh(te3);
  }

  // entry.mjs
  globalThis.Strudel = { ...dist_exports, ...dist_exports2 };
})();
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
