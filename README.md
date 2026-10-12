# ANCIENT DRUMS
Generator kicków do hardstyle / rawstyle / uptempo / frenchcore. VST3 (instrument), działa w FL Studio.

## Instalacja (Windows)
Skopiuj folder `Ancient Drums.vst3` do `C:\Program Files\Common Files\VST3\`, potem w FL: *Options → Manage plugins → Find installed plugins*.
Plik `.vst3` buduje się automatycznie na GitHubie (zakładka **Actions** → ostatni build → **Artifacts**).

## Jak to działa
Każda nuta MIDI odpala kicka. Nuta **ROOT** (domyślnie C5) gra kicka bez transpozycji, wyższe/niższe nuty go stroją.

`PUNCH + CRUNCH + KICK (+ SUB)` → `RUMBLE` → `PRE-GAIN` → `CLIPPER (4× oversampling)` → `LIMITER` → `VOLUME`

### Warstwy PUNCH / CRUNCH / KICK
- **ON** – włącza/wyłącza warstwę (np. sam punch, albo pełny kick z sampla bez punch i crunch).
- Lista – 8 sampli z wbudowanej biblioteki albo **USER SAMPLE**.
- **LOAD** albo **przeciągnij plik** (wav / aiff / flac / ogg / mp3) na panel – wczytuje własny sample.
- **LEVEL, PITCH** (±24 st), **START** (przesunięcie początku), **LENGTH** (długość z wyciszeniem), **LOW CUT, HIGH CUT**.
- Crunch i kick zawsze idą za wysokością nuty. Punch tylko gdy włączysz **KEY**.

### SUB BASS
WAVE (sine / saw / square), OCTAVE, LEVEL, ATTACK, LENGTH, TONE (filtr dla saw/square), DRIVE. Strojony do tej samej nuty co kick.

### RUMBLE
Rumble robiony z samego kicka: DELAY (przesunięcie) → pogłos (DECAY = długość) → DRIVE → LOW PASS, z **DUCK** (chowa się pod uderzeniem kicka).

### MASTER
ROOT, PRE-GAIN, CLIP (ceiling + softness: 0% = twardy clip), LIMIT (ceiling + release), VOLUME. Panel **LAST HIT** pokazuje kształt ostatniego uderzenia.

## Presety
Fabryczne: Ancient Raw, Temple Euphoric, Pharaoh Uptempo, Babylon Hardcore, Colossus Frenchcore, Monolith Rawstyle, Punch Only, Sub & Rumble.
- **SAVE** – zapisuje preset do `Dokumenty\Ancient Drums\Presets\*.adp` (razem ze ścieżkami Twoich sampli).
- **RANDOMIZE** – losuje kicka w rozsądnych zakresach; warstwy z Twoim samplem go zachowują, ROOT i VOLUME zostają.
- **PLAY** – odgrywa kicka na nucie ROOT bez MIDI.
