var console = { log(){}, warn(){}, error(){}, debug(){}, info(){} };
// Webbläsar-globaler som Strudel/kabelsalat rör vid inläsning. Stubbade, inget körs på riktigt.
var performance = { now: function () { return Date.now(); } };
var setTimeout = function () { return 0; }, clearTimeout = function () {}, setInterval = setTimeout, clearInterval = clearTimeout;
