#include "../Engine/ui/QuestDialogInput.h"
#include "../Engine/ui/screens/NpcDialogUI.h"
#include "../Engine/ui/HudDrawList.h"
#include "../Engine/ui/UIRenderer.h"
#include <cassert>
#include <string>

int main() {
    using top::ui::QuestDialogHitState;
    using top::ui::QuestDialogInput;
    auto hit = [](const QuestDialogHitState& s, int x, int y) {
        return QuestDialogInput::hit(s, x, y, 1280, 720);
    };
    // Closed, outside or blank modal cannot affect the game world.
    QuestDialogHitState s;
    assert(hit(s, 500, 680) == -1);
    s.open = true;
    assert(hit(s, 0, 0) == -1);
    assert(hit(s, 650, 560) == 0);
    s.missionCount = 6;
    s.missionSelected = -1;
    assert(hit(s, 510, 404 + 110) == 10);
    assert(hit(s, 510, 404 + 110 + 29*3) == 13);
    assert(hit(s, 510, 404 + 110 + 29*4) == 40);
    assert(hit(s, 510, 404 + 274) == 30);
    assert(hit(s, 600, 404 + 274) == 31);
    assert(hit(s, 730, 404 + 274) == 0);
    s.missionPage = 1;
    assert(hit(s, 510, 404 + 110) == 10); // 5th mission
    assert(hit(s, 510, 404 + 110 + 29*2) == 40);
    s.missionSelected = 4;
    assert(hit(s, 510, 404 + 274) == 40); // not eligible
    s.offer = true;
    assert(hit(s, 510, 404 + 274) == 1);
    s.offer = false; s.ready = true;
    assert(hit(s, 510, 404 + 274) == 5);
    assert(hit(s, 720, 404 + 274) == 2);
    s.missionCount = 0; s.ready = false; s.missionSelected = -1;
    s.trade = true; s.forge = true;
    assert(hit(s, 510, 404 + 276) == 3);
    assert(hit(s, 615, 404 + 276) == 4);
    s.offer = true;
    assert(hit(s, 615, 404 + 276) == 1);
    assert(hit(s, 705, 404 + 276) == 2);

    // Painter uses same state; creates UI commands without a graphics device.
    top::ui::NpcDialogUI npc;
    npc.setWindow(true, "Senna", "Test", false, false, false);
    top::NpcQuestView quest;
    quest.id = 701; quest.title = "Blacksmith's Greetings";
    quest.status = top::NpcQuestView::Ready;
    npc.setMissions({quest}, 0, 0);
    top::ui::UIRenderer painter(1280,720);
    npc.draw(painter,1280,720);
    top::ui::HudDrawList draw;
    npc.appendText(draw,1280,720);
    bool completing = false;
    for (const auto& label : draw.labels) if (label.value == "Completar") completing = true;
    assert(completing && !painter.empty());
    const auto batch = painter.takeBatch();
    assert(batch.mode == 5 && !batch.verts.empty());

    // Jimberry's legacy dialogue uses tutorial mode, but must still expose
    // the separate Trade action and route its click to the shop.
    top::ui::NpcDialogUI jimberry;
    jimberry.setWindow(true, "Grocery - Jimberry", "Welcome", false, true, false);
    jimberry.setDialogOptions({"Services", "Nothing"}, true, false);
    top::ui::UIRenderer jimberryPainter(1280, 720);
    jimberry.draw(jimberryPainter, 1280, 720);
    top::ui::HudDrawList jimberryText;
    jimberry.appendText(jimberryText, 1280, 720);
    bool tradeVisible = false;
    for (const auto& label : jimberryText.labels)
        if (label.value == "Trade") tradeVisible = true;
    assert(tradeVisible);

    const int jimberryX = (1280 - 320) / 2 + 40;
    const int jimberryY = 720 - 316 + 275;
    assert(jimberry.tutorialFooterHit(jimberryX, jimberryY, 1280, 720) == -1);
    QuestDialogHitState jimberryState;
    jimberryState.open = true;
    jimberryState.trade = jimberry.hasTrade();
    assert(hit(jimberryState, jimberryX, jimberryY) == 3);
}
