var console = (function () { var out = function (level) { return function () { var s = []; for (var i = 0; i < arguments.length; i++) { try { s.push(typeof arguments[i] === "string" ? arguments[i] : JSON.stringify(arguments[i])); } catch (e) { s.push(String(arguments[i])); } } host.log(level, s.join(" ")); }; }; return { log: out("log"), info: out("log"), warn: out("warn"), error: out("error"), debug: function () {} }; })();
// Webbläsar-globaler som Strudel/kabelsalat rör vid inläsning. Stubbade, inget körs på riktigt.
var performance = { now: function () { return Date.now(); } };
var setTimeout = function () { return 0; }, clearTimeout = function () {}, setInterval = setTimeout, clearInterval = clearTimeout;
