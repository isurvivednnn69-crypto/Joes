# Spider-Man (Gameloft) loader for PS Vita, milestone 1

Loads `libSpiderMan.so` from the HD APK, relocates it, resolves its imports against Vita/newlib shims,
runs its constructors and calls `JNI_OnLoad` with a fake JavaVM. It does NOT start the game yet.
Its output is `ux0:data/spiderman/log.txt`, which lists unresolved imports and every Java class/method the
native code asks for, and that list drives milestone 2.

## Put these on the Vita (nothing from the game is bundled in the VPK)
Folder `ux0:data/spiderman/`:
- `libSpiderMan.so`  (from the APK: lib/armeabi-v7a/libSpiderMan.so, NOT the .txt copy)
- `actors.gla  citys.gla  gamedata.gla  automat.gla  effects.gla  effects_adreno.gla  effects_nexus_s.gla  effects_s3.gla`
- `audio.bin  level_01_lod_data.bin  level_01_low_lod_data.bin`
About 1.3 GB in total. `actors_1.gla` is a duplicate of `actors.gla`, leave it out.

## Vita prerequisites
- kubridge plugin enabled in taiHEN (`*KERNEL` section: `ur0:tai/kubridge.skprx`)
- libshacccg installed (needed from milestone 2 on)
- Run the VPK, wait about 5 seconds, then read `log.txt`.

## Build
Push to GitHub; `.github/workflows/build-spiderman.yml` builds the VPK and uploads it as an artifact.
