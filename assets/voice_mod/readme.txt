================================================================================
  3SXtra — Modded Voice Lines
================================================================================

Replace the VS screen and Character Select music with your own audio files.

SETUP
-----
1. Place your audio file(s) in this folder (assets/voice_mod/)
2. Open the game and press F3 to open the Mods menu
3. Enable "Enable Modded Voice Lines"
4. The game will use your files next time those tracks play

   Voice mod has its own toggle, separate from "Enable Modded BGM".
   You need to enable voice mod specifically — BGM mod won't cover these.


SUPPORTED FILES
---------------
Name the file exactly as shown (case-sensitive, no extra characters):

  File          | What it replaces               | When it plays
  --------------|--------------------------------|---------------------------
  vs.ogg        | VS screen announcer music      | Matchup screen before fight
  emsel.ogg     | Character Select music         | Character selection screen

Only these two names are recognized. Other files in this folder are ignored.

The extension does NOT have to be .ogg — any of these formats work:
  .ogg   — Ogg Vorbis (recommended, supports embedded loop tags)
  .flac  — FLAC
  .opus  — Opus
  .mp3   — MP3
  .wav   — WAV

The engine tries extensions in the order above, so if you have both
vs.ogg and vs.mp3, it will use vs.ogg.


PLAYBACK BEHAVIOR
-----------------
Voice mod files play as ONE-SHOT (play once, no looping). This is
different from bgm_mod which loops tracks. This matches the original
behavior — VS music is a short jingle, and character select music
plays once and transitions.

These tracks play on a dedicated voice channel, separate from the
regular BGM channel. They are NOT affected by the BGM mod system.


WHAT ARE THESE TRACKS?
----------------------
The original game stores the VS screen and Character Select music
pre-loaded in RAM (not streamed from disc). They come from compressed
PPX archives in the AFS file:

  Track    | BGM Code | Arranged source (fnum) | Arcade source (fnum)
  ---------|----------|------------------------|---------------------
  VS       |  51      | 1346 (51_VS.adx)       | 557 (o51_VS.adx)
  EmSel    |  53      | 1348 (53_P_Sel.adx)    | 559 (o53_P_Sel.adx)

  "Arranged" = Dreamcast/console soundtrack
  "Arcade"   = Original CPS3 arcade soundtrack

Voice mod replacements apply regardless of which soundtrack is selected.


ALTERNATIVE: USING BGM_MOD INSTEAD
-----------------------------------
You can also replace these tracks via the bgm_mod system by creating
files named by their fnum:

  Arranged VS:      assets/bgm_mod/1346.ogg
  Arranged EmSel:   assets/bgm_mod/1348.ogg
  Arcade VS:        assets/bgm_mod/557.ogg
  Arcade EmSel:     assets/bgm_mod/559.ogg

The difference:
  - voice_mod:  friendly names (vs.ogg/emsel.ogg), one-shot playback,
                applies to both soundtracks, has its own enable toggle
  - bgm_mod:    fnum-based names, loops the track, soundtrack-specific,
                uses the "Enable Modded BGM" toggle

For most users, voice_mod is the simpler option.
