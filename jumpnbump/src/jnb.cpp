// ============================================================================
//  Jump 'n Bump - Darstellung auf 128x64 schwarzweiss und die Netzanbindung.
//
//  Bild: das Level wird in HALBER Aufloesung aus der Kachelkarte gezeichnet
//  (176x128, Kacheln 8x8 statt 16x16). Das Display zeigt davon 128x64 und
//  scrollt auf den EIGENEN Hasen. Die Mechanik rechnet weiter in
//  Originalpixeln - nur das Zeichnen halbiert die Koordinaten.
//    fest    Punktraster mit durchgezogener Kante zu offenen Seiten
//    Eis     Schraegschraffur mit Kante
//    Wasser  bewegte Wellenlinie und wandernde Punkte
//  Hase 1 ist gefuellt, Hase 2 ein Umriss. Fell, Fleisch, Rauch, Fliegen und
//  Falter sind einzelne Pixel, Blutflecken bleiben dauerhaft im Hintergrund.
//
//  Netz (wie in der PBC+-Fassung): der Host rechnet die ganze Mechanik und
//  schickt je Paket Position, Bildnummer, Zustandsbits, Todes- und
//  Ereigniszaehler beider Hasen. Alles Schmueckende laeuft auf beiden Geraeten
//  lokal und wird nur durch die Zaehler angestossen. Zaehler statt Bits,
//  weil nicht jedes Bild einen Austausch bekommt - ein Bit koennte verfallen.
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jnb.h"
#include "play.h"

namespace {

constexpr int WW = 176, WH = 128;      // Welt in halber Aufloesung
constexpr int VW = 128, VH = 64;       // Displayausschnitt
constexpr int PKT = 4 + 2 * 9;

uint8_t world[WH][WW / 8];             // Hintergrund inkl. Blutflecken
int camx, camy;
bool camInit;
uint8_t matchNo, matchStatus;
uint8_t lastDeaths[2];
uint8_t fxCnt[2][4];                   // Host: Sprung, Feder, Wasser, Rauch (mod 16)
int me;                                // 0 = Host-Hase, 1 = Gast-Hase

// 8x8-Hasen mit Blick nach rechts (Bit 7 = linke Spalte), Bildnummern wie im
// Original je Richtung 0..8: stehen, laufen 1-3, springen, fallen 5-7, tot.
const uint8_t RABBIT[9][8] = {
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x3C, 0x24 },   // 0 stehen
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x3C, 0x42 },   // 1 laufen
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x3C, 0x24 },   // 2
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x3C, 0x18 },   // 3
    { 0x0A, 0x0A, 0x0B, 0x3F, 0x7F, 0xFE, 0xC0, 0x80 },   // 4 springen
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x5A, 0x81 },   // 5 fallen
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x5A, 0x42 },   // 6
    { 0x05, 0x05, 0x0B, 0x3F, 0x7F, 0x7E, 0x3C, 0x42 },   // 7
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x7E, 0xFF },   // 8 tot
};

// ---- Hintergrund ----------------------------------------------------------
inline void wset(int x, int y) {
    if ((unsigned)x < WW && (unsigned)y < WH) world[y][x >> 3] |= (uint8_t)(0x80 >> (x & 7));
}

int tileAt(int tx, int ty) {
    if (ty < 0) return BAN_VOID;
    if (tx < 0 || tx > 21 || ty > 16) return BAN_SOLID;
    return jnb_ban_map[ty][tx];
}
bool solidish(int t) { return t == BAN_SOLID || t == BAN_ICE; }

void buildWorld() {
    memset(world, 0, sizeof world);
    for (int ty = 0; ty < 16; ty++)
        for (int tx = 0; tx < 22; tx++) {
            int t = tileAt(tx, ty);
            if (!solidish(t)) continue;
            int x0 = tx * 8, y0 = ty * 8;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    bool tex = t == BAN_SOLID ? ((x + (y & 2)) % 4 == 0 && (y & 1)) : ((x + y) % 4 == 0);   // versetzte Punkte bzw. Schraffur
                    if (tex) wset(x0 + x, y0 + y);
                }
            // Kanten nur zu offenen Nachbarn: so entstehen klare Plattformen
            if (!solidish(tileAt(tx, ty - 1))) for (int x = 0; x < 8; x++) wset(x0 + x, y0);
            if (!solidish(tileAt(tx, ty + 1))) for (int x = 0; x < 8; x++) wset(x0 + x, y0 + 7);
            if (!solidish(tileAt(tx - 1, ty))) for (int y = 0; y < 8; y++) wset(x0, y0 + y);
            if (!solidish(tileAt(tx + 1, ty))) for (int y = 0; y < 8; y++) wset(x0 + 7, y0 + y);
        }
}

