#include "birthday_gift.h"
#include "birthday_config.h"
#include "charcoal_font_data.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace Birthday;
static void tap(Gift &gift, uint8_t code, uint32_t now) {
    gift.key(code,true,now); gift.key(code,false,now+1);
}
static void open(Gift &gift, uint32_t now) {
    assert(!gift.key(0x36,true,now)); assert(!gift.key(0x3a,true,now));
    assert(gift.key(0x05,true,now)); assert(gift.open());
    gift.key(0x05,false,now); gift.key(0x3a,false,now); gift.key(0x36,false,now);
}
static void click(Gift &gift, Rect r, uint32_t now) {
    assert(gift.move(r.x+r.w/2,r.y+r.h/2,false,now));
    assert(gift.button(0,true,now)); assert(gift.button(0,false,now+1));
}
static void snapshot(const char *name, const Frame &frame)
{
    constexpr int w = kWidth, h = kHeight;
    std::vector<uint16_t> full(w*h,0x7396), tiled(w*h,0x7396);
    composite(frame,0,0,w,h,full.data());
    // Odd tile sizes and partial edges expose clipping/stride mistakes.
    for (int y=0;y<h;y+=17) for (int x=0;x<w;x+=31) {
        const int tw=std::min(31,w-x), th=std::min(17,h-y);
        std::vector<uint16_t> tile(tw*th+2,0xbeef);
        std::fill(tile.begin()+1,tile.end()-1,0x7396);
        composite(frame,x,y,tw,th,tile.data()+1);
        assert(tile.front()==0xbeef && tile.back()==0xbeef);
        for (int yy=0;yy<th;++yy) for (int xx=0;xx<tw;++xx)
            tiled[(y+yy)*w+x+xx]=tile[1+yy*tw+xx];
    }
    if (tiled != full) {
        for (int i=0;i<w*h;++i) if (tiled[i]!=full[i]) {
            std::fprintf(stderr,"%s: clipped pixel (%d,%d) %04x != %04x\n",name,i%w,i/w,tiled[i],full[i]);
            break;
        }
        std::abort();
    }
    // pixels outside the advertised footprint must stay untouched.
    for (int y=0;y<h;++y) for (int x=0;x<w;++x)
        if (!covers(frame,x,y,1,1)) assert(full[y*w+x]==0x7396);
    if (const char *dir=std::getenv("PREBOOT_UI_OUT")) {
        std::string path=std::string(dir)+"/"+name+".ppm";
        FILE *f=std::fopen(path.c_str(),"wb"); assert(f);
        std::fprintf(f,"P6\n%d %d\n255\n",w,h);
        for (auto c:full) {
            const unsigned char rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
            std::fwrite(rgb,1,3,f);
        }
        std::fclose(f);
    }
}
int main()
{
    Gift gift;
    assert(!gift.key(0x05,true,0)); // unavailable until InputInit
    gift.reset(0xfffffff0);
    assert(!gift.frame(0xfffffff0+29999u).toast);
    assert(gift.frame(0xfffffff0+30000u).toast);
    snapshot("invitation",gift.frame(0xfffffff0+30000u));
    assert(!gift.frame(0xfffffff0+45000u).toast);
    open(gift,42); // launch across counter wrap
    assert(gift.frame(42).found==1 && !gift.frame(30000).toast);
    snapshot("01-cake",gift.frame(642));
    tap(gift,0x31,700); assert(gift.frame(701).wished);
    snapshot("01-wish",gift.frame(800));
    // Dragging off an icon and slow clicks do not accumulate the secret.
    click(gift,kMac,1000); click(gift,kMac,3000);
    gift.move(55,290,false,3010); gift.button(0,true,3010);
    gift.move(100,290,false,3020); gift.button(0,false,3020);
    for (int i=0;i<4;++i) click(gift,kMac,3100+i*100);
    assert(gift.frame(3500).scene==Scene::Cake);
    click(gift,kMac,3600); assert(gift.frame(3600).scene==Scene::Mac);
    snapshot("02-friendship-mac",gift.frame(4000));
    tap(gift,0x35,4100); assert(gift.frame(4100).scene==Scene::Cake);
    // Hold is canceled on drift; duplicate downs must not restart the timer.
    gift.move(413,42,false,4200); gift.button(0,true,4200);
    gift.move(390,60,false,4300); gift.tick(8000);
    assert(gift.frame(8000).scene==Scene::Cake); gift.button(0,false,8000);
    gift.move(413,42,false,8100); gift.button(0,true,8100);
    gift.button(0,true,8200); gift.tick(11099);
    assert(gift.frame(11099).scene==Scene::Cake); gift.tick(11100);
    assert(gift.frame(11100).scene==Scene::Stars); gift.button(0,false,11101);
    snapshot("03-pocket-universe",gift.frame(11500));
    click(gift,kBack,11600);
    const uint8_t combo[]={0x7e,0x7e,0x7d,0x7d,0x7b,0x7c,0x7b,0x7c,0x0b,0x00};
    uint32_t now=12000;
    // An expired partial combination cannot launch the arcade.
    tap(gift,combo[0],now); tap(gift,combo[1],now+5000);
    for (int i=2;i<10;++i) tap(gift,combo[i],now+5000+i*40);
    assert(gift.frame(now+6000).scene==Scene::Cake);
    now += 6000;
    for (auto code:combo) { tap(gift,code,now+=100); }
    assert(gift.frame(now).scene==Scene::Arcade);
    snapshot("04-arcade",gift.frame(now+1000));
    tap(gift,0x24,now+=1500);
    const uint8_t year[]={0x12,0x19,0x1c,0x15,0x24};
    for (auto code:year) tap(gift,code,now+=150);
    assert(gift.frame(now).scene==Scene::Letter && gift.frame(now).found==31);
    snapshot("05-time-capsule",gift.frame(now+1));
    // Wording must fit the real font and the available letter width.
    for (auto line:BirthdayConfig::kLetter) {
        int width=0;
        for (auto ch=line;*ch;++ch) { assert(*ch>=32 && *ch<=126); width+=CHARCOAL_GLYPH_TABLE[*ch-32].advance; }
        assert(width<=350);
    }
    tap(gift,0x35,now+=10); click(gift,kClose,now+=10);
    assert(!gift.open()); assert(!gift.frame(now+10000).toast);
    open(gift,now+=100); assert(gift.frame(now).found==31);
    gift.move(INT32_MAX,INT32_MIN,true,now);
    assert(gift.frame(now).mouse_x==479 && gift.frame(now).mouse_y==0);
    // Closing on mouse-down must still consume the matching up.
    gift.move(413,42,false,now); gift.button(0,true,now); gift.close();
    assert(gift.button(0,false,now+1));
    assert(!gift.key(0x80,true,now)); assert(!gift.button(3,true,now));
    puts("PASS: five secrets, sequence timeouts, mouse holds, wraparound, unlocks, clipped tiles and art previews");
}
