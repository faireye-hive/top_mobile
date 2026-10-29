// 05-AV: selecao, paginacao e progresso de objetivos no dominio mission.
// Sem dependencias da biblioteca UI, ActorSkin ou GL.
#include "top/Play.h"
#include "top/MissionJournal.h"
#include <algorithm>
#include <string>
#include <vector>
namespace top {
MissionJournalView Play::missionJournalView() const {
    MissionJournalView journal;
    std::vector<const HistoricalProgress*> missions;
    if(historical_.id>0)missions.push_back(&historical_);
    for(const auto& mission:additionalMissions_)if(mission.id>0)missions.push_back(&mission);
    const int total=static_cast<int>(missions.size())+3; // preservar as tres missoes locais.
    const int pages=std::max(1,(total+4)/5);
    journalPage_=std::max(0,std::min(journalPage_,pages-1));
    if(journalSelectedId_==0 || (journalSelectedId_>0&&!findHistoricalMission(journalSelectedId_)))
        journalSelectedId_=!missions.empty()?missions.front()->id:-1;
    const int first=journalPage_*5;
    for(int index=first;index<std::min(first+5,total);++index) {
        if(index<static_cast<int>(missions.size())) {
            const auto& m=*missions[static_cast<std::size_t>(index)];
            std::string row=m.complete?"[Feita] ":m.active&&m.turnInRequired&&historicalObjectivesReady(m)?"[Pronta] ":
                            m.active?"[Ativa] ":"[Historica] ";
            row+=(journalSelectedId_==m.id?"> ":"  ")+m.title;
            journal.lines.push_back(std::move(row));
        } else {
            const int localIndex=index-static_cast<int>(missions.size());
            const auto& quest=quests_[localIndex];
            std::string row=quest.claimed?"[Feita] ":"[Local] ";
            row+=(journalSelectedId_==-localIndex-1?"> ":"  ")+std::string(quest.title);
            journal.lines.push_back(std::move(row));
        }
    }
    std::vector<std::string> details;
    const HistoricalProgress* selected=findHistoricalMission(journalSelectedId_);
    if(selected) {
        details.push_back(selected->title.substr(0,34));
        // Quando existem gatilhos, o texto longo e repetitivo da missao nao
        // deve esconder os contadores individuais no painel estreito.
        if(selected->triggers.empty() && !selected->objective.empty())
            details.push_back(selected->objective.substr(0,36));
        if(selected->targetCount>0 && selected->triggers.empty())
            details.push_back("Progresso "+std::to_string(std::max(0,std::min(selected->have,selected->targetCount)))+
                              "/"+std::to_string(selected->targetCount));
        for(std::size_t i=0;i<selected->triggers.size();++i) {
            const auto& trigger=selected->triggers[i];
            const int progress=i<selected->triggerCounts.size()?selected->triggerCounts[i]:0;
            std::string name;
            if(trigger.kind==MissionEventKind::GetItem) {
                const auto found=items_.find(trigger.targetId);
                if(found!=items_.end())name=found->second.name;
            }
            details.push_back(missionEventProgress(trigger,progress,name));
        }
        for(const auto& item:selected->neededItems) {
            const auto it=items_.find(item.id);
            const std::string name=it!=items_.end()&&!it->second.name.empty()?it->second.name:"Item "+std::to_string(item.id);
            details.push_back("Entregar "+name.substr(0,16)+" "+
                              std::to_string(std::min(bagCount(item.id),item.count))+"/"+
                              std::to_string(item.count));
        }
        if(selected->active && selected->turnInRequired) {
            if(historicalObjectivesReady(*selected))details.push_back("[Pronta] Volte ao NPC:");
            else details.push_back("Entregar ao NPC:");
            details.push_back(selected->turnInNpc.substr(0,30));
        }
        journal.canAbandon=selected->active;
    } else if(journalSelectedId_<0 && journalSelectedId_>=-3) {
        const auto& quest=quests_[-journalSelectedId_-1];
        details.push_back(std::string(quest.title));
        details.push_back("Progresso "+std::to_string(std::min(quest.have,quest.need))+"/"+
                           std::to_string(quest.need));
    }
    // O painel QuestUI comporta ate 10 linhas na area Objetivos. Reservar
    // a ultima para pagina e outra para avisar quando ha conteudo oculto.
    constexpr std::size_t maxDetailsBeforeFooter=9;
    const std::size_t mandatoryFooter=selected && selected->active &&
                                       selected->turnInRequired ? 2U : 0U;
    limitMissionJournalDetails(details,maxDetailsBeforeFooter,mandatoryFooter);
    journal.lines.insert(journal.lines.end(),details.begin(),details.end());
    journal.lines.push_back("Pagina "+std::to_string(journalPage_+1)+"/"+std::to_string(pages));
    return journal;
}
} // namespace top
