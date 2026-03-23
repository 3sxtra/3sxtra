================================================================================
  3SXtra — Modded Voice & SFX
================================================================================

Replace character voices, sound effects, and the VS/Char Select announcer tracks 
with your own audio files.

SETUP
-----
1. Place your audio file(s) in the correct subfolders inside assets/voice_mod/
2. Open the game and press F3 to open the Mods menu
3. Enable "Enable Modded Voice Lines"
4. The game will play your files instead of the original sounds

Supported audio formats: .ogg, .wav, .flac, .opus, .mp3
(.ogg or .wav recommended)


FOLDER STRUCTURE & NAMING
-------------------------
The game looks for files in specific subfolders based on the sound bank.
Files must be named by their internal engine ID (e.g., 5.ogg):

  assets/voice_mod/SE/          -> General Sound Effects
  assets/voice_mod/PL00/        -> Character Voices (Gill/Boss)
  assets/voice_mod/PL01/        -> Character Voices (Alex)
  assets/voice_mod/PL02/        -> Character Voices (Ryu)
  assets/voice_mod/PL03/        -> Character Voices (Yun)
  assets/voice_mod/PL04/        -> Character Voices (Dudley)
  assets/voice_mod/PL05/        -> Character Voices (Necro)
  assets/voice_mod/PL06/        -> Character Voices (Hugo)
  assets/voice_mod/PL07/        -> Character Voices (Ibuki)
  assets/voice_mod/PL08/        -> Character Voices (Elena)
  assets/voice_mod/PL09/        -> Character Voices (Oro)
  assets/voice_mod/PL10/        -> Character Voices (Yang)
  assets/voice_mod/PL11/        -> Character Voices (Ken)
  assets/voice_mod/PL12/        -> Character Voices (Sean)
  assets/voice_mod/PL13/        -> Character Voices (Urien)
  assets/voice_mod/PL14/        -> Character Voices (Gouki / Akuma)
  assets/voice_mod/PL15/        -> Character Voices (Chun-Li)
  assets/voice_mod/PL16/        -> Character Voices (Makoto)
  assets/voice_mod/PL17/        -> Character Voices (Q)
  assets/voice_mod/PL18/        -> Character Voices (Twelve)
  assets/voice_mod/PL19/        -> Character Voices (Remy)

Example: To replace Ken's (PL11) medium shoryuken voice, you would place it at:
  assets/voice_mod/PL11/45.ogg


VS SCREEN & CHARACTER SELECT ANNOUNCER
--------------------------------------
The announcer tracks for VS and Character Select screens can also be replaced here.
Place them directly in the root of the voice_mod folder:

  assets/voice_mod/vs.ogg       -> VS screen announcer music
  assets/voice_mod/emsel.ogg    -> Character Select music

These play as ONE-SHOT (play once, no looping), matching the original arcade behavior.
They are NOT affected by the "Enable Modded BGM" system. 

Alternatively, if you want them to loop continuously, use the bgm_mod folder instead 
(see assets/bgm_mod/readme.txt).


LEGACY FALLBACK
---------------
The older flat-folder method using the master request number (e.g., assets/voice_mod/1245.ogg) 
is still supported but not recommended for new mods, as finding the specific bank ID 
is usually easier for modders.
