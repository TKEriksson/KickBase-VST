# Frågor till Tobias (granskningen, första körningen)

Varje fråga: sammanhang, alternativ, min rekommendation, vad som händer om du inte svarar, och status. Svara gärna bara med numret och
ett ord ("1: ja", "4: B"). Inget här är gjort förrän du svarat, utom där det står **gjort** (då är det lätt att backa).

---

## 1. Moduleringen följer DAW:ens buffertstorlek: ljudet blir olika i olika inställningar

**Sammanhang.** LFO:erna, makrona och instrumentens ENV 2/3, VELOCITY och RANDOM räknas _en gång per ljudblock_, och värdet ligger sedan
still under hela blocket ([Modulation.cpp:230](../../Source/Core/Modulation.cpp#L230), anropas från `InstrumentProcessor::processBlock`).
Ett block är det DAW:en skickar: 64–2048 samples beroende på ljudkortets buffert. FL Studio ändrar dessutom blockstorleken under
spelningen. Mätning med `tool blocksizetest` (samma ljud, olika blockstorlekar, nolltest mot block 32):

|                                             | block 64 | 256 | 512 | 1024 | 2048 |
| ------------------------------------------- | -------- | --- | --- | ---- | ---- |
| Init (LFO på cutoff): skillnadens topp      | −27 dBFS | −12 | −7  | −5.5 | −4.6 |
| Init: skillnaden under signalen (rms)       | 50 dB    | 33  | 26  | 20   | 16   |
| Tungt läge (LFO, ENV 2/3): skillnadens topp | −12 dBFS | −9  | −7  | −3   | −1.7 |

Alltså: samma projekt låter hörbart olika med buffert 256 och 2048, och exporten och AI-analysen (som renderar med block 512) låter
inte exakt som det du hör i FL. Med stora block går en snabb LFO i trappsteg (zipper), och ENV 2/3 hinner inte med korta toner.

**Alternativ.**

- **A (rekommenderas):** moduleringen räknas med fast takt, var 32:a sample, oavsett DAW:ens block. Ljudet blir detsamma i alla
  buffertstorlekar och mjukare med snabba LFO:er. Ljudet ändras jämfört med i dag (det låter som "block 32" i tabellen, alltså som i
  dag med en liten buffert). CPU: lite mer (jag mäter och optimerar så att bara modulerade rattar läses om).
- B: som A men var 64:e sample (billigare, nästan lika bra).
- C: låt det vara.

**Om du inte svarar:** inget ändras. **Status:** fråga (inte gjord).

--- SVAR: A

## 2. Live-spektrumet (ANALYZER) är tungt att rita

**Sammanhang.** Du sa att spektrumet känns tungt. Mätt med `tool uiprofile` (fönstret öppet, 10 tunga instrument, spektrumet på):
meddelandetråden använder ~44 % av en kärna, mot ~25 % med samma flik och spektrumet av. Själva analysen (FFT:erna) är bara ~4 %:
det dyra är att _rita_ fyllda, halvgenomskinliga kurvor för varje källa och krockfälten, 30 gånger per sekund. Jag har redan gjort det
som inte syns (tysta källor hoppar över FFT:n, inga kopior av kurvorna varje bildruta), pixel för pixel identiskt.

**Alternativ.**

- **A (rekommenderas):** rita med 20 bilder per sekund i stället för 30 (analysen oförändrad). Nästan inte märkbart, ~⅓ mindre ritning.
- B: SPEED: SLOW som standard (8 analyser/s) och 15 bilder/s.
- C: bara kanterna på kurvorna, ingen fyllning (ser annorlunda ut, mycket billigare).
- D: låt det vara.

**Om du inte svarar:** inget ändras. **Status:** fråga.

--- Svar: A. Man kanske ska kunna ställa in det till ännnu lägre i inställingar! Det finns ju redan "visa mindre animeringar", det här är ju liknande optimering (så gör en kategori: Optimeringar). Man kanske ska kunna välja 5 olika steg, där 1 är riktigt lågt typ 5 bilder per sekund upp till väldigt snabb typ 40. Så 1: 5, 2: 10, 3: 20, 4: 30, 5: 40 bilder per sek. Med kryssruta för om kurvorna ska ha fyllning (varna att det är mycket dyrare). Man ska också kunna anpassa hur många analyser per sekund som ska göras

## 3. ANALYSE (rapporten) tar mycket CPU medan den körs

**Sammanhang.** Du skrev att analysen "verkar väldigt krävande". Den körs redan på en egen tråd med låg prioritet. Jag har gjort den
snabbare utan att ändra resultatet: kopian som analyseras skapas på 123 ms i stället för 561 (mönstret lästes in kvadratiskt, se
rapporten), och en analys tar ~35 % kortare tid. Rapporterna är identiska före och efter.

**Alternativ (för att lämna mer åt DAW:en under tiden).**

- **A (rekommenderas):** kör analysen med Windows lägsta prioritet ("background": den använder bara CPU som annars står oanvänd,
  och Windows kan lägga den på strömsnåla kärnor). Ingen påverkan på DAW:ens ljud; analysen tar längre tid om datorn är upptagen.
- B: analysen pausar en kort stund efter varje bit (tar ungefär dubbelt så lång tid, jämn last).
- C: låt det vara.

**Om du inte svarar:** inget ändras. **Status:** fråga.

--- SVAR: A.

## 4. Sidechain-fabriksljuden sätter ett djup som inte finns längre

**Sammanhang.** Fabrikspresets för SIDECHAIN ("Classic Pump", "Tight 1/16", "Soft 1/8", "Hold Then Release") sätter `duckAmount`
(0,7–1,0). Den parametern finns inte sedan djupet blev per instrument (`bassNDuck`), så djupet ignoreras och alla får standard (0,8).
[Presets.cpp:77-83](../../Source/Core/Presets.cpp#L77-L83).

**Alternativ.** **A (rekommenderas):** värdet sätts på alla instrumentens DUCK. B: ta bort värdet ur presets (som i dag i praktiken).

**Om du inte svarar:** inget ändras. **Status:** fråga.

--- SVAR: A

## 5. Kick-presets rör inte längre kickens på/av — gjort

**Sammanhang.** Instrument-presets har medvetet aldrig sparat instrumentets på/av (det är radens M-knapp). Kick-presets sparade och
laddade däremot `kickOn`: ett kick-preset sparat med kicken avstängd stängde av kicken när det laddades. Nu är kicken som
instrumenten. Ingen av dina 165 presets låter annorlunda (kontrollerat).

**Status:** **gjort** (commit f2272d1). Säg till om du vill ha det som förut.

--- SVAR: LÅT DET VARA

## 6. Egna wavetables sparas nu i Full- och instrument-presets — gjort

**Sammanhang.** En oscillator med en egen WAV-tabell (User WAV) sparade bara valet "User WAV" i presetet, inte tabellen. Presetet
laddades sedan med fel ljud (den tabell som råkade finnas, eller Basic). Nu följer tabellen med (~44 kB per oscillator i filen). Äldre
instrument-presets utan tabeller lämnar lagrets tabell orörd (som förut).

**Status:** **gjort** (commit ee9a052).

--- SVAR: LÅT DET VARA

## 7. Filerna som ligger i repots rot

`current_ui.png`, `current_ui_pattern.png`, `new_ui_pattern.png`, `new_ui_synth.png`, `out.txt`, `test_out.txt` är äldre skärmbilder och
utdata från tidigare sessioner (enligt agenten som byggde dem). `createVst.bat` är ditt installationsskript. Ingen av dem är incheckad.

**Alternativ.** **A (rekommenderas):** jag raderar de sex gamla filerna och checkar in `createVst.bat`. B: låt dem ligga.

**Om du inte svarar:** inget raderas. **Status:** fråga.

--- SVAR: a.

## 8. Mappen och GitHub-repot heter fortfarande MyOwnFruit

**Sammanhang.** CMake-målen, skripten och dokumenten heter nu Kickbase (se rapporten). Kvar är själva mappen
`C:\Users\hunga\Documents\GitHub\MyOwnFruit` och repot `github.com/TKEriksson/MyOwnFruit`. Det kan jag inte byta mitt i en session:
Claude Codes minne är knutet till sökvägen, CMake-cachen i `build\` innehåller den, och att döpa om repot är en ändring på GitHub.
Plugin-identiteten påverkas inte (VST3-id:t räknas bara från `PLUGIN_MANUFACTURER_CODE` och `PLUGIN_CODE`, se rapporten), så FL hittar
dina projekt som förut.

**Om du vill byta (ca 15 minuter, plus ett bygge):**

1. Stäng FL Studio, VS Code och alla Claude Code-fönster. (Claude Desktops MCP-brygga ligger i `%LOCALAPPDATA%\Kickbase` och påverkas inte.)
2. Döp om mappen i Utforskaren: `Documents\GitHub\MyOwnFruit` → `Documents\GitHub\Kickbase`.
3. Kopiera Claude Codes minne (kopiera, så att det gamla finns kvar som backup):
   `C:\Users\hunga\.claude\projects\c--Users-hunga-Documents-GitHub-MyOwnFruit` →
   `C:\Users\hunga\.claude\projects\c--Users-hunga-Documents-GitHub-Kickbase`
4. Radera mappen `build` i den omdöpta mappen (CMake-cachen har den gamla sökvägen) och bygg om:
   `.\build.bat Release all` (tar en stund), sedan `createVst.bat` som administratör.
5. GitHub: repot → **Settings** → **General** → _Repository name_ → `Kickbase` → **Rename**. GitHub skickar den gamla adressen vidare.
   Sedan i den omdöpta mappen: `git remote set-url origin https://github.com/TKEriksson/Kickbase.git` och `git fetch` för att se att det fungerar.
6. Öppna mappen i VS Code och starta Claude Code där. Kör `install-mcp.ps1 Release` om du vill uppdatera bryggan också.

**Alternativ.** **A (rekommenderas):** gör det när granskningen är sammanslagen, så att inget annat pågår samtidigt. B: låt det vara
(allt fungerar med det gamla mappnamnet).

SVAR: A

**Om du inte svarar:** inget ändras. **Status:** fråga (bara du kan göra det).

---

## 9. Biblioteket: en personlig nivå? (innehållsplanen)

**Sammanhang.** Biblioteket (`Dokument\Kickbase\Library`) får ljud från CC0-källor, som får följa med när Kickbase delas. Vissa gratisljud
(t.ex. Looperman-loopar) får bara användas i egna låtar. Se [docs/content-plan.md](../content-plan.md).

**Alternativ.** **A (rekommenderas):** en PERSONAL-nivå: sådana ljud kan ligga i ditt bibliotek med ett märke, men följer aldrig med i en
installation och hamnar aldrig i repot. B: bara CC0 i biblioteket.

**Om du inte svarar:** bara CC0. **Status:** fråga.

---

## 10. Kickens SAMPLE-flik: MODE CLICK / LAYER / REPLACE

**Sammanhang.** KICK-sidan får två flikar, SYNTH (dagens) och SAMPLE (biblioteket med kickar). Kickmotorn kan redan spela en sampling som
klick, som lager ovanpå synten, eller i stället för synten (`kickSampleMode`). Flikarna byter bara vy; ljudet ändras av MODE.

**Alternativ.** **A (rekommenderas):** MODE-väljaren med alla tre ligger i SAMPLE-fliken; att välja en kick i biblioteket sätter REPLACE om
den stod på CLICK. B: bara REPLACE i SAMPLE-fliken.

**Om du inte svarar:** A. **Status:** fråga.

---

## 11. Polyfoni (för piano, klockor, music box, pads)

**Sammanhang.** Varje instrument spelar i dag en ton i taget: en ny ton klipper den förra. Det är rätt för bas och lead, men piano och
klockor med lång utklingning låter fel, och ackord går inte.

**Alternativ.** **A (rekommenderas):** en VOICES-ratt per instrument (1–8, standard 1 = exakt som i dag). B: låt det vara (piano och
klockor blir sämre).

**Om du inte svarar:** A byggs, med 1 som standard (inget ändras förrän du vrider på den). **Status:** fråga.
