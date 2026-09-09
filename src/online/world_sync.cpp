#include "online/world_sync.hpp"
#include <algorithm>
#include <stdexcept>
namespace fr::online {
bool validWorldReport(const WorldReport& r){
    if(!game::validBattle(r.battle)||(r.battle.id&&r.camp.id))return false;
    if(r.npcs.size()>15||r.wild.size()>12||!game::validCamp(r.camp)||(r.camp.id&&(!r.field||r.camp.group!=r.group||r.camp.map!=r.map)))return false;
    std::set<unsigned> seen;
    for(const auto& n:r.npcs){const auto& p=n.pose;
        if(!n.localId||n.localId>=255||!seen.insert(n.localId).second||n.anim>=32||n.command>=64||p.mapGroup!=r.group||p.mapNumber!=r.map||p.graphics>=152||p.facing<1||p.facing>4||p.elevation>15||p.pixelX< -32||p.pixelY< -32||p.pixelX>8224||p.pixelY>8224||p.flip>3||p.spriteFrame>=64||p.x<0||p.y<0||p.x>511||p.y>511||n.oldX< -7||n.oldY< -7||n.oldX>518||n.oldY>518)return false;
    }
    seen.clear();for(const auto& w:r.wild)if(!w.id||!seen.insert(w.id).second||!w.species||w.species>411||!w.level||w.level>100||w.elevation>15||w.x<0||w.y<0||w.x>511||w.y>511||w.pixelX<0||w.pixelY<0||w.pixelX>8192||w.pixelY>8192||w.facing<1||w.facing>4||w.frame>3)return false;
    return true;
}
MapState& WorldAuthority::map(uint8_t group,uint8_t number){
    for(auto& m:state.maps)if(m.group==group&&m.map==number)return m;
    // There are fewer than 512 maps in the supported game. Bound even hostile rooms.
    if(state.maps.size()>=512)throw std::runtime_error("Shared world map limit exceeded");
    state.maps.push_back({group,number});return state.maps.back();
}
bool WorldAuthority::available(unsigned slot,uint8_t group,uint8_t number,uint64_t now,bool requireStarted)const{
    if(slot>=MaxRoomPlayers)return false;const auto& p=present[slot];return p.report.field&&(!requireStarted||p.report.started)&&p.report.group==group&&p.report.map==number&&now-p.time<2000;
}
void WorldAuthority::report(unsigned slot,const WorldReport& update,uint64_t now){
    if(slot>=MaxRoomPlayers||!validWorldReport(update))throw std::runtime_error("Invalid shared world update");
    auto& source=present[slot];
    if(source.time&&int32_t(update.sequence-source.report.sequence)<0)return;
    source={update,now};
    // A battle reports field=false on its original map; retain its lease.
    // Once its owner arrives on another field map, an abandoned old-map scene
    // must become available even if the explicit blackout release was missed.
    if(update.field)for(const auto& l:state.leases)if(l.owner==slot&&(l.key.group!=update.group||l.key.map!=update.map)){
        release(slot,l.token,true);break;
    }
    state.battles[slot]=update.battle;
    if(state.battles[slot].id){state.battles[slot].trainer.sequence=++revision;state.battles[slot].trainer.sampleTime=uint32_t(now);}
    for(unsigned i=0;i<MaxRoomPlayers;++i)if(state.battles[i].id&&now-present[i].time>=2000)state.battles[i]={};
    for(unsigned i=0;i<MaxRoomPlayers;++i)if(state.camps[i].id&&now-present[i].time>=2000)state.camps[i]={};
    auto& camp=state.camps[slot];const auto& incoming=update.camp;
    if(!incoming.id||!update.field)camp={};
    else if(incoming.id!=state.campDecisions[slot]){
        state.campDecisions[slot]=incoming.id;camp={};bool clear=true;
        for(const auto& other:state.camps)if(game::campsOverlap(incoming,other))clear=false;
        for(const auto& presence:present)if(presence.report.field&&presence.report.group==incoming.group&&presence.report.map==incoming.map)for(const auto& n:presence.report.npcs)if(n.visible&&(game::campContains(incoming,n.pose.x,n.pose.y)||game::campContains(incoming,n.oldX,n.oldY)))clear=false;
        if(clear)camp=incoming;
    }else if(camp.id==incoming.id){
        bool unchanged=camp.group==incoming.group&&camp.map==incoming.map&&camp.x==incoming.x&&camp.y==incoming.y&&camp.elevation==incoming.elevation&&camp.ground==incoming.ground&&camp.party.size()==incoming.party.size();
        for(size_t i=0;unchanged&&i<camp.party.size();++i)unchanged=camp.party[i].species==incoming.party[i].species;
        if(unchanged)camp=incoming;
    }
    if(camp.id){camp.sequence=++revision;camp.sampleTime=uint32_t(now);}
    if(!update.field)return;
    auto& m=map(update.group,update.map);
    // NPC/story authority begins after personal onboarding. Grass encounters
    // are available as soon as the player has a usable Pokemon.
    if(update.started)for(const auto& incoming:update.npcs){
        auto it=std::find_if(m.npcs.begin(),m.npcs.end(),[&](const NpcState& n){return n.localId==incoming.localId;});
        if(it==m.npcs.end()){if(m.npcs.size()>=64)continue;m.npcs.push_back(incoming);it=std::prev(m.npcs.end());it->owner=uint8_t(slot);it->generation=++revision;}
        auto& n=*it;unsigned selected=n.owner;
        bool leased=false;
        for(const auto& l:state.leases)if(l.key.group==m.group&&l.key.map==m.map&&(l.key.kind==EncounterKind::Story||l.key.kind==EncounterKind::Unique||(l.key.kind==EncounterKind::Npc&&l.key.id==n.localId))){selected=l.owner;leased=true;break;}
        if(!leased){
            bool loaded=available(selected,m.group,m.map,now);
            if(loaded)loaded=std::any_of(present[selected].report.npcs.begin(),present[selected].report.npcs.end(),[&](const NpcState& a){return a.localId==n.localId;});
            if(!loaded)selected=slot;
        }
        if(n.owner!=selected){n.owner=uint8_t(selected);n.generation=++revision;continue;}
        const bool waiting=std::any_of(state.retries.begin(),state.retries.end(),[&](const auto& retry){return retry.key.group==m.group&&retry.key.map==m.map&&std::find(retry.actors.begin(),retry.actors.end(),n.localId)!=retry.actors.end();});
        if(!leased&&waiting)continue; // keep the staged actor at its encounter spot
        if(selected==slot){const auto owner=n.owner;const auto gen=n.generation;n=incoming;n.owner=owner;n.generation=gen;n.pose.sequence=++revision;n.pose.sampleTime=uint32_t(now);}
    }
    if(!available(m.wildOwner,m.group,m.map,now,false)||!present[m.wildOwner].report.wildEnabled){m.wildOwner=update.wildEnabled?uint8_t(slot):255;}
    else if(m.wildOwner==slot){
        m.wild=update.wild;
        std::erase_if(m.wild,[&](const WildState& w){return std::find(m.consumed.begin(),m.consumed.end(),w.id)!=m.consumed.end();});
    }
    m.revision=++revision;m.sampleTime=uint32_t(now);
}
void WorldAuthority::leave(unsigned slot){if(slot>=MaxRoomPlayers)return;for(const auto& l:state.leases)if(l.owner==slot){release(slot,l.token,true);break;}present[slot]={};state.camps[slot]={};state.battles[slot]={};state.campDecisions[slot]=0;std::erase_if(state.leases,[&](const auto& l){return l.owner==slot;});for(auto& m:state.maps){if(m.wildOwner==slot)m.wildOwner=255;for(auto& n:m.npcs)if(n.owner==slot)n.owner=255;}}
bool WorldAuthority::claim(unsigned slot,const EncounterKey& key,uint32_t token,uint64_t now){
    const bool wild=key.kind==EncounterKind::Wild;
    if(slot>=MaxRoomPlayers||!token||!available(slot,key.group,key.map,now,!wild))return false;
    if(wild?!present[slot].report.wildEnabled:!state.storyReady)return false;
    if(key.kind!=EncounterKind::Story&&key.kind!=EncounterKind::Npc&&key.kind!=EncounterKind::Wild&&key.kind!=EncounterKind::Unique)return false;
    if((key.kind==EncounterKind::Npc&&(!key.id||key.id>=255))||(key.kind==EncounterKind::Story&&key.id))return false;
    if(key.kind==EncounterKind::Unique&&((rewardPolicy&game::ShareSpecialPokemon)||!game::uniqueClaim(uint16_t(key.id))||key.id>65535||key.condition!=key.id||key.expected!=0))return false;
    if(key.condition){auto condition=std::find_if(state.story.begin(),state.story.end(),[&](const auto& s){return s.id==key.condition;});if(condition==state.story.end()||condition->value!=key.expected)return false;}
    for(const auto& l:state.leases){
        if(l.owner==slot)return l.token==token&&l.key==key;
        if(key.kind==EncounterKind::Unique&&l.key.kind==EncounterKind::Unique&&key.id==l.key.id)return false;
        if(l.key.group!=key.group||l.key.map!=key.map)continue;
        if((l.key.kind==key.kind&&l.key.id==key.id)||(key.kind!=EncounterKind::Wild&&l.key.kind!=EncounterKind::Wild&&(key.kind==EncounterKind::Story||key.kind==EncounterKind::Unique||l.key.kind==EncounterKind::Story||l.key.kind==EncounterKind::Unique)))return false;
    }
    if(state.leases.size()>=MaxRoomPlayers)return false;
    auto& m=map(key.group,key.map);
    if(key.kind==EncounterKind::Npc&&std::none_of(m.npcs.begin(),m.npcs.end(),[&](const auto& n){return n.localId==key.id&&n.visible;}))return false;
    if(key.kind==EncounterKind::Wild){
        auto i=std::find_if(m.wild.begin(),m.wild.end(),[&](const auto& w){return w.id==key.id;});if(i==m.wild.end()||m.consumed.size()>=4096)return false;
        m.consumed.push_back(key.id);m.wild.erase(i);
    }
    state.leases.push_back({key,uint8_t(slot),token});attempts[slot]={token,{}};
    for(auto& n:m.npcs)if(key.kind==EncounterKind::Story||key.kind==EncounterKind::Unique||(key.kind==EncounterKind::Npc&&key.id==n.localId)){n.owner=uint8_t(slot);n.generation=++revision;}
    m.revision=++revision;return true;
}
void WorldAuthority::checkpoint(unsigned slot,uint32_t token,const std::vector<NpcState>& actors){
    if(slot>=MaxRoomPlayers||actors.empty()||actors.size()>15)return;
    const auto lease=std::find_if(state.leases.begin(),state.leases.end(),[&](const auto& l){return l.owner==slot&&l.token==token;});
    if(lease==state.leases.end()||(lease->key.kind!=EncounterKind::Story&&lease->key.kind!=EncounterKind::Npc))return;
    WorldReport check;check.group=lease->key.group;check.map=lease->key.map;check.npcs=actors;
    if(!validWorldReport(check))throw std::runtime_error("Invalid encounter checkpoint");
    if(lease->key.kind==EncounterKind::Npc&&std::any_of(actors.begin(),actors.end(),[&](const auto& n){return n.localId!=lease->key.id;}))return;
    auto& m=map(check.group,check.map);auto& attempt=attempts[slot];
    if(attempt.token!=token||!attempt.actors.empty())return; // one checkpoint per attempt
    for(const auto& incoming:actors){
        if(!incoming.visible)continue;
        auto it=std::find_if(m.npcs.begin(),m.npcs.end(),[&](const auto& n){return n.localId==incoming.localId;});
        if(it==m.npcs.end()){if(m.npcs.size()>=64)continue;m.npcs.push_back(incoming);it=std::prev(m.npcs.end());}
        const auto generation=it->generation;*it=incoming;it->owner=uint8_t(slot);it->generation=generation?generation:++revision;
        it->pose.sequence=++revision;it->pose.sampleTime=uint32_t(present[slot].time);
        attempt.actors.push_back(incoming.localId);
    }
    m.revision=++revision;
}
void WorldAuthority::release(unsigned slot,uint32_t token,bool retry){
    if(slot>=MaxRoomPlayers)return;
    const auto lease=std::find_if(state.leases.begin(),state.leases.end(),[&](const auto& l){return l.owner==slot&&l.token==token;});
    if(lease==state.leases.end())return; // stale/foreign releases cannot end another attempt
    const auto key=lease->key;auto& attempt=attempts[slot];
    const bool staged=attempt.token==token&&!attempt.actors.empty();
    auto record=std::find_if(state.retries.begin(),state.retries.end(),[&](const auto& r){return r.key==key;});
    bool stillOpen=true;if(key.condition)for(const auto& value:state.story)if(value.id==key.condition)stillOpen=value.value==key.expected;
    if(retry&&staged&&stillOpen){
        if(record==state.retries.end()&&state.retries.size()<128){state.retries.push_back({key,{}});record=std::prev(state.retries.end());}
        if(record!=state.retries.end())record->actors=attempt.actors;
    }else if((!retry&&staged)||!stillOpen){if(record!=state.retries.end())state.retries.erase(record);}
    attempt={};state.leases.erase(lease);
}
void WorldAuthority::story(unsigned slot,const std::vector<game::StoryValue>& values,bool initial,bool final){
    if(slot>=MaxRoomPlayers||values.size()>128||(initial&&(slot!=0||state.storyReady))||(!initial&&!state.storyReady))return;
    for(const auto& v:values)if(!game::validStory(v,rewardPolicy))throw std::runtime_error("Personal or invalid state in story transaction");
    for(auto v:values){auto i=std::find_if(state.story.begin(),state.story.end(),[&](const auto& s){return s.id==v.id;});
        if(i==state.story.end()){if(!initial)continue;v.revision=++revision;state.story.push_back(v);}
        else if(initial||i->revision==v.revision){
            if(!initial&&(game::personalStoryState(v.id)||v.id==0x4057)&&i->value>v.value){i->revision=++revision;continue;}
            if(!initial&&game::uniqueFlag(v.id)&&i->value&&!v.value){i->revision=++revision;continue;}
            if(!initial&&game::uniqueClaim(v.id)&&v.value&&!i->value&&std::none_of(state.leases.begin(),state.leases.end(),[&](const auto& l){return l.owner==slot&&l.key.kind==EncounterKind::Unique&&l.key.id==v.id;})){i->revision=++revision;continue;}
            v.revision=++revision;*i=v;
        }
    }
    for(const auto& rule:game::uniquePokemon){
        const auto claim=std::find_if(state.story.begin(),state.story.end(),[&](const auto& v){return v.id==rule.claimed&&v.value;});
        if(claim==state.story.end())continue;
        for(auto hide:{rule.hide,rule.otherHide})if(hide)for(auto& v:state.story)if(v.id==hide&&!v.value){v.value=1;v.revision=++revision;}
    }
    std::erase_if(state.retries,[&](const auto& retry){return retry.key.condition&&std::any_of(state.story.begin(),state.story.end(),[&](const auto& v){return v.id==retry.key.condition&&v.value!=retry.key.expected;});});
    if(initial&&final){if(state.story.size()!=game::storyKeys(rewardPolicy).size())throw std::runtime_error("Incomplete room story baseline");state.storyReady=true;}
}
}
