#pragma once
// 05-AC: banca de forja visual. Somente apresentacao/UID; regras ficam em Play.
#include "InventoryUI.h"
#include <algorithm>
#include <array>
#include <string>

namespace top::ui {
class UIRenderer;
struct HudDrawList;
class ForgeWorkbenchUI {
public:
    enum class Mode { Forge=0, Combine=1, Socket=2, Production=3, Repair=4, ExtractGem=5, TiChun=6, Upgrade=7 };
    struct Selection { int uid=0; int id=0; };
    struct Rect { float x=0,y=0,w=0,h=0; };
    static Rect bounds(int w, int h) {
        return {std::max(2.f,float(w)-320.f),std::clamp(58.f,2.f,std::max(2.f,float(h)-300.f)),314.f,292.f};
    }
    bool visible() const { return visible_; }
    void toggle() { if(visible_) close(); else open(7u); }
    void open(unsigned modeMask=7u) {
        allowedModes_ = modeMask & 255u;
        if (!allowedModes_) { close(); return; }
        visible_=true;
        mode_=firstAllowed();
        clear();
    }
    void close() { visible_=false; allowedModes_=7u;clear(); }
    unsigned allowedModes() const { return allowedModes_; }
    bool modeAllowed(int mode) const { return mode>=0 && mode<8 && (allowedModes_ & (1u<<mode))!=0; }
    Mode firstAllowed() const {
        for (int i=0;i<8;++i) if(modeAllowed(i)) return Mode(i);
        return Mode::Forge;
    }
    Mode mode() const { return mode_; }
    void setMode(Mode mode) { if (modeAllowed(int(mode)) && mode_!=mode) {mode_=mode;clear();} }
    void clear() { selections_={}; valid_=false; cost_=0; chance_=0; status_.clear(); }
    void clearSlot(int slot) {
        if(slot<0||slot>=3)return;
        if(mode_==Mode::Repair&&slot>0)return;
        if((mode_==Mode::ExtractGem||mode_==Mode::TiChun)&&slot>1)return;
        selections_[std::size_t(slot)]={};
    }
    void setPreview(bool valid, int cost, int chance, std::string status) {
        valid_=valid; cost_=cost; chance_=chance; status_=std::move(status);
    }
    bool validPreview() const { return valid_; }
    bool contains(int x, int y, int w, int h) const {
        if(!visible_)return false;
        const auto b=bounds(w,h);
        return x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h;
    }
    // -1 fora, 0 corpo, 1 fechar, 2..4 abas, 6 reparo, 7 extrair, 8 TiChun, 9 efetividade, 5 confirmar, 10..12 limpar.
    int hit(int x, int y, int w, int h) const {
        if(!contains(x,y,w,h))return -1;
        const auto b=bounds(w,h);
        const float dx=float(x)-b.x,dy=float(y)-b.y;
        if(dx>=288.f&&dy<25.f)return 1;
        if(mode_!=Mode::Production && dy>=36.f&&dy<65.f) {
            if(allowedModes_&240u) {
                constexpr int modes[]={0,1,2,4,5,6,7};
                for(int i=0;i<7;++i) {
                    const float left=8.f+i*42.f;
                    if(dx>=left&&dx<left+40.f) {
                        const int mode=modes[i];
                        return modeAllowed(mode)?(mode==4?6:mode==5?7:mode==6?8:mode==7?9:mode+2):0;
                    }
                }
                return 0;
            }
            if(dx>=8.f&&dx<104.f)return modeAllowed(0)?2:0;
            if(dx>=108.f&&dx<206.f)return modeAllowed(1)?3:0;
            if(dx>=210.f&&dx<306.f)return modeAllowed(2)?4:0;
        }
        for(int i=0;i<3;++i) {
            if((mode_==Mode::Repair&&i>0)||((mode_==Mode::ExtractGem||mode_==Mode::TiChun)&&i>1))continue;
            if(dx>=10.f+i*102.f&&dx<106.f+i*102.f &&dy>=81.f&&dy<142.f)return 10+i;
        }
        if(dx>=16.f&&dx<298.f&&dy>=247.f&&dy<284.f)return 5;
        return 0;
    }
    int dropSlot(int x,int y,int w,int h) const {
        const int code=hit(x,y,w,h);
        return code>=10&&code<13?code-10:-1;
    }
    bool select(int slot,const BagView& row) {
        if(!visible_ || slot<0||slot>=3||(mode_==Mode::Repair&&slot!=0)||
           ((mode_==Mode::ExtractGem||mode_==Mode::TiChun)&&slot>1)||row.id<=0||row.uid<=0)return false;
        for(int i=0;i<3;++i)if(i!=slot&&selections_[std::size_t(i)].uid==row.uid)return false;
        selections_[std::size_t(slot)]={row.uid,row.id};
        valid_=false; return true;
    }
    bool selectNext(const BagView& row) {
        if(mode_==Mode::Repair)return select(0,row);
        for(int i=0;i<3;++i)if(selections_[std::size_t(i)].uid==0)return select(i,row);
        return false;
    }
    int index(int slot,const InventoryUI& inv) const {
        if(slot<0||slot>=3)return -1;
        const Selection s=selections_[std::size_t(slot)];
        if(s.uid<=0)return -1;
        for(const BagView& item:inv.rows())
            if(item.uid==s.uid&&item.id==s.id&&item.index>=0)return item.index;
        return -1;
    }
    std::array<int,3> indices(const InventoryUI& inv) const {
        return {index(0,inv),index(1,inv),index(2,inv)};
    }
    const Selection& selected(int slot) const { return selections_.at(std::size_t(slot)); }
    void draw(UIRenderer& r,HudDrawList& d,int w,int h,const InventoryUI& inv) const;
private:
    bool visible_=false;
    unsigned allowedModes_=7u;
    Mode mode_=Mode::Forge;
    std::array<Selection,3> selections_{};
    bool valid_=false;
    int cost_=0,chance_=0;
    std::string status_;
};
} // namespace top::ui
