#include "ForgeWorkbenchUI.h"
#include "../UIRenderer.h"
#include "../PanelChrome.h"
#include "../HudDrawList.h"
namespace top::ui {
void ForgeWorkbenchUI::draw(UIRenderer& r,HudDrawList& d,int w,int h,const InventoryUI& inv) const {
        if(!visible_)return;
        const Rect b=bounds(w,h);
        chrome::panel(r,b.x,b.y,b.w,b.h);
        chrome::closeButton(r,b.x+288.f,b.y+4.f,18.f,17.f);
        d.addText(mode_==Mode::Production?"Substance Generator":
                  mode_==Mode::Repair?"Blacksmith - Repair":
                  mode_==Mode::ExtractGem?"Blacksmith - Gem Extraction":
                  mode_==Mode::TiChun?"Blacksmith - Equipment Upgrade":
                  mode_==Mode::Upgrade?"Blacksmith - Effectiveness":"Blacksmith - Oficina",
                  b.x+11.f,b.y+8.f,.06f,.15f,.20f);
        if(mode_!=Mode::Production){
            if(allowedModes_&240u) {
                constexpr int modes[]={0,1,2,4,5,6,7};
                constexpr const char* tabs[]={"Forja","Comb.","Encaixe","Reparo","Extrair","TiChun","Efeito"};
                for(int i=0;i<7;++i){
                    const int tabMode=modes[i];
                    const float x=b.x+8.f+i*42.f;
                    const bool enabled=modeAllowed(tabMode);
                    chrome::button(r,x,b.y+36.f,40.f,28.f,int(mode_)==tabMode);
                    d.addText(enabled?tabs[i]:"-",x+1.f,b.y+43.f,.07f,.17f,.20f);
                }
            } else {
                constexpr const char* tabs[]={"Forjar","Combinar","Encaixe"};
                for(int i=0;i<3;++i){
                    const float x=b.x+8.f+i*101.f;
                    chrome::button(r,x,b.y+36.f,96.f,28.f,int(mode_)==i);
                    d.addText(tabs[i],x+12.f,b.y+43.f,.07f,.17f,.20f);
                }
            }
        }else{
            d.addText("Producao offline - resultado aproximado",b.x+12.f,b.y+44.f,.2f,.28f,.28f);
        }
        constexpr const char* roles[8][3]={{"Equipamento","Gema","Refiner"},
            {"Pergaminho","Gema A","Gema B"},{"Equipamento","Stabilizer","Catalyst"},
            {"Fada","Blueprint","Ferramenta"},{"Equipamento","Ignorado","Ignorado"},
            {"Equipamento","Alicate","Ignorado"},
            {"Equipamento","Catalisador","Ignorado"},
            {"Scroll","Equipamento","Crystal"}};
        for(int i=0;i<3;++i){
            if((mode_==Mode::Repair&&i>0)||((mode_==Mode::ExtractGem||mode_==Mode::TiChun)&&i>1))continue;
            const float x=b.x+10.f+i*102.f;
            chrome::slot(r,x,b.y+81.f,96.f,61.f);
            d.addText(roles[int(mode_)][i],x+3.f,b.y+85.f,.07f,.16f,.20f);
            const int idx=index(i,inv);
            if(idx>=0){
                const auto it=std::find_if(inv.rows().begin(),inv.rows().end(),
                    [&](const BagView& row){return row.index==idx&&row.uid==selections_[std::size_t(i)].uid;});
                if(it!=inv.rows().end()){
                    if(!it->icon.empty())d.icons.push_back({it->icon,x+3.f,b.y+104.f,30.f,30.f});
                    d.addText(it->name.substr(0,11),x+35.f,b.y+111.f,.05f,.13f,.18f);
                    d.addText("x limpar",x+36.f,b.y+127.f,.35f,.18f,.18f);
                }
            }else if(selections_[std::size_t(i)].uid){
                d.addText("Item saiu",x+5.f,b.y+112.f,.6f,.18f,.14f);
            }else d.addText("Solte aqui",x+5.f,b.y+113.f,.32f,.39f,.40f);
        }
        if(mode_==Mode::Production) {
            d.addText("Slots: fada, blueprint e ferramenta opcional",b.x+10.f,b.y+152.f,.13f,.25f,.27f);
            std::size_t start=0;
            float y=b.y+173.f;
            int shown=0;
            while(start<status_.size()&&shown<3) {
                const std::size_t end=status_.find('\n',start);
                d.addText(status_.substr(start,std::min<std::size_t>(40,end==std::string::npos?status_.size()-start:end-start)),
                          b.x+10.f,y,valid_?.10f:.65f,valid_?.38f:.20f,.17f);
                if(end==std::string::npos)break;
                start=end+1;y+=16.f;++shown;
            }
        } else {
            d.addText(mode_==Mode::Repair?"Arraste um equipamento da bolsa para reparar.":
                      mode_==Mode::ExtractGem?"Equipamento + alicate da posicao do encaixe.":
                      mode_==Mode::TiChun?"Equipamento e catalisador historico.":
                      mode_==Mode::Upgrade?"Scroll tipo 62 + equipamento + crystal tipo 63.":
                      "Clique na bolsa em ordem ou arraste para os slots.",
                      b.x+10.f,b.y+152.f,.13f,.25f,.27f);
            d.addText("Ouro: "+std::to_string(inv.gold()),b.x+10.f,b.y+173.f,.16f,.22f,.22f);
            if(cost_>0)d.addText("Custo: "+std::to_string(cost_)+"  |  Chance: "+
                std::to_string(chance_/100)+"%",b.x+10.f,b.y+191.f,.17f,.23f,.22f);
            if(!status_.empty())d.addText(status_.substr(0,40),b.x+10.f,b.y+214.f,
                 valid_?.10f:.65f,valid_?.38f:.20f,.17f);
        }
        if(valid_ && mode_!=Mode::Socket && mode_!=Mode::Repair && mode_!=Mode::ExtractGem && mode_!=Mode::TiChun && mode_!=Mode::Upgrade)
            d.addText(mode_==Mode::Production?"Falha consome materiais e moedas de fada":"Falha consome ouro e materiais!",
                      b.x+10.f,b.y+230.f,.58f,.22f,.18f);
        chrome::button(r,b.x+16.f,b.y+247.f,282.f,36.f,valid_);
        d.addText(valid_?(mode_==Mode::Production?"Processar blueprint":
                          mode_==Mode::Repair?"Reparar equipamento":
                          mode_==Mode::ExtractGem?"Extrair gema":
                          mode_==Mode::TiChun?"Aprimorar equipamento":
                          mode_==Mode::Upgrade?"Aprimorar efetividade":"Revisar e confirmar"):
                  "Selecione itens validos",b.x+48.f,b.y+256.f,
                  .06f,.15f,.17f);
    }
}