// ---- Zeichnen -------------------------------------------------------------
inline void wpix(int wx, int wy, int c = gfx::ON) { gfx::pixel(wx - camx, wy - camy, c); }

void drawRabbit(int i) {
    const player_t& p = player[i];
    if (p.enabled != 1) return;
    int img = p.image, frame = img % 9;
    bool left = img >= 9;
    const uint8_t* b = RABBIT[frame];
    int x0 = (p.x >> 17) - camx, y0 = (p.y >> 17) - camy;
    auto bit = [&](int x, int y) {
        if ((unsigned)x > 7 || (unsigned)y > 7) return false;
        int c = left ? 7 - x : x;
        return ((b[y] >> (7 - c)) & 1) != 0;
    };
    // Nur die Flaeche des Hasen freiraeumen (beim Umriss-Hasen soll kein Muster
    // durchscheinen). Kein Rand darum - der wuerde an Kanten die Landschaft
    // anfressen, sobald der Hase daneben steht oder von unten dagegen springt.
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (bit(x, y)) gfx::pixel(x0 + x, y0 + y, gfx::OFF);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if (!bit(x, y)) continue;
            // Hase 2: nur der Umriss
            if (i == 1 && bit(x - 1, y) && bit(x + 1, y) && bit(x, y - 1) && bit(x, y + 1)) continue;
            gfx::pixel(x0 + x, y0 + y, gfx::ON);
        }
}

void drawWater(uint32_t t) {
    int phase = (int)(t / 180);
    for (int ty = camy / 8; ty <= (camy + VH) / 8 && ty < 16; ty++)
        for (int tx = camx / 8; tx <= (camx + VW) / 8 && tx < 22; tx++) {
            if (tileAt(tx, ty) != BAN_WATER) continue;
            int x0 = tx * 8, y0 = ty * 8;
            bool surface = tileAt(tx, ty - 1) != BAN_WATER;
            for (int x = 0; x < 8; x++) {
                int wx = x0 + x;
                if (surface) wpix(wx, y0 + (((wx >> 1) + phase) & 1));
                for (int y = 3; y < 8; y += 3)
                    if ((wx + y * 3 + phase) % 7 == 0) wpix(wx, y0 + y);
            }
        }
}

void drawSpring(const object_t& o) {
    int x0 = (o.x >> 17) & ~7, y0 = (o.y >> 17) & ~7;
    int h = o.image >= 3 ? 5 : 2 + o.image;       // Bild 0 = zusammengedrueckt
    for (int x = 1; x < 7; x++) wpix(x0 + x, y0 + 7);
    for (int k = 1; k < h; k++) wpix(x0 + 2 + (k & 1) * 3, y0 + 7 - k);
    for (int x = 1; x < 7; x++) wpix(x0 + x, y0 + 7 - h);
}

void drawObjects() {
    for (int c1 = NUM_OBJECTS - 1; c1 >= 0; c1--) {
        const object_t& o = objects[c1];
        if (o.used != 1) continue;
        int wx = o.x >> 17, wy = o.y >> 17;
        switch (o.type) {
            case OBJ_SPRING: drawSpring(o); break;
            case OBJ_SPLASH: {                     // Fontaene: steigt und faellt
                int f = o.frame, up = f < 5 ? f : 9 - f;
                for (int k = -2; k <= 2; k++) wpix(wx + k * (f + 2) / 3, wy - up + (k < 0 ? -k : k));
                break;
            }
            case OBJ_SMOKE: if (o.frame < 3) wpix(wx, wy); break;
            case OBJ_YEL_BUTFLY:
            case OBJ_PINK_BUTFLY:                  // zwei Fluegel, im Takt schlagend
                if (o.frame & 1) { wpix(wx - 1, wy); wpix(wx + 1, wy); }
                else { wpix(wx - 1, wy - 1); wpix(wx + 1, wy - 1); wpix(wx, wy); }
                break;
            case OBJ_FUR:
            case OBJ_FLESH: wpix(wx, wy); break;
            case OBJ_FLESH_TRACE: if (o.frame < 2) wpix(wx, wy); break;
        }
    }
}

void drawHud() {
    char b[8];
    for (int i = 0; i < 2; i++) {
        snprintf(b, sizeof b, "%d", player[i].bumps);
        int w = gfx::textWidth(b) + 2;
        int x = i == 0 ? 0 : VW - w;
        bool mine = i == me;
        gfx::fillRect(x, 0, w, 9, mine ? gfx::ON : gfx::OFF);
        gfx::text(x + 1, 1, b, mine ? gfx::OFF : gfx::ON);
    }
}

