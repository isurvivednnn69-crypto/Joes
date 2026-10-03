# Dragon Ball Tap Battle (Android, Namco Bandai) -> PS Vita: findings

## Status
DONE and verified here:
- `.pac` container fully decoded; `tools/pac.py` extracts all 291 archives (3460 entries).
  The C loader in `vita/src/pac.c` parses all 291 real files with 0 errors (host-compiled test).
- Extracted PNGs decode correctly (mostly 512x512 RGBA atlases); WAVs are PCM 16-bit mono 22050 Hz.
- `tools/dexlist.py` / `tools/dexdis.py`: small DEX class lister + Dalvik disassembler (no jadx available).

NOT done (be realistic): the game logic. It lives in `TCBManajer` (~221,000 Dalvik code units,
253 methods, state machines `Game1..GameNN`, battle/card logic) and must be rewritten in C.
The Vita app in `vita/` is only a texture viewer to prove the asset pipeline. UNTESTED on hardware.

## .pac format (little endian)
    u16 count
    count x { u32 offset; u32 size; char type[8] }   (16 bytes)
    data; offsets relative to 2 + 16*count
Types seen: png 1582, wav 1137, cnv 292, dac 292, bin 132, u 9 (URL text, ignore), spr 6, plt 5, act 4.
Game loader = `GameData.Init`: png->GL texture, wav->sound[], cnv->data[0], act->data[1],
bin->data[2], dac->data[3], spr->SpriteData (its own nested pac: png + bin).
`cnv`/`dac` are raw blobs read directly by `TCBManajer._SetAct` / `_ActReqMain` (sprite/animation
tables; `dac` has a u16 action count at byte offset 2 followed by offset tables). Still to decode.
Other files: `mk.bin` (392 bytes, looks XOR-obfuscated), `save.bin` (12906 bytes, default save data).
Text in `bin` entries is Shift-JIS (game is Japanese).

## Class map (from dex)
Keep/rewrite: TCBManajer (+AutoCardTask), TCB, ObjReq, GameData, Controller (input/touch UI),
Graphics2D + AndroidGL* (-> vitaGL), SoundEffect (-> SceAudio), StringTexture (text -> needs a font),
Utility, KeyData, GameTimer, GlobalWork, offscreen.
Drop: BluetoothManajer/BluetoothSearch (multiplayer), jp.co.bandainamcogames.Smap.* and smap*/Billing*/
Purchase*/Security (store + IAP), HttpControl, JsonConvert, NewsData, Downloader, AppInfoManager,
SignatureActivity, R.* (Android resources).

## Suggested order of work
1. Build `vita/` with VitaSDK + vitaGL, copy `assets/*.pac` to ux0:data/dbtap/assets/, confirm textures show.
2. Decode `cnv`/`dac` using `tools/dexdis.py classes.dex TCBManajer _SetAct` and `_ActReqMain`.
3. Port Graphics2D calls, then the main loop (`dragonballtap` activity / `Controller`), then game states.

## Getting a .vpk without installing VitaSDK
Push this folder to a new GitHub repo. The workflow in `.github/workflows/build.yml` builds it on GitHub;
download the `.vpk` from the run's "Artifacts". Install it with VitaShell. Homebrew using vitaGL also needs
`libshacccg.suprx` installed on the Vita (ur0:data/libshacccg.suprx) - the usual ShaccCg setup guides cover it.
