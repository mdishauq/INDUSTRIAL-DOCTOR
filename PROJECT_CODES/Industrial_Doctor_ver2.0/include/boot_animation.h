// ============================================================
//  INDUSTRIAL DOCTOR v2.0  —  boot_animation.h
//
//  The iD monogram swoosh is a quadratic Bezier U-turn:
//    P0 = (63,  4)  → exits the top of D's spine
//    P1 = (30, 58)  → control pulls deep below the mark (rollercoaster valley)
//    P2 = (25, 12)  → rises back up, arriving at the i from below
//
//  This creates the "D does a rollercoaster U-turn → i" shape from the logo.
//
//  Place in: include/boot_animation.h
//  Call in setup(), after OLED init:
//    oled_boot_animation(display, oledReady);
// ============================================================
#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

namespace BootDraw {

// ── Primitive helpers ─────────────────────────────────────────────────────────

static inline void px(Adafruit_SSD1306& d, int x, int y, bool v = true) {
    if (x >= 0 && x < 128 && y >= 0 && y < 64)
        d.drawPixel(x, y, v ? SSD1306_WHITE : SSD1306_BLACK);
}

static void bline(Adafruit_SSD1306& d, int x0, int y0, int x1, int y1, bool v = true) {
    int dx = abs(x1-x0), dy = abs(y1-y0);
    int sx = x0<x1?1:-1, sy = y0<y1?1:-1, err = dx-dy;
    while (true) {
        px(d, x0, y0, v);
        if (x0==x1 && y0==y1) break;
        int e2=2*err;
        if (e2>-dy){err-=dy; x0+=sx;}
        if (e2< dx){err+=dx; y0+=sy;}
    }
}

// ── lowercase 'i' ─────────────────────────────────────────────────────────────
static void drawI(Adafruit_SSD1306& d, int jx=0, int jy=0) {
    d.fillRect(23+jx, 1+jy,  5, 4,  SSD1306_WHITE);  // dot
    d.fillRect(24+jx, 7+jy,  4, 26, SSD1306_WHITE);  // stem
}

// ── large 'D' ─────────────────────────────────────────────────────────────────
static void drawD(Adafruit_SSD1306& d, int jx=0, int jy=0) {
    d.fillRect(38+jx, 1+jy, 7, 32, SSD1306_WHITE);   // left spine

    // Elliptical bowl: outer semi-axes (22,15), inner counter (14,9),
    // inner D arc (17,11) — the double-bowl effect from the logo
    const int cx=45+jx, cy=17+jy;

    for (int dy=-15; dy<=15; dy++) {
        int xr=(int)round(22.0*sqrt(max(0.0,1.0-(double)(dy*dy)/225.0)));
        for (int dx=0; dx<=xr; dx++) px(d, cx+dx, cy+dy);
    }
    for (int dy=-9; dy<=9; dy++) {
        int xr=(int)round(14.0*sqrt(max(0.0,1.0-(double)(dy*dy)/81.0)));
        for (int dx=1; dx<=xr; dx++) px(d, cx+dx, cy+dy, false);
    }
    for (int dy=-11; dy<=11; dy++) {
        int xr=(int)round(17.0*sqrt(max(0.0,1.0-(double)(dy*dy)/121.0)));
        px(d, cx+xr, cy+dy);
        if (xr>0) px(d, cx+xr-1, cy+dy);
    }
}

// ── U-TURN SWOOSH — Quadratic Bezier ─────────────────────────────────────────
//
//  The swoosh path:
//    P0 = (63,  4)  — exits from top of D's spine area (top of mark)
//    P1 = (30, 58)  — control: pulls the curve deep below the mark.
//                     The curve BOTTOM (the U-turn valley) sits at t≈0.5
//                     which evaluates to approximately (37, 31) — the base
//                     of the mark area.  This is the "rollercoaster low point."
//    P2 = (25, 12)  — curve rises back up, arriving at the i from below.
//
//  Rendering:
//    Pass 1 — 50-step walk, each step draws a radius-2 filled disc and a
//             connecting Bresenham line, producing a 5px-thick white stroke.
//    Pass 2 — Walk again, compute the curve normal at every step, paint
//             black pixels at ±EDGE_DIST from centre.  This creates the
//             visible separation slot where the swoosh crosses the white
//             letter fills — approximating the bevelled gap in the gold logo.
// ─────────────────────────────────────────────────────────────────────────────
static void drawSwoosh(Adafruit_SSD1306& d, int jx=0, int jy=0) {
    const float P0x=63+jx, P0y= 4+jy;
    const float P1x=30+jx, P1y=58+jy;
    const float P2x=25+jx, P2y=12+jy;

    constexpr int   STEPS     = 50;
    constexpr float EDGE_DIST = 2.8f;

    // ── Pass 1: thick white stroke ────────────────────────────────────────────
    float prevX=P0x, prevY=P0y;
    for (int i=1; i<=STEPS; i++) {
        const float t = (float)i / (float)STEPS;
        const float u = 1.0f - t;
        const float x = u*u*P0x + 2*u*t*P1x + t*t*P2x;
        const float y = u*u*P0y + 2*u*t*P1y + t*t*P2y;
        const int   ix = (int)round(x), iy = (int)round(y);

        // 5×5 diamond disc
        for (int dy=-2; dy<=2; dy++)
            for (int dx=-2; dx<=2; dx++)
                if (dx*dx+dy*dy <= 4) px(d, ix+dx, iy+dy);

        bline(d, (int)round(prevX), (int)round(prevY), ix, iy);
        prevX=x; prevY=y;
    }

    // ── Pass 2: black edge cuts ───────────────────────────────────────────────
    for (int i=0; i<=STEPS; i++) {
        const float t  = (float)i / (float)STEPS;
        const float u  = 1.0f - t;
        const float x  = u*u*P0x + 2*u*t*P1x + t*t*P2x;
        const float y  = u*u*P0y + 2*u*t*P1y + t*t*P2y;

        // Tangent = derivative of quadratic Bezier
        const float tx = 2*(1-t)*(P1x-P0x) + 2*t*(P2x-P1x);
        const float ty = 2*(1-t)*(P1y-P0y) + 2*t*(P2y-P1y);
        const float ln = sqrtf(tx*tx + ty*ty);
        if (ln < 0.01f) continue;

        // Unit normal (perpendicular to tangent)
        const float nx = -ty/ln, ny = tx/ln;

        // Two black pixels on each edge for a clean 2px cut
        px(d, (int)round(x + EDGE_DIST    *nx), (int)round(y + EDGE_DIST    *ny), false);
        px(d, (int)round(x - EDGE_DIST    *nx), (int)round(y - EDGE_DIST    *ny), false);
        px(d, (int)round(x +(EDGE_DIST+1)*nx), (int)round(y +(EDGE_DIST+1)*ny), false);
        px(d, (int)round(x -(EDGE_DIST+1)*nx), (int)round(y -(EDGE_DIST+1)*ny), false);
    }
}

// ── Full mark ─────────────────────────────────────────────────────────────────
static void drawIDMark(Adafruit_SSD1306& d) {
    drawI(d); drawD(d); drawSwoosh(d);
}

// ── ECG 'O' — replaces O in DOCTOR ───────────────────────────────────────────
static void drawEcgO(Adafruit_SSD1306& d, int x, int y) {
    d.drawCircle(x+4, y+4, 4, SSD1306_WHITE);
    px(d, x+1, y+4);
    bline(d, x+1,y+4, x+2,y+2); bline(d, x+2,y+2, x+3,y+6);
    bline(d, x+3,y+6, x+5,y+3); bline(d, x+5,y+3, x+7,y+4);
    px(d, x+7, y+4);
}

// ── Brand text ────────────────────────────────────────────────────────────────
static void drawBrandText(Adafruit_SSD1306& d, bool showDiag) {
    d.drawFastHLine(18, 37, 92, SSD1306_WHITE);
    d.setTextColor(SSD1306_WHITE); d.setTextSize(1);
    d.setCursor(34, 39); d.print("INDUSTRIAL");
    d.setCursor(44, 49); d.print("D");
    drawEcgO(d, 51, 48);
    d.setCursor(62, 49); d.print("CTOR");
    if (showDiag) { d.setCursor(1, 58); d.print("DIAGNOSTICS & SOLUTIONS"); }
}

} // namespace BootDraw