void draw() {
    uint32_t t = hw::ms();
    // Kamera ruhig dem eigenen Hasen nachfuehren (hoechstens 3 Pixel je Bild)
    int tx = (player[me].x >> 17) + 4 - VW / 2, ty = (player[me].y >> 17) + 4 - VH / 2;
    tx = tx < 0 ? 0 : tx > WW - VW ? WW - VW : tx;
    ty = ty < 0 ? 0 : ty > WH - VH ? WH - VH : ty;
    if (!camInit) { camx = tx; camy = ty; camInit = true; }
    auto follow = [](int& c, int target) {
        int d = target - c;
        c += d > 3 ? 3 : d < -3 ? -3 : d;
    };
    follow(camx, tx);
    follow(camy, ty);

    gfx::clear();
    for (int y = 0; y < VH; y++) {
        const uint8_t* row = world[camy + y];
        for (int x = 0; x < VW; x++) {
            int wx = camx + x;
            if (row[wx >> 3] & (0x80 >> (wx & 7))) gfx::pixel(x, y, gfx::ON);
        }
    }
    drawWater(t);
    drawObjects();
    for (int i = 0; i < NUM_FLIES; i++) wpix(flies[i].x >> 1, flies[i].y >> 1);
    drawRabbit(me ^ 1);
    drawRabbit(me);                              // der eigene liegt oben
    drawHud();
    if (matchStatus) play::resultBox(matchStatus, player[0].bumps, player[1].bumps);
}

// ---- Ereignisse -------------------------------------------------------------
// Klang und Schmuckobjekte zu einem Ereignis. spawn nur beim Gast: beim Host
// hat steer_players() Rauch und Fontaene schon selbst erzeugt.
void playerFx(int i, int kind, bool spawn) {
    const player_t& p = player[i];
    switch (kind) {
        case 0: if (i == me) jnbSfx(SFX_JUMP); break;
        case 1:
            jnbSfx(SFX_SPRING);
            if (spawn) {
                int s1 = p.x >> 16, s2 = p.y >> 16;
                for (int c2 = 0; c2 < NUM_OBJECTS; c2++)
                    if (objects[c2].used == 1 && objects[c2].type == OBJ_SPRING &&
                        (objects[c2].y >> 20) == ((s2 + 15) >> 4) && abs((objects[c2].x >> 20) - ((s1 + 8) >> 4)) <= 1) {
                        objects[c2].frame = 0; objects[c2].ticks = 3; objects[c2].image = 0;
                        break;
                    }
            }
            break;
        case 2:
            jnbSfx(SFX_SPLASH);
            if (spawn) add_object(OBJ_SPLASH, (p.x >> 16) + 8, ((p.y >> 16) & 0xfff0) + 15, 0, 0, OBJ_ANIM_SPLASH, 0);
            break;
        case 3:
            if (spawn) add_object(OBJ_SMOKE, (p.x >> 16) + 2 + rnd(9), (p.y >> 16) + 13 + rnd(5), 0,
                                  -16384 - rnd(8192), OBJ_ANIM_SMOKE, 0);
            break;
    }
}

void newMatch() {
    gameInit();
    matchStatus = 0;
    memset(lastDeaths, 0, sizeof lastDeaths);
    memset(fxCnt, 0, sizeof fxCnt);
    camInit = false;
}

int pack(uint8_t* out) {
    uint8_t* p = out;
    *p++ = matchNo; *p++ = matchStatus; *p++ = (uint8_t)player[0].bumps; *p++ = (uint8_t)player[1].bumps;
    for (int i = 0; i < 2; i++) {
        play::put16(p, player[i].x >> 16);
        play::put16(p, player[i].y >> 16);
        *p++ = (uint8_t)player[i].image;
        *p++ = (uint8_t)((player[i].dead_flag ? 1 : 0) | (player[i].in_water ? 2 : 0));
        *p++ = player[i].deaths;
        *p++ = (uint8_t)((fxCnt[i][0] & 15) | (fxCnt[i][1] << 4));
        *p++ = (uint8_t)((fxCnt[i][2] & 15) | (fxCnt[i][3] << 4));
    }
    return (int)(p - out);
}

