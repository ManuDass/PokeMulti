#include "online/campaign.hpp"
#include "online/session.hpp"
#include "platform/atomic_file.hpp"
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
namespace fr::online {
void CampaignBook::open(const std::filesystem::path& folder,bool fresh){
    path.clear();state={};saved.clear();state.id=randomId();
    if(folder.empty())return;
    const auto directory=folder/"campaigns",active=directory/"active.cfg";
    if(!fresh&&std::filesystem::exists(active)){
        std::ifstream in(active);in>>state.id;in>>std::ws;
        if(!in.eof()||!validIdentity(state.id,"Campaign"))throw std::runtime_error("Invalid active campaign record");
    }
    path=directory/(state.id+".cfg");
    if(std::filesystem::exists(path)){
        if(std::filesystem::file_size(path)>262144)throw std::runtime_error("Campaign record is too large");
        std::ifstream in(path);std::string magic,id;size_t count=0,journal=0;
        if(!(in>>magic>>id>>state.sequence>>count>>journal)||magic!="PMCAM1"||id!=state.id||count>2048||journal>64)throw std::runtime_error("Invalid campaign header");
        for(size_t i=0;i<count;++i){unsigned key=0,value=0;
            if(!(in>>key>>value)||key>65535||value>65535||(!game::validStory({uint16_t(key),uint16_t(value)},0)&&!game::validStory({uint16_t(key),uint16_t(value)},7))||!saved.emplace(uint16_t(key),uint16_t(value)).second)throw std::runtime_error("Invalid campaign world state");
        }
        std::set<uint16_t> seen;uint32_t last=0;
        for(size_t i=0;i<journal;++i){game::CampaignCompletion c;
            if(!(in>>c.objective>>c.sequence>>std::quoted(c.actor))||!game::campaignObjective(c.objective)||!c.sequence||c.sequence<=last||c.sequence>state.sequence||c.actor.empty()||c.actor.size()>32||!seen.insert(c.objective).second)throw std::runtime_error("Invalid campaign journal");
            last=c.sequence;state.completed.push_back(c);
        }
        in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected campaign data");
    }else if(!fresh&&std::filesystem::exists(active))throw std::runtime_error("The active campaign save is missing; choose New campaign to start separately");
    // Materialize a new record before publishing its active pointer. Older
    // campaigns are retained when the player explicitly starts another one.
    if(!std::filesystem::exists(path))flush(true);
    fr::replaceText(active,state.id+"\n");
}
void CampaignBook::flush(bool force)const{
    if(path.empty()||(manual&&!force))return;
    std::ostringstream out;out<<"PMCAM1 "<<state.id<<' '<<state.sequence<<' '<<saved.size()<<' '<<state.completed.size()<<'\n';
    for(const auto& [id,value]:saved)out<<id<<' '<<value<<'\n';
    for(const auto& c:state.completed)out<<c.objective<<' '<<c.sequence<<' '<<std::quoted(c.actor)<<'\n';
    fr::replaceText(path,out.str());
}
std::vector<game::StoryValue> CampaignBook::baseline(uint8_t policy)const{
    std::vector<game::StoryValue> values;
    for(auto id:game::storyKeys(policy)){const auto i=saved.find(id);values.push_back({id,i==saved.end()?uint16_t(0):i->second,0});}
    return values;
}
void CampaignBook::commit(const std::vector<game::StoryValue>& story,const std::string& actor){
    auto previousSaved=saved;auto previousState=state;bool changed=false;
    for(const auto& value:story){auto [it,inserted]=saved.emplace(value.id,value.value);if(inserted||it->second!=value.value){it->second=value.value;changed=true;}}
    for(const auto& r:game::campaignObjectives){
        const auto it=saved.find(r.proof);
        if(it==saved.end()||it->second<r.minimum||std::any_of(state.completed.begin(),state.completed.end(),[&](const auto& c){return c.objective==r.id;}))continue;
        state.completed.push_back({r.id,++state.sequence,actor.substr(0,32)});changed=true;
    }
    if(changed)try{flush();}catch(...){saved=std::move(previousSaved);state=std::move(previousState);throw;}
}
}