// ─────────────────────────────────────────────────────────────────────────────
//  BOOT ANIMATION  (~4 s total)
// ─────────────────────────────────────────────────────────────────────────────
static void oled_boot_animation(Adafruit_SSD1306& d, bool ready) {
    if (!ready) return;

    // Phase 1: pixel static
    for (int f=0; f<10; f++) {
        d.clearDisplay();
        for (int i=0; i<f*28; i++) d.drawPixel(random(128),random(64),SSD1306_WHITE);
        d.display(); delay(22);
    }
    d.clearDisplay(); d.display(); delay(30);

    // Phase 2: scanline wipe top→bottom
    for (int y=0; y<64; y+=2) {
        d.drawFastHLine(0,y,128,SSD1306_WHITE);
        d.drawFastHLine(0,y+1,128,SSD1306_WHITE);
        int noise=max(0,40-y);
        for (int i=0; i<noise; i++) d.drawPixel(random(128),random(min(y+2,63),64),SSD1306_WHITE);
        d.display(); delay(8);
    }
    d.clearDisplay(); d.display(); delay(25);

    // Phase 3: glitch logo reveal
    for (int f=0; f<9; f++) {
        d.clearDisplay();
        if (f<5) for (int b=0; b<3-f/2; b++) d.drawFastHLine(0,random(4,60),128,SSD1306_WHITE);
        const int   mag=(f<3)?(5-f):(f<6)?2:0;
        const int8_t jx=(mag>0)?(int8_t)random(-mag,mag+1):0;
        const int8_t jy=(mag>0)?(int8_t)random(-2,3):0;
        if (f>0) BootDraw::drawI(d,jx,jy);
        if (f>1) BootDraw::drawD(d,jx,jy);
        if (f>2) BootDraw::drawSwoosh(d,jx,jy);
        d.display(); delay(55);
    }

    // Phase 4: lock blink
    for (int b=0; b<3; b++) {
        d.clearDisplay(); d.display(); delay(45);
        d.clearDisplay(); BootDraw::drawIDMark(d); d.display(); delay(80);
    }
    d.clearDisplay(); BootDraw::drawIDMark(d); d.display(); delay(100);

    // Phase 5: text types in
    d.drawFastHLine(18,37,92,SSD1306_WHITE); d.display(); delay(100);
    d.setTextColor(SSD1306_WHITE); d.setTextSize(1);
    d.setCursor(34,39); d.print("INDUSTRIAL"); d.display(); delay(180);
    d.setCursor(44,49); d.print("D");
    BootDraw::drawEcgO(d,51,48);
    d.setCursor(62,49); d.print("CTOR"); d.display(); delay(200);
    d.setCursor(1,58); d.print("DIAGNOSTICS & SOLUTIONS"); d.display(); delay(300);

    // Phase 6: EKG heartbeat (2 sweeps)
    static const int8_t EKG[32]={0,0,0,0,0,0,0,0,-2,-4,-3,0,-10,-16,8,6,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for (int sw=0; sw<2; sw++) {
        for (int off=0; off<64; off+=3) {
            d.clearDisplay(); BootDraw::drawIDMark(d); BootDraw::drawBrandText(d,true);
            for (int x=0; x<128; x++) {
                int y=constrain(60+EKG[((x/4)+off)&31],0,63);
                d.drawPixel(x,y,SSD1306_WHITE);
                if (y+1<64) d.drawPixel(x,y+1,SSD1306_WHITE);
            }
            d.display(); delay(16);
        }
    }

    // Phase 7: SYSTEM ONLINE flash
    for (int b=0; b<6; b++) {
        d.clearDisplay(); BootDraw::drawIDMark(d); BootDraw::drawBrandText(d,true);
        if (b%2==0) {
            d.fillRect(0,57,128,7,SSD1306_WHITE);
            d.setTextColor(SSD1306_BLACK); d.setTextSize(1);
            d.setCursor(4,58); d.print("[ SYSTEM  ONLINE ]");
            d.setTextColor(SSD1306_WHITE);
        }
        d.display(); delay(150);
    }

    // Phase 8: final hold
    d.clearDisplay(); BootDraw::drawIDMark(d); BootDraw::drawBrandText(d,true);
    d.fillRect(0,57,128,7,SSD1306_WHITE);
    d.setTextColor(SSD1306_BLACK); d.setTextSize(1);
    d.setCursor(4,58); d.print("[ SYSTEM  ONLINE ]");
    d.setTextColor(SSD1306_WHITE);
    d.display(); delay(800);
}