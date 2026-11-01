// ============================================================================
//  Jump 'n Bump fuer zwei (alte) PicoBoys - Spielmodul fuer den PicoBoy-Duo-
//  Rahmen (play.h). Mechanik aus der PBC+-Fassung, die sie wiederum 1:1 aus
//  main.c des Originals uebernommen hat (Brainchild Design 1998, GPL2+).
//
//  Unterschiede zur PBC+-Fassung: 128x64 schwarzweiss statt 240x280 Farbe
//  (Level in halber Aufloesung aus der Kachelkarte gezeichnet), keine Musik,
//  keine Klangdateien - nur kurze Piepser.
// ============================================================================
#pragma once
#include <stdint.h>

#define JNB_MAX_PLAYERS 2
#define JNB_END_SCORE 100

#define NUM_OBJECTS 200
#define NUM_FLIES 20

#define OBJ_SPRING 0
#define OBJ_SPLASH 1
#define OBJ_SMOKE 2
#define OBJ_YEL_BUTFLY 3
#define OBJ_PINK_BUTFLY 4
#define OBJ_FUR 5
#define OBJ_FLESH 6
#define OBJ_FLESH_TRACE 7

#define OBJ_ANIM_SPRING 0
#define OBJ_ANIM_SPLASH 1
#define OBJ_ANIM_SMOKE 2
#define OBJ_ANIM_YEL_BUTFLY_RIGHT 3
#define OBJ_ANIM_YEL_BUTFLY_LEFT 4
#define OBJ_ANIM_PINK_BUTFLY_RIGHT 5
#define OBJ_ANIM_PINK_BUTFLY_LEFT 6
#define OBJ_ANIM_FLESH_TRACE 7

#define BAN_VOID 0
#define BAN_SOLID 1
#define BAN_WATER 2
#define BAN_ICE 3
#define BAN_SPRING 4

#define SFX_JUMP 0
#define SFX_DEATH 1
#define SFX_SPRING 2
#define SFX_SPLASH 3

typedef struct {
  int action_left, action_up, action_right;
  int enabled, dead_flag;
  int bumps;
  int x, y;         // 16.16-Festkomma wie im Original
  int x_add, y_add; // Geschwindigkeit, 16.16
  int direction, jump_ready, jump_abort, in_water;
  int anim, frame, frame_tick, image;
  uint8_t deaths;   // zaehlt jeden Tod (Gast erkennt daran neue Gedaerme)
  uint8_t sfxflags; // in diesem Tick ausgeloeste Ereignisse, s. SF_*
} player_t;

#define SF_JUMP 0x01
#define SF_SPRING 0x02
#define SF_SPLASH 0x04
#define SF_SMOKE 0x08

typedef struct {
  int used, type;
  int x, y;
  int x_add, y_add;
  int x_acc, y_acc;
  int anim;
  int frame, ticks;
  int image;
} object_t;

typedef struct {
  int16_t x, y;
} fly_t;

extern player_t player[JNB_MAX_PLAYERS];
extern object_t objects[NUM_OBJECTS];
extern fly_t flies[NUM_FLIES];
extern int flies_enabled;
extern const uint8_t jnb_ban_map[17][22];

// ---- game.cpp (Mechanik) ---------------------------------------------------
void gameInit(void);
void steer_players(void);
void collision_check(void);
void update_objects(void);
void update_flies(void);
void position_player(int n);
void add_object(int type, int x, int y, int x_add, int y_add, int anim, int frame);
void spawn_gore(int who, int x, int y);
void player_kill(int killer, int victim);
unsigned short rnd(unsigned short max);
int ban(int x, int y);

// ---- jnb.cpp (Darstellung, Ton) --------------------------------------------
void jnbSfx(int which);                  // kurzer Piepser statt Klangdatei
void stainsClear(void);                  // Hintergrund neu aufbauen
void addStain(int x, int y, int image);  // dauerhafter Blutfleck
