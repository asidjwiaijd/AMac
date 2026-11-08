#if defined(BOARD_AMAC_P4) || defined(BIRTHDAY_HOST_TEST)
#include "birthday_gift.h"
#include "birthday_config.h"
#include "charcoal_font_data.h"
#include <algorithm>
#include <stddef.h>

namespace Birthday {
namespace {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
constexpr uint16_t ink = rgb(38,40,61), paper = rgb(255,248,232);
constexpr uint16_t white = 0xffff, gray = rgb(118,116,134);
constexpr uint16_t lavender = rgb(207,200,245), pink = rgb(235,111,139);
constexpr uint16_t yellow = rgb(255,210,103), teal = rgb(82,172,157);
constexpr uint16_t blue = rgb(100,139,211), night = rgb(24,31,63);
constexpr uint16_t rainbow[] = {pink, yellow, teal, blue, lavender};

bool overlaps(Rect a, Rect b) {
    return a.x < b.x + b.w && a.y < b.y + b.h && b.x < a.x + a.w && b.y < a.y + a.h;
}

class Painter {
public:
    Painter(int x, int y, int w, int h, uint16_t *pixels) : tile{x,y,w,h}, pixels(pixels) {}
    Rect tile;
    uint16_t *pixels;
    void fill(Rect r, uint16_t c) {
        const int left = std::max(r.x, tile.x), top = std::max(r.y, tile.y);
        const int right = std::min(r.x + r.w, tile.x + tile.w);
        const int bottom = std::min(r.y + r.h, tile.y + tile.h);
        for (int y = top; y < bottom; ++y)
            for (int x = left; x < right; ++x) pixels[(y - tile.y) * tile.w + x - tile.x] = c;
    }
    void box(Rect r, uint16_t c) {
        fill({r.x,r.y,r.w,1},c); fill({r.x,r.y+r.h-1,r.w,1},c);
        fill({r.x,r.y,1,r.h},c); fill({r.x+r.w-1,r.y,1,r.h},c);
    }
    void dot(int x, int y, uint16_t c, int scale = 1) { fill({x,y,scale,scale},c); }
    int width(const char *s, int scale = 1) {
        int w = 0;
        for (; s && *s; ++s) w += glyph(*s).advance * scale;
        return w;
    }
    void text(int x, int baseline, const char *s, uint16_t c, int scale = 1) {
        for (; s && *s; ++s) {
            const auto &g = glyph(*s);
            const int top = baseline - (g.bbx_y + g.h) * scale;
            if (overlaps(tile, {x+g.bbx_x*scale,top,int(g.w)*scale,int(g.h)*scale})) {
                const int stride = (g.w + 7) / 8;
                for (int y = 0; y < g.h; ++y) for (int gx = 0; gx < g.w; ++gx)
                    if (CHARCOAL_GLYPH_BLOB[g.offset+y*stride+(gx>>3)] & (0x80>>(gx&7)))
                        dot(x+(g.bbx_x+gx)*scale,top+y*scale,c,scale);
            }
            x += g.advance * scale;
        }
    }
    void center(int baseline, const char *s, uint16_t c, int scale = 1) {
        text((kWidth-width(s,scale))/2,baseline,s,c,scale);
    }
    void star(int x, int y, int size, uint16_t c) {
        fill({x-size,y,size*2+1,1},c); fill({x,y-size,1,size*2+1},c);
        if (size > 2) fill({x-1,y-1,3,3},c);
    }
    void heart(int x, int y, int s, uint16_t c) {
        static const char *rows[] = {"0110110","1111111","1111111","0111110","0011100","0001000"};
        for (int yy=0; yy<6; ++yy) for (int xx=0; xx<7; ++xx)
            if (rows[yy][xx]=='1') dot(x+xx*s,y+yy*s,c,s);
    }
    void mac(int x, int y, int s) {
        fill({x,y,22*s,26*s},ink); fill({x+s,y+s,20*s,24*s},lavender);
        fill({x+3*s,y+3*s,16*s,14*s},ink); fill({x+4*s,y+4*s,14*s,12*s},paper);
        dot(x+7*s,y+8*s,ink,s); dot(x+14*s,y+8*s,ink,s);
        fill({x+8*s,y+12*s,6*s,s},ink);
        dot(x+7*s,y+11*s,ink,s); dot(x+14*s,y+11*s,ink,s);
        fill({x+13*s,y+20*s,6*s,s},ink); dot(x+4*s,y+20*s,pink,s);
        fill({x-2*s,y+26*s,26*s,3*s},ink);
    }
    void cursor(int x, int y) {
        for (int row=0; row<12; ++row) {
            fill({x,y+row,row/2+1,1},ink);
            if (row>1 && row<9) fill({x+1,y+row,std::max(1,row/2-1),1},white);
        }
        fill({x+3,y+9,2,5},ink);
    }
private:
    const CharcoalGlyph &glyph(unsigned char c) {
        if (c < CHARCOAL_FIRST_CP || c > CHARCOAL_LAST_CP) c = '?';
        return CHARCOAL_GLYPH_TABLE[c-CHARCOAL_FIRST_CP];
    }
};

void chrome(Painter &p, const Frame &f)
{
    p.fill({kWindow.x+4,kWindow.y+4,kWindow.w,kWindow.h},ink);
    p.fill(kWindow,paper); p.box(kWindow,ink);
    p.fill({25,29,430,26},lavender); p.fill({25,54,430,1},ink);
    p.text(39,46,"aMac  //  a little gift",ink);
    p.star(413,42,5,(f.elapsed/500)%2 ? paper : yellow);
    p.box(kClose,ink);
    for (int i=3;i<11;++i) { p.dot(kClose.x+i,kClose.y+i,ink); p.dot(kClose.x+13-i,kClose.y+i,ink); }
    p.fill({38,274,404,1},lavender);
    if (f.scene == Scene::Cake) {
        p.mac(kMac.x+4,kMac.y+2,1);
        p.text(85,296,"Model 1984",gray);
        p.text(85,313,"More than meets the eye.",gray);
    } else {
        p.fill(kBack,lavender); p.box(kBack,ink); p.center(307,"Back to cake",ink);
        p.text(42,310,"Esc: back",gray);
    }
    for (int i=0;i<5;++i) {
        const int x=373+i*14;
        p.box({x,298,8,8},gray);
        if (f.found & (1u<<i)) p.fill({x+2,300,4,4},rainbow[i]);
    }
    if (f.found == 31) p.text(347,322,"All five. For you.",gray);
}

void confetti(Painter &p, uint32_t elapsed)
{
    const int tick = int((elapsed/80)%252);
    for (int i=0;i<24;++i) {
        const int x=42+(i*71)%394, y=61+(i*37+tick)%208;
        p.fill({x,y,2+(i&1),3},rainbow[i%5]);
    }
}

void cake(Painter &p, const Frame &f)
{
    confetti(p,f.elapsed);
    p.center(87,"Happy Birthday!",ink,2);
    p.center(107,BirthdayConfig::kRecipient,gray);
    p.fill({169,215,142,7},lavender); p.box({169,215,142,7},ink);
    p.fill({178,173,124,42},pink); p.box({178,173,124,42},ink);
    p.fill({178,171,124,12},paper); p.box({178,171,124,12},ink);
    for (int i=0;i<7;++i) p.fill({181+i*18,181,9,5+(i%3)*2},paper);
    p.fill({191,146,98,27},yellow); p.box({191,146,98,27},ink);
    p.fill({191,145,98,9},paper); p.box({191,145,98,9},ink);
    for (int i=0;i<3;++i) {
        const int x=213+i*24;
        p.fill({x,126,5,19},rainbow[i*2]); p.box({x,126,5,19},ink);
        if (!f.wished) {
            const int drift=(int(f.elapsed/180)+i)%2;
            p.fill({x+1,116-drift,3,7},yellow); p.dot(x+2,114-drift,pink,2);
        } else p.star(x+2,118,2,lavender);
    }
    p.heart(229,191,3,paper);
    p.center(244,f.wished ? "Wish saved. Now go make it happen." : "One more trip around the sun.",ink);
    p.center(263,f.wished ? "There are still little secrets to discover." : "Click the cake or press Space. Make a wish.",gray);
}

void happyMac(Painter &p, const Frame &f)
{
    p.center(87,"Friendship OS",ink,2);
    p.mac(89,115,3);
    for (int i=0;i<4;++i) p.heart(67+i*31,109-int((f.elapsed/110+i*7)%22),1,rainbow[i]);
    p.text(203,136,"Processor: one curious heart",ink);
    p.text(203,160,"Memory: all the good times",ink);
    p.text(203,184,"Storage: room for many more",ink);
    p.text(203,208,"Status: glad we are friends",pink);
    p.center(251,"A tiny computer. A very big thank-you.",gray);
}

void stars(Painter &p, const Frame &f)
{
    p.fill({25,55,430,219},night);
    for (int i=0;i<55;++i) {
        const int x=39+(i*83)%402, y=66+(i*47)%191;
        p.star(x,y,(i+f.elapsed/500)%9==0 ? 2 : 0,(i+f.elapsed/400)%4==0 ? yellow : lavender);
    }
    // A small original constellation, joined by dotted trails.
    constexpr int pts[][2]={{156,148},{199,130},{237,151},{278,126},{316,143}};
    for (int i=0;i<4;++i) for (int j=0;j<20;j+=2)
        p.dot(pts[i][0]+(pts[i+1][0]-pts[i][0])*j/20,
              pts[i][1]+(pts[i+1][1]-pts[i][1])*j/20,blue);
    for (auto &point:pts) p.star(point[0],point[1],4,yellow);
    p.center(93,"A pocket universe",paper,2);
    p.center(213,"Among all these pixels,",paper);
    p.center(236,"I am glad our paths crossed.",paper);
    p.center(261,"Keep a little room for wonder.",lavender);
}

void arcade(Painter &p, const Frame &f)
{
    p.fill({25,55,430,219},night);
    static const int rays[][2]={{10,0},{7,7},{0,10},{-7,7},{-10,0},{-7,-7},{0,-10},{7,-7}};
    for (int i=0;i<6;++i) {
        const int radius=4+int((f.elapsed/45+i*13)%43);
        const int cx=72+(i*71)%342, cy=102+(i*47)%100;
        for (auto &ray:rays) {
            p.dot(cx+ray[0]*radius/10,cy+ray[1]*radius/10,rainbow[i%5],2);
            p.dot(cx+ray[0]*(radius-4)/10,cy+ray[1]*(radius-4)/10,lavender);
        }
    }
    p.fill({130,111,220,59},night);
    p.center(145,"LEVEL UP!",yellow,2);
    p.center(183,"Player 1: you.",paper);
    p.center(222,"A whole new year of adventures awaits.",paper);
    p.center(252,"Infinite continues. Keep playing.",lavender);
}

void letter(Painter &p)
{
    p.center(87,"A note from 1984",ink,2);
    p.center(110,"Delivered a little late. Meant just for you.",gray);
    for (size_t i=0;i<sizeof(BirthdayConfig::kLetter)/sizeof(*BirthdayConfig::kLetter);++i)
        p.text(65,140+int(i)*18,BirthdayConfig::kLetter[i],ink);
    p.center(261,BirthdayConfig::kSignature,pink);
}
} // namespace

bool covers(const Frame &f, int x, int y, int w, int h)
{
    if (w<=0 || h<=0) return false;
    const Rect tile{x,y,w,h};
    if (f.scene != Scene::Closed) return overlaps(tile,{24,28,436,308});
    return f.toast && overlaps(tile,kToast);
}

void composite(const Frame &f, int x, int y, int w, int h, uint16_t *pixels)
{
    if (!pixels || !covers(f,x,y,w,h)) return;
    Painter p{x,y,w,h,pixels};
    if (f.scene == Scene::Closed) {
        p.fill(kToast,paper); p.box(kToast,ink);
        p.heart(82,316,2,pink);
        p.text(108,320,"There is a little gift for you.",ink);
        p.text(108,339,"Ctrl + Alt + G",gray);
        return;
    }
    chrome(p,f);
    switch (f.scene) {
        case Scene::Cake: cake(p,f); break;
        case Scene::Mac: happyMac(p,f); break;
        case Scene::Stars: stars(p,f); break;
        case Scene::Arcade: arcade(p,f); break;
        case Scene::Letter: letter(p); break;
        default: break;
    }
    if (f.pointer) p.cursor(std::clamp(int(f.mouse_x),26,441),std::clamp(int(f.mouse_y),30,313));
}
} // namespace Birthday
#endif
