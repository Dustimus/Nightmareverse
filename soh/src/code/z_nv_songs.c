/*
 * Nightmareverse: songs of our own on Link's ocarina, shared by the RE1 side (the mansion's warp
 * song, the "Moonlight Sonata": z_nv_script.c) and the Majora's Mask side. Each song registers its
 * notes, its effect and, optionally, an OoT song slot it borrows: the warp song rides Nocturne of
 * Shadow's correct-song sequence (the warp light, the fade), its own notes standing in for
 * Nocturne's in OoT's playback while it runs, Nocturne's back after.
 *
 * The notes come from z_message_PAL.c as they're played. A song is matched only from notes played
 * in one go: the buffer empties as the ocarina comes out and when OoT recognises one of its own
 * songs (the Moonlight Sonata is the Song of Time shifted by a note, and leftovers from one Song of
 * Time would complete it on the next one's first note).
 */
#include "z_nv_songs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_SONGS_MAX 16

static const NvSongDef* sNvSongs[NV_SONGS_MAX];
static s32 sNvSongCount;
static s32 sNvSongsReady;
static s32 (*sNvSongListener)(PlayState* play, u8 button);
static u8 sNvSongHeard[NV_SONG_MAX_NOTES]; // the last notes played in free play, newest last (0xFF none)
static const NvSongDef* sNvSongPending;    // a slot's song just played (its sequence is running)

void NvSongs_RegisterRe1(void); // z_nv_script.c

static void NvSongs_Init(void) {
    if (sNvSongsReady) {
        return;
    }
    sNvSongsReady = true;
    memset(sNvSongHeard, 0xFF, sizeof(sNvSongHeard));
    NvSongs_RegisterRe1();
    // (each side's own register goes here, after RE1's: the Majora's Mask side's songs)
}

void NvSongs_Register(const NvSongDef* song) {
    if ((song == NULL) || (song->count <= 0) || (song->count > NV_SONG_MAX_NOTES) || (sNvSongCount >= NV_SONGS_MAX)) {
        printf("NVSONG not registered: %s\n", (song != NULL) ? song->name : "(none)");
        fflush(stdout);
        return;
    }
    sNvSongs[sNvSongCount++] = song;
}

void NvSongs_SetListener(s32 (*listener)(PlayState* play, u8 button)) {
    sNvSongListener = listener;
}

s32 NvSongs_Heard(const NvSongDef* song) {
    s32 k;

    if (song == NULL) {
        return false;
    }
    for (k = 0; k < song->count; k++) {
        if (sNvSongHeard[NV_SONG_MAX_NOTES - song->count + k] != song->notes[k]) {
            return false;
        }
    }
    return true;
}

/*
 * A borrowed slot's playback: OoT plays a correct song back from its note table
 * (sOcarinaSongNotes), so while the sequence runs the song's own notes stand in there (18 frames a
 * note, the last held, as Nocturne's quick run), and the slot's own go back once it's over.
 */
extern OcarinaNote sOcarinaSongNotes[OCARINA_SONG_MAX][20];
static OcarinaNote sNvSongSaved[20];
static s32 sNvSongSwapped = -1; // the slot whose notes are ours now, or -1

static void NvSong_Restore(void) {
    if (sNvSongSwapped >= 0) {
        memcpy(sOcarinaSongNotes[sNvSongSwapped], sNvSongSaved, sizeof(sNvSongSaved));
        sNvSongSwapped = -1;
    }
}

/** z_message_PAL.c: the buffer empties (the ocarina coming out, one of OoT's songs recognised). */
void NvSong_Reset(void) {
    memset(sNvSongHeard, 0xFF, sizeof(sNvSongHeard));
}

/**
 * A note played (z_message_PAL.c). In free play, the songs are matched in the order registered:
 * one with no slot does its effect there and then; one with a slot returns that slot, and the staff
 * goes on as OoT's song (z_message_PAL.c). -1 for nothing.
 */
s32 NvSong_Note(PlayState* play, u8 button) {
    s32 i;

    NvSongs_Init();
    sNvSongPending = NULL; // (a song played where OoT won't warp leaves nothing pending)
    NvSong_Restore();
    memmove(sNvSongHeard, sNvSongHeard + 1, sizeof(sNvSongHeard) - 1);
    sNvSongHeard[NV_SONG_MAX_NOTES - 1] = button;
    if (getenv("NV_LOG")) {
        printf("NVSONG note %d (action %d)\n", (int)button, (int)play->msgCtx.ocarinaAction);
        fflush(stdout);
    }
    if (play->msgCtx.ocarinaAction != OCARINA_ACTION_FREE_PLAY) {
        return -1;
    }
    if ((sNvSongListener != NULL) && sNvSongListener(play, button)) { // (a scene's own: the piano's)
        return -1;
    }
    for (i = 0; i < sNvSongCount; i++) {
        const NvSongDef* s = sNvSongs[i];

        if (!NvSongs_Heard(s) || ((s->ready != NULL) && !s->ready(play))) {
            continue;
        }
        NvSong_Reset();
        if (s->slot < 0) {
            if (s->effect != NULL) {
                s->effect(play);
            }
            continue; // (the buffer's empty: nothing after it matches)
        }
        sNvSongPending = s;
        if (s->effect != NULL) {
            s->effect(play);
        }
        printf("NVSONG %s\n", s->name);
        fflush(stdout);
        return s->slot;
    }
    return -1;
}

/** Whether OoT's correct-song sequence running is one of ours (z_message_PAL.c). */
s32 NvSong_Pending(void) {
    return sNvSongPending != NULL;
}

static u8 NvSong_Pitch(u8 button) {
    switch (button) {
        case OCARINA_BTN_A:
            return OCARINA_PITCH_D4;
        case OCARINA_BTN_C_DOWN:
            return OCARINA_PITCH_F4;
        case OCARINA_BTN_C_RIGHT:
            return OCARINA_PITCH_A4;
        case OCARINA_BTN_C_LEFT:
            return OCARINA_PITCH_B4;
        default:
            return OCARINA_PITCH_D5;
    }
}

/** OoT's playback of the song, ours in its slot's place (z_message_PAL.c, in place of its call). */
void NvSong_Playback(void) {
    const NvSongDef* s = sNvSongPending;
    OcarinaNote* n;
    s32 k;

    if (s == NULL) {
        return;
    }
    n = sOcarinaSongNotes[s->slot];
    if (sNvSongSwapped != s->slot) {
        NvSong_Restore();
        memcpy(sNvSongSaved, n, sizeof(sNvSongSaved));
        sNvSongSwapped = s->slot;
    }
    for (k = 0; k <= s->count; k++) {
        n[k].pitch = (k < s->count) ? NvSong_Pitch(s->notes[k]) : OCARINA_PITCH_NONE; // (then the end)
        n[k].length = (k == s->count - 1) ? 90 : ((k < s->count) ? 18 : 0);
        n[k].volume = 86;
        n[k].vibrato = 0;
        n[k].bend = 0;
        n[k].bFlat4Flag = 0;
    }
    AudioOcarina_SetPlaybackSong(s->slot + 1, 1);
}

/** Its sound after the playback (z_message_PAL.c). */
void NvSong_PlayClip(void) {
    if ((sNvSongPending != NULL) && (sNvSongPending->clip != NULL)) {
        sNvSongPending->clip();
    }
}

/** In place of the slot's fanfare (z_message_PAL.c). */
void NvSong_Fanfare(void) {
    if ((sNvSongPending != NULL) && (sNvSongPending->fanfare != NULL)) {
        sNvSongPending->fanfare();
    }
}

const NvSongDef* NvSongs_Take(s32 slot) {
    const NvSongDef* s = sNvSongPending;

    if ((s == NULL) || (s->slot != slot)) {
        return NULL;
    }
    sNvSongPending = NULL;
    NvSong_Restore(); // the playback's done: the slot's notes back
    return s;
}

void NvSongs_SetPending(const NvSongDef* song) {
    NvSongs_Init();
    sNvSongPending = song;
}
