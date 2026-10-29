#pragma once
// Read-only evidence from CPU geometry emission, not proof of GPU visibility.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

namespace top::effect_binding_trace {
inline bool enabled() {
    const char* value=std::getenv("TOP_HISTORICAL_BINDING_TRACE");
    return value && value[0]=='1' && value[1]=='\0';
}
struct Row {
    int skill=0,visual=0,kind=0,dummy=-1,actor=-2,type=0;
    unsigned seed=0;
    std::size_t system=0,part=0,vertices=0;
    float age=0.f;
    bool parentBillboard=false,modelDirection=false,childBillboard=false;
    bool boundMatrix=false,boundCenter=false;
    const char* owner="unqualified";
    std::string particle,resource,model;
    const std::array<float,16>* matrix=nullptr;
};
inline void text(std::ostream& out,const std::string& value) {
    out<<'"';for(char c:value){if(c=='"')out<<'"';out<<c;}out<<'"';
}
inline void header(std::ostream& out) {
    out<<"skill_id,visual_id,event_kind,seed,age,owner,target_actor,dummy,system,part,parent_type,"
        "parent_billboard,model_direction,child_billboard,particle,resource,model,dummy_available,"
        "bound_matrix,bound_center,vertices";
    for(int i=0;i<16;++i)out<<",m"<<i;
    out<<'\n';
}
inline void write(std::ostream& out,const Row& row) {
    out<<std::setprecision(std::numeric_limits<float>::max_digits10)
       <<row.skill<<','<<row.visual<<','<<row.kind<<','<<row.seed<<','<<row.age<<','
       <<row.owner<<','<<row.actor<<','<<row.dummy<<','<<row.system<<','<<row.part<<','<<row.type<<','
       <<row.parentBillboard<<','<<row.modelDirection<<','<<row.childBillboard<<',';
    text(out,row.particle);out<<',';text(out,row.resource);out<<',';text(out,row.model);
    out<<','<<(row.matrix?1:0)<<','<<row.boundMatrix<<','<<row.boundCenter<<','<<row.vertices;
    for(int i=0;i<16;++i){out<<',';if(row.matrix)out<<(*row.matrix)[i];}
    out<<'\n';
}
inline void record(const Row& row) {
    if(!enabled())return;
    static bool first=true;
    if(first){std::ostringstream out;header(out);std::fprintf(stderr,"[fx-binding] %s",out.str().c_str());first=false;}
    std::ostringstream out;write(out,row);std::fprintf(stderr,"[fx-binding] %s",out.str().c_str());
}
}
