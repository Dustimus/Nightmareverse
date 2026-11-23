#ifndef Z_NV_SONGS_H
#define Z_NV_SONGS_H
/*
 * Nightmareverse: songs of our own on Link's ocarina (z_nv_songs.c). Each is registered with its
 * notes and what it does; the shared part hears the notes played in free play (z_message_PAL.c)
 * and matches them, one go at a time.
 */
#include "global.h"

#define NV_SONG_MAX_NOTES 16 // (a borrowed slot's playback holds 19)

typedef struct NvSongDef {
    const char* name; // for the log ("NVSONG <name>")
    const u8* notes;  // OCARINA_BTN_* in order
    s32 count;
    // the OoT song whose correct-song sequence it rides (OCARINA_SONG_*: the warp light, the fade,
    // OoT's playback of the notes, which are ours while it runs), or -1: its effect is all it does
    s32 slot;
    s32 (*ready)(PlayState* play);   // can it be played here, now (NULL: always)
    void (*effect)(PlayState* play); // when matched (a slot's song: as its sequence starts)
    void (*clip)(void);              // a slot's song: its sound after the playback (NULL: none)
    void (*fanfare)(void);           // a slot's song: in place of the slot's fanfare (NULL: none)
} NvSongDef;

/** Registered from each side's NvSongs_Register* (z_nv_songs.c NvSongs_Init); matched in order. */
void NvSongs_Register(const NvSongDef* song);
/** At the piano and the like: notes a scene is listening for itself. True: the note is the scene's. */
void NvSongs_SetListener(s32 (*listener)(PlayState* play, u8 button));
/** The song riding OoT's `slot`, if it was just played: taken (its slot's notes put back). */
const NvSongDef* NvSongs_Take(s32 slot);
/** As if `song` had just been played (a dev boot's warp). */
void NvSongs_SetPending(const NvSongDef* song);
/** Whether the last notes played (in free play, one go) end with `song`'s; NULL-safe. */
s32 NvSongs_Heard(const NvSongDef* song);

// z_message_PAL.c's
s32 NvSong_Note(PlayState* play, u8 button);
s32 NvSong_Pending(void);
void NvSong_Playback(void);
void NvSong_PlayClip(void);
void NvSong_Fanfare(void);
void NvSong_Reset(void);

#endif
