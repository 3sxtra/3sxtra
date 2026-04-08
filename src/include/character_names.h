/**
 * @file character_names.h
 * @brief Shared character name table for display purposes.
 *
 * Indices match the My_char[] global (0-19 characters, plus Akuma variant).
 * Used by native_save.c (replay filenames) and rmlui_palmod_menu.cpp (UI).
 */
#ifndef CHARACTER_NAMES_H
#define CHARACTER_NAMES_H

#ifdef __cplusplus
extern "C" {
#endif

extern int chkNameAkuma(int plnum, int rnum);

#define CHARACTER_NAME_COUNT 21

static const char* const g_character_names[CHARACTER_NAME_COUNT] = {
    "Gill", "Alex", "Ryu",   "Yun",   "Dudley",  "Necro",  "Hugo", "Ibuki",  "Elena", "Oro",  "Yang",
    "Ken",  "Sean", "Urien", "Gouki", "Chun-Li", "Makoto", "Q",    "Twelve", "Remy",  "Akuma"
};

/** Get display name for a character, with Akuma variant handling. */
static inline const char* character_get_name(int my_char_id) {
    if (my_char_id < 0 || my_char_id >= 20)
        return "???";
    int idx = my_char_id + chkNameAkuma(my_char_id, 6);
    if (idx >= 0 && idx < CHARACTER_NAME_COUNT)
        return g_character_names[idx];
    return "???";
}

#ifdef __cplusplus
}
#endif

#endif /* CHARACTER_NAMES_H */