// Ein 60-Hz-Takt des Hosts (Originaltakt)
void hostTick(uint8_t h1, uint8_t h2) {
    for (int i = 0; i < 2; i++) player[i].sfxflags = 0;
    if (!matchStatus) {
        uint8_t h[2] = { h1, h2 };
        for (int i = 0; i < 2; i++) {
            player[i].action_left = (h[i] & hw::K_LEFT) ? 1 : 0;
            player[i].action_right = (h[i] & hw::K_RIGHT) ? 1 : 0;
            player[i].action_up = (h[i] & (hw::K_UP | hw::K_C)) ? 1 : 0;
        }
        steer_players();
        collision_check();
        if (player[0].bumps >= JNB_END_SCORE) matchStatus = 1;
        else if (player[1].bumps >= JNB_END_SCORE) matchStatus = 2;
    }
    update_objects();
    if (flies_enabled) update_flies();
    for (int i = 0; i < 2; i++)
        for (int k = 0; k < 4; k++)
            if (player[i].sfxflags & (1 << k)) { fxCnt[i][k] = (uint8_t)((fxCnt[i][k] + 1) & 15); playerFx(i, k, false); }
}

}  // namespace

// ---- Anbindungen fuer game.cpp -------------------------------------------------
void jnbSfx(int which) {
    switch (which) {
        case SFX_JUMP:   hw::beep(1100, 10); break;
        case SFX_SPRING: hw::beep(1600, 40); break;
        case SFX_SPLASH: hw::beep(260, 40); break;
        case SFX_DEATH:  hw::beep(140, 160); break;
    }
}

void stainsClear(void) { buildWorld(); }

void addStain(int x, int y, int) { wset(x >> 1, y >> 1); }

// ---- Spielschleifen (play-Rahmen) -------------------------------------------
namespace jnb {

void host() {
    me = 0;
    matchNo = 0;
    newMatch();
    play::Edges e1, e2;
    e1.sync(play::pad(1));
    e2.sync(play::pad(2));
    uint32_t last = hw::ms(), acc = 0, endAt = 0;
    while (play::running()) {
        uint32_t now = hw::ms(), dt = now - last;
        last = now;
        if (!play::live()) acc = 0;
        else {
            play::Pad a = play::pad(1), b = play::pad(2);
            uint8_t k = e1.take(a) | e2.take(b);
            if (matchStatus && now - endAt > 1000 && (k & hw::K_C)) {
                matchNo++;
                newMatch();
                hw::beep(900, 60);
            }
            acc += dt * 3;                    // 60 Hz = alle 50/3 ms
            if (acc > 50 * 4) acc = 50 * 4;
            while (acc >= 50) {
                acc -= 50;
                uint8_t before = matchStatus;
                hostTick(a.held, b.held);
                if (!before && matchStatus) { endAt = now; hw::beep(120, 350); }
            }
        }
        uint8_t buf[PKT];
        play::send(buf, pack(buf));
        draw();
        play::present();
    }
}

void guest() {
    me = 1;
    newMatch();
    int myMatch = -1;
    uint8_t lastFx[2][4] = {};
    uint32_t last = hw::ms(), acc = 0;
    uint8_t b[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        if (play::receive(b, &n) && n >= PKT) {
            const uint8_t* p = b;
            uint8_t m = *p++;
            bool fresh = m != myMatch;
            if (fresh) { myMatch = m; newMatch(); }
            uint8_t st = *p++;
            if (!matchStatus && st) hw::beep(120, 350);
            matchStatus = st;
            player[0].bumps = *p++;
            player[1].bumps = *p++;
            for (int i = 0; i < 2; i++) {
                player_t& pl = player[i];
                pl.enabled = 1;
                pl.x = play::get16(p) * 65536;
                pl.y = play::get16(p) * 65536;
                pl.image = *p++;
                uint8_t fl = *p++;
                pl.dead_flag = fl & 1;
                pl.in_water = (fl >> 1) & 1;
                uint8_t deaths = *p++;
                uint8_t f[4] = { (uint8_t)(p[0] & 15), (uint8_t)(p[0] >> 4), (uint8_t)(p[1] & 15), (uint8_t)(p[1] >> 4) };
                p += 2;
                if (fresh) { lastDeaths[i] = deaths; memcpy(lastFx[i], f, 4); continue; }
                if (deaths != lastDeaths[i]) { lastDeaths[i] = deaths; spawn_gore(i, pl.x, pl.y); }
                for (int k = 0; k < 4; k++) {
                    int d = (f[k] - lastFx[i][k]) & 15;
                    for (int j = 0; j < d && j < 2; j++) playerFx(i, k, true);
                    lastFx[i][k] = f[k];
                }
            }
        }
        // Schmueckendes laeuft lokal im Originaltakt
        uint32_t now = hw::ms();
        acc += (now - last) * 3;
        last = now;
        if (acc > 50 * 4) acc = 50 * 4;
        while (acc >= 50) {
            acc -= 50;
            update_objects();
            if (flies_enabled) update_flies();
        }
        draw();
        play::present();
    }
}

}  // namespace jnb
