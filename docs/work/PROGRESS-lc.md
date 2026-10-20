# Läge LC-spike (arbetare 2, gren spike-lc)
- 20 okt: spike klar, rapport i [lc-spike-report.md](lc-spike-report.md). Fungerar; rekommenderar QuickJS + Strudel.

# LC-bygget (projektledaren, gren content-lc, worktree KickBase-VST-live)
Plan: (1) motor `Source/Core/LiveCode.h/.cpp` + bunt i BinaryData + `live_code_create` (klart i kod 20 okt, test kvar);
(2) tester i Tools/ (mappning, fel, tidsgräns); (3) Live-läge: bakgrundstråd som frågar queryArc 2-4 cykler fram, kö till schemaläggaren;
(4) UI-flik LIVE CODE med reglage + kodblock i chatten (Play/Stop/Create); (5) transpiler (`$:`, `.p()`, reglage-id).
Mappning: 1 cykel = 1 takt = 384 tick. s("bd"/"kick") = kickraden (steg = round(b*16)); s("i1".."i10") = instrument; note/n utan s = instrument 1.
Pitch: C2 = MIDI 36 = pitch 0 (ton utan oktav = oktav 3, som i Strudel). gain = velocity (standard 0.8). En not per tick tills polyfonin finns.
Strudel-banner: AGPL, visas i UI/THIRD-PARTY.md (kvar att göra).
