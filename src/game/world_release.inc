// Included inside world.cpp. Guest access is confined to the game thread.
#ifdef FR_TEST_HARNESS
extern bool chaseRequested,walkRequested;
#endif
ReleaseBook soloReleases;
std::map<unsigned,ReleaseMap> releaseMaps;
std::map<std::string,MotionTimeline> releaseMotion;
struct PendingRelease {ReleasedMon mon;unsigned saved=0;};
std::vector<PendingRelease> pendingReleases;
PlayerState lastReleaseField;
bool releasesLoaded=false,releaseSaving=false,releaseRestored=false;
std::string releaseBattleId,releaseBattleHost,releaseMessage;
bool releaseBattleEntered=false,releaseBattleSaving=false,releaseBattleCaught=false;
uint64_t lastReleaseTick=0,lastReleaseRetry=0,releaseBattleStart=0;
std::map<std::string,unsigned> heardRelease;
struct ReleaseTravel {ReleasePhase phase=ReleasePhase::Queued;std::vector<ReleasePoint> route;size_t index=0;unsigned blocked=0;};
std::map<std::string,ReleaseTravel> releaseTravel;
ReleasePoint releasePoint(const PlayerState& p){return {p.mapGroup,p.mapNumber,p.elevation,p.x,p.y};}
std::filesystem::path releasePendingFile(){return diagnosticPath.parent_path()/"released-pending.cfg";}
void storePendingReleases(){std::ostringstream out;out<<"PMPENDING1\n";for(const auto& p:pendingReleases)out<<p.saved<<' '<<encodeReleased(p.mon)<<'\n';fr::replaceText(releasePendingFile(),out.str());}
void storeReleaseBattle(){std::ostringstream out;out<<"PMRELEASEBATTLE1 "<<(releaseBattleId.empty()?"-":releaseBattleId)<<' '<<(releaseBattleHost.empty()?"-":releaseBattleHost)<<' '<<releaseBattleCaught<<'\n';fr::replaceText(diagnosticPath.parent_path()/"released-battle.cfg",out.str());}
void loadReleases(){if(releasesLoaded||!room)return;releasesLoaded=true;soloReleases.open(diagnosticPath.parent_path()/"released-solo.cfg");
    std::ifstream in(releasePendingFile());std::string line;if(std::getline(in,line)){if(line!="PMPENDING1")throw std::runtime_error("Invalid pending release history");while(std::getline(in,line)){if(line.size()<3||line[0]<'0'||line[0]>'1'||line[1]!=' '||pendingReleases.size()>=420)throw std::runtime_error("Invalid pending release record");pendingReleases.push_back({decodeReleased(line.substr(2)),unsigned(line[0]-'0')});}releaseRestored=true;}
    std::ifstream battle(diagnosticPath.parent_path()/"released-battle.cfg");if(battle>>line){battle>>releaseBattleId>>releaseBattleHost>>releaseBattleCaught;if(!battle||line!="PMRELEASEBATTLE1")throw std::runtime_error("Invalid released battle receipt");if(releaseBattleId=="-")releaseBattleId.clear();if(releaseBattleHost=="-")releaseBattleHost.clear();}
}
uint32_t releaseHeader(uint8_t group,uint8_t number){
    if(group>=43)return 0;const auto groups=gameAddress(0x083526a8),list=r32(groups+4*group),end=group==42?groups:r32(groups+4*(group+1));
    if(!validRom(list,4)||end<list||end-list>512||number>=(end-list)/4)return 0;const auto header=r32(list+4*number);return validRom(header,28)?header:0;
}
const ReleaseMap* releaseMap(uint8_t group,uint8_t number){
    const unsigned key=(unsigned(group)<<8)|number;if(auto i=releaseMaps.find(key);i!=releaseMaps.end())return &i->second;
    const auto header=releaseHeader(group,number);if(!header)return nullptr;const auto layout=r32(header),grid=r32(layout+12);if(!validRom(layout,24))return nullptr;
    ReleaseMap map;map.width=int(r32(layout));map.height=int(r32(layout+4));const auto type=r8(header+23);map.outdoors=type>=1&&type<=3;
    if(map.width<1||map.height<1||map.width>512||map.height>512||size_t(map.width)*map.height>65536||!validRom(grid,size_t(map.width)*map.height*2))return nullptr;
    map.cells.resize(size_t(map.width)*map.height);const bool live=local.active&&local.mapGroup==group&&local.mapNumber==number&&size_t(map.width+15)*(map.height+14)*2<=0x5000;
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x){const auto value=r16(live?0x02031dfc+2*((y+7)*(map.width+15)+x+7):grid+2*(y*map.width+x));const unsigned id=value&1023;const auto ts=r32(layout+(id<640?16:20));if(!validRom(ts,24))continue;const auto attributes=r32(ts+20)+4*(id<640?id:id-640);if(!validRom(attributes,4))continue;
        auto& c=map.cells[y*map.width+x];c.elevation=uint8_t(value>>12);c.behavior=uint16_t(r32(attributes)&511);c.grass=c.behavior==2||c.behavior==3;
        c.walk=!(value&0xc00)&&!(c.behavior>=0x10&&c.behavior<=0x1b)&&!(c.behavior>=0x40&&c.behavior<=0x47);}
    const auto events=r32(header+4);if(validRom(events,20)){
        const auto objects=r32(events+4);const auto count=r8(events);if(count<=64&&validRom(objects,count*24))for(unsigned i=0;i<count;++i){const auto o=objects+24*i;const auto flag=r16(o+20);if(flag&&storyRead(flag))continue;const int x=s16(o+4),y=s16(o+6);if(map.cell(x,y))map.cells[y*map.width+x].walk=false;}
        const auto warps=r32(events+8);const auto n=r8(events+1);if(n<=64&&validRom(warps,n*8))for(unsigned i=0;i<n;++i){const auto warp=warps+8*i;const int x=s16(warp),y=s16(warp+2);const auto dest=releaseHeader(r8(warp+7),r8(warp+6));if(!dest||!map.cell(x,y))continue;const auto destEvents=r32(dest+4);if(!validRom(destEvents,20)||r8(warp+5)>=r8(destEvents+1))continue;const auto target=r32(destEvents+8)+8*r8(warp+5);if(!validRom(target,8))continue;
            map.cells[y*map.width+x].walk=true;map.exits[{x,y}]={r8(warp+7),r8(warp+6),r8(target+4),s16(target),s16(target+2)};}
    }
    const auto links=r32(header+12);if(validRom(links,8)&&r32(links)<=8){const auto rows=r32(links+4);if(validRom(rows,r32(links)*12))for(unsigned i=0;i<r32(links);++i){const auto row=rows+i*12;const unsigned direction=r32(row);const int offset=int32_t(r32(row+4));const auto dest=releaseHeader(r8(row+8),r8(row+9));if(!dest||direction<1||direction>4)continue;const auto dl=r32(dest);if(!validRom(dl,24))continue;const int dw=int(r32(dl)),dh=int(r32(dl+4));if(dw<1||dh<1||dw>512||dh>512)continue;
            for(int k=0;k<(direction<=2?map.width:map.height);++k){const int tx=direction<=2?k-offset:direction==3?dw-1:0,ty=direction>=3?k-offset:direction==1?0:dh-1;if(tx<0||ty<0||tx>=dw||ty>=dh)continue;
                const int x=direction<=2?k:direction==3?-1:map.width,y=direction>=3?k:direction==1?map.height:-1;map.exits[{x,y}]={r8(row+8),r8(row+9),0,int16_t(tx),int16_t(ty)};}}
    }
    return &releaseMaps.emplace(key,std::move(map)).first->second;
}
ReleasedPokemon captureReleasePokemon(uint32_t mon){
    ReleasedPokemon p;if(!validRam(mon,80))return p;
    p.personality=r32(mon);p.trainerId=r32(mon+4);p.language=r8(mon+18);p.markings=r8(mon+27);for(unsigned i=0;i<10;++i)p.nickname[i]=r8(mon+8+i);for(unsigned i=0;i<7;++i)p.trainerName[i]=r8(mon+20+i);
    std::array<std::array<uint8_t,12>,4> data{};uint16_t checksum=0;
    for(unsigned g=0;g<4;++g){const auto ptr=callGuest(gameAddress(0x0803f940),mon,p.personality,g);if(ptr<mon+32||ptr>mon+68||(ptr-mon-32)%12)return {};
        for(unsigned j=0;j<3;++j){const auto v=r32(ptr+j*4)^p.personality^p.trainerId;checksum=uint16_t(checksum+(v&65535)+(v>>16));for(unsigned k=0;k<4;++k)data[g][j*4+k]=uint8_t(v>>(8*k));}}
    if(checksum!=r16(mon+28)||(r8(mon+19)&5))return {};
    auto u16=[&](unsigned g,unsigned o){return uint16_t(data[g][o]|data[g][o+1]<<8);};auto u32=[&](unsigned g,unsigned o){return uint32_t(u16(g,o))|(uint32_t(u16(g,o+2))<<16);};
    p.species=u16(0,0);p.item=u16(0,2);p.experience=u32(0,4);p.ppBonuses=data[0][8];p.friendship=data[0][9];for(unsigned i=0;i<4;++i){p.moves[i]=u16(1,2*i);p.pp[i]=data[1][8+i];}for(unsigned i=0;i<6;++i){p.ev[i]=data[2][i];p.condition[i]=data[2][i+6];}
    p.virus=data[3][0];p.metLocation=data[3][1];p.origins=u16(3,2);p.ivs=u32(3,4);p.ribbons=u32(3,8);p.level=uint8_t(callGuest(gameAddress(0x0803e830),mon));return p;
}
void installReleasedEnemy(const ReleasedPokemon& p){
    callGuest(addresses.createWild,p.species,p.level,0);const uint32_t mon=0x0202402c;const uint32_t key=p.personality^p.trainerId;
    bus()->write32(mon,p.personality);bus()->write32(mon+4,p.trainerId);for(unsigned i=0;i<10;++i)bus()->write8(mon+8+i,p.nickname[i]);bus()->write8(mon+18,p.language);bus()->write8(mon+19,2);for(unsigned i=0;i<7;++i)bus()->write8(mon+20+i,p.trainerName[i]);bus()->write8(mon+27,p.markings);bus()->write16(mon+30,0);
    std::array<std::array<uint8_t,12>,4> data{};auto w16=[&](unsigned g,unsigned o,uint16_t v){data[g][o]=uint8_t(v);data[g][o+1]=uint8_t(v>>8);};auto w32=[&](unsigned g,unsigned o,uint32_t v){w16(g,o,uint16_t(v));w16(g,o+2,uint16_t(v>>16));};
    w16(0,0,p.species);w16(0,2,p.item);w32(0,4,p.experience);data[0][8]=p.ppBonuses;data[0][9]=p.friendship;for(unsigned i=0;i<4;++i){w16(1,2*i,p.moves[i]);const unsigned base=p.moves[i]?r8(gameAddress(0x08250c04)+12*p.moves[i]+4):0;data[1][i+8]=uint8_t(base+base*((p.ppBonuses>>(2*i))&3)/5);}for(unsigned i=0;i<6;++i){data[2][i]=p.ev[i];data[2][i+6]=p.condition[i];}data[3][0]=p.virus;data[3][1]=p.metLocation;w16(3,2,p.origins);w32(3,4,p.ivs);w32(3,8,p.ribbons);
    uint16_t checksum=0;for(unsigned g=0;g<4;++g){const auto ptr=callGuest(gameAddress(0x0803f940),mon,p.personality,g);if(ptr<mon+32||ptr>mon+68)throw std::runtime_error("Invalid native Pokemon layout");for(unsigned j=0;j<3;++j){uint32_t v=0;for(unsigned k=0;k<4;++k)v|=uint32_t(data[g][j*4+k])<<(k*8);checksum=uint16_t(checksum+(v&65535)+(v>>16));bus()->write32(ptr+4*j,v^key);}}
    bus()->write16(mon+28,checksum);callGuest(gameAddress(0x0803e774),mon,mon); // Native BoxMonToMon heals and recalculates stats.
}
bool stillOwnReleased(const ReleasedPokemon& p,bool recaptured=false){
    auto matches=[&](uint32_t m){if(!validRam(m,80)||r32(m)!=p.personality||!(r8(m+19)&2))return false;return recaptured?sameReleasedIndividual(captureReleasePokemon(m),p):r32(m+4)==p.trainerId;};
    for(unsigned i=0;i<6;++i)if(matches(0x02024284+100*i))return true;const auto boxes=r32(0x03005010);if(validRam(boxes,4+14*30*80))for(unsigned i=0;i<420;++i)if(matches(boxes+4+80*i))return true;return false;
}
int releaseMonHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||replaying()||!room)return 0;loadReleases();const auto storage=r32(0x020397b0);if(!validRam(storage,0x2110)||!lastReleaseField.active)return 0;
    const unsigned slot=r8(0x02039821);uint32_t mon=0;
    if(r8(0x02039822))mon=storage+0x20a0;
    else if(r8(0x02039820)==1&&slot<6)mon=0x02024284+100*slot;
    else if(r8(0x02039820)==0&&slot<30)mon=callGuest(gameAddress(0x0808bd30),callGuest(gameAddress(0x0808b9f4)),slot);
    auto pokemon=captureReleasePokemon(mon);if(!validReleasedPokemon(pokemon))throw std::runtime_error("The released Pokemon could not be preserved; release was stopped.");
    ReleasedMon m;m.id=online::randomId();m.owner=room->id();m.pokemon=pokemon;m.origin=m.position=m.entrance=releasePoint(lastReleaseField);m.pixelX=int16_t(m.position.x*16);m.pixelY=int16_t(m.position.y*16);m.facing=lastReleaseField.facing;
    pendingReleases.push_back({m,0});storePendingReleases(); // Before the native confirmed deletion; rollback is detected on recovery.
    const auto result=callGuest(address,cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3]);
    releaseMessage="Your released Pokemon will leave through the door and wait outside.";
    std::printf("POKEMON_RELEASED id=%s species=%u level=%u pid=%u map=%u,%u\n",m.id.c_str(),pokemon.species,pokemon.level,pokemon.personality,m.origin.group,m.origin.map);
    return guestReturn(cpu,result);
}
std::vector<ReleasedMon> releasePopulation(){return roomState.connected?roomState.released:soloReleases.records();}
bool releasedOccupied(int x,int y){for(const auto& m:releasePopulation())if(m.phase!=ReleasePhase::Queued&&m.phase!=ReleasePhase::Caught&&m.position.group==local.mapGroup&&m.position.map==local.mapNumber&&m.position.x==x&&m.position.y==y)return true;return false;}
std::vector<PlayerState> releasePoses(){std::vector<PlayerState> out;for(const auto& m:releasePopulation()){
    if(m.phase==ReleasePhase::Queued||m.phase==ReleasePhase::Battle||m.phase==ReleasePhase::Caught)continue;
    auto i=releaseMotion.find(m.id);PlayerState p=i==releaseMotion.end()?PlayerState{}:i->second.sample(clockMs());
    // Durable, resting Pokemon remain visible even when their pose has not
    // changed for longer than the live-player timeline's disconnect timeout.
    if(!p.active){p.active=true;p.mapGroup=m.position.group;p.mapNumber=m.position.map;p.elevation=m.position.elevation;p.x=m.position.x;p.y=m.position.y;p.pixelX=m.pixelX;p.pixelY=m.pixelY;p.follower=m.pokemon.species;p.followerShiny=isShiny(m.pokemon.personality,m.pokemon.trainerId);p.followerFacing=m.facing;p.followerFrame=m.frame;}
    if(sameScene(local,p))out.push_back(p);
}return out;}
std::vector<ReleasePoint> releaseReservations(const std::vector<ReleasedMon>& population,const std::string& except){std::vector<ReleasePoint> occupied;
    for(const auto& m:population)if(m.id!=except&&m.phase!=ReleasePhase::Queued&&m.phase!=ReleasePhase::Caught){occupied.push_back(m.position);if(auto i=releaseTravel.find(m.id);i!=releaseTravel.end()&&!i->second.route.empty()){occupied.push_back(i->second.route.back());if(i->second.index<i->second.route.size())occupied.push_back(i->second.route[i->second.index]);}}
    if(local.active){occupied.push_back(releasePoint(local));for(const auto& w:wild)occupied.push_back({local.mapGroup,local.mapNumber,local.elevation,int16_t(w.x),int16_t(w.y)});
        for(unsigned i=0;i<16;++i){const auto o=0x02036e38+36*i;if((r32(o)&1)&&!(r32(o)&0x2000)&&i!=r8(0x0203707d))occupied.push_back({local.mapGroup,local.mapNumber,uint8_t(r8(o+11)&15),int16_t(s16(o+16)-7),int16_t(s16(o+18)-7)});}}
    if(roomState.connected){for(const auto& peer:roomState.peers)if(peer.player.active)occupied.push_back(releasePoint(peer.player));
        for(const auto& map:roomState.world.maps){for(const auto& w:map.wild)occupied.push_back({map.group,map.map,w.elevation,w.x,w.y});for(const auto& n:map.npcs)if(n.visible)occupied.push_back(releasePoint(n.pose));}
        for(const auto& camp:roomState.world.camps)if(camp.id){for(int y=0;y<3;++y)for(int x=0;x<4;++x)occupied.push_back({camp.group,camp.map,camp.elevation,int16_t(camp.x+x),int16_t(camp.y+y)});for(const auto& p:camp.party)occupied.push_back({camp.group,camp.map,camp.elevation,int16_t(p.x/16),int16_t(p.y/16)});}
    }return occupied;
}
bool advanceRelease(ReleasedMon& m,unsigned elapsed,const std::vector<ReleasedMon>& population){
    if(m.phase==ReleasePhase::Battle||m.phase==ReleasePhase::Caught)return false;
    const PlayerState* owner=m.owner==room->id()?&local:nullptr;if(roomState.connected)for(const auto& peer:roomState.peers)if(peer.id==m.owner){owner=peer.slot==roomSlot?&local:&peer.player;break;}
    if(owner&&owner->active&&(owner->mapGroup!=m.origin.group||owner->mapNumber!=m.origin.map)){const auto* area=releaseMap(owner->mapGroup,owner->mapNumber);if(area&&area->outdoors)m.ownerOutside=true;}
    if(m.phase==ReleasePhase::Queued){
        if(!owner||!owner->active)return false; // Wait for the native PC/menu to close.
        const auto* map=releaseMap(m.origin.group,m.origin.map);if(!map)return false;
        auto reserved=releaseReservations(population,m.id);constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};ReleasePoint spawn=m.origin;bool placed=false;
        for(unsigned d:{unsigned(m.facing),1u,4u,3u,2u,0u}){auto p=m.origin;p.x+=int16_t(dx[d]);p.y+=int16_t(dy[d]);const auto* cell=map->cell(p.x,p.y);if(!cell||!cell->walk||std::find(reserved.begin(),reserved.end(),p)!=reserved.end())continue;p.elevation=cell->elevation;spawn=p;placed=true;break;}
        if(!placed)return false;m.position=spawn;m.pixelX=int16_t(spawn.x*16);m.pixelY=int16_t(spawn.y*16);m.phase=ReleasePhase::Hello;m.tick=0;return true;
    }
    m.tick=std::min(3600000u,m.tick+elapsed);
    if(m.phase==ReleasePhase::Hidden){if(m.tick>600&&m.tick%300<elapsed)m.facing=uint8_t((m.facing%4)+1);return m.tick<=186||m.tick%60<elapsed;}
    if(m.phase==ReleasePhase::Waiting){if(m.ownerOutside){m.phase=ReleasePhase::Goodbye;m.tick=0;return true;}if(now%180<elapsed){m.facing=uint8_t(1+(m.pokemon.personality+m.tick/180)%4);return true;}return false;}
    if(m.phase==ReleasePhase::Hello||m.phase==ReleasePhase::Goodbye){if(m.tick<54)return true;m.phase=m.phase==ReleasePhase::Hello?ReleasePhase::Leaving:ReleasePhase::Running;m.tick=0;releaseTravel.erase(m.id);}
    auto& travel=releaseTravel[m.id];
    if(travel.phase!=m.phase||travel.route.empty()){
        if(travel.phase==m.phase&&travel.route.empty()&&m.tick>elapsed&&m.tick%180>=elapsed)return true;
        travel={};travel.phase=m.phase;auto reserved=releaseReservations(population,m.id);
        if(m.phase==ReleasePhase::Leaving){
            auto exit=releaseRoute(m.position,releaseMap,[](auto,const auto& map){return map.outdoors;});if(exit.empty()){releaseMessage="Released Pokemon is waiting for a reachable exit.";return true;}
            m.entrance=exit.back();auto waiting=releaseCrowdPlace(m.entrance,releaseMap,reserved,m.pokemon.personality);if(!waiting)return true;
            auto crowd=releaseRoute(m.entrance,releaseMap,[&](auto p,const auto&){return p==*waiting;},reserved,16000);if(crowd.empty())return true;
            travel.route=std::move(exit);travel.route.insert(travel.route.end(),crowd.begin()+1,crowd.end());
        }else {travel.route=releaseRoute(m.position,releaseMap,[](auto p,const auto& map){const auto* c=map.cell(p.x,p.y);return map.outdoors&&c&&c->grass;},reserved);if(travel.route.empty()){releaseMessage="Released Pokemon is waiting for a reachable path to tall grass.";return true;}}
        travel.index=travel.route.size()>1?1:travel.route.size();
    }
    if(travel.index>=travel.route.size()){m.phase=m.phase==ReleasePhase::Leaving?ReleasePhase::Waiting:ReleasePhase::Hidden;m.tick=0;m.frame=0;releaseTravel.erase(m.id);return true;}
    const auto target=travel.route[travel.index];
    auto occupied=releaseReservations(population,m.id);const bool blocked=std::find(occupied.begin(),occupied.end(),target)!=occupied.end();
    if(blocked){m.frame=0;travel.blocked+=elapsed;if(travel.blocked>120)travel.route.clear();return true;}travel.blocked=0;
    if(target.group!=m.position.group||target.map!=m.position.map){m.position=target;m.pixelX=int16_t(target.x*16);m.pixelY=int16_t(target.y*16);++travel.index;return true;}
    const int dx=target.x*16-m.pixelX,dy=target.y*16-m.pixelY;m.facing=movementFacing(dx,dy,m.facing);m.frame=uint8_t((m.tick/6)%4);
    auto approach=[&](int16_t value,int targetValue){return int16_t(value+std::clamp(targetValue-int(value),-int(elapsed),int(elapsed)));};m.pixelX=approach(m.pixelX,target.x*16);m.pixelY=approach(m.pixelY,target.y*16);
    if(m.pixelX==target.x*16&&m.pixelY==target.y*16){m.position=target;++travel.index;}
    return true;
}
void updateReleases(){
    if(!room||!bus())return;loadReleases();if(local.active&&!replaying())lastReleaseField=local;
    if(releaseRestored&&safeWallet()){releaseRestored=false;std::erase_if(pendingReleases,[](const auto& p){return !p.saved&&stillOwnReleased(p.mon.pokemon);});storePendingReleases();}
    if(releaseSaving){if(bus()->save().dirty())return;for(auto& p:pendingReleases)p.saved=1;releaseSaving=false;storePendingReleases();}
    if(!releaseRestored&&safeWallet()&&std::any_of(pendingReleases.begin(),pendingReleases.end(),[](const auto& p){return !p.saved;})){
        for(auto& p:pendingReleases)p.saved=1;storePendingReleases();}
    if(now>=lastReleaseRetry+60){lastReleaseRetry=now;for(auto i=pendingReleases.begin();i!=pendingReleases.end();){if(!i->saved){++i;continue;}const bool accepted=roomState.connected?room->offerReleased(i->mon):soloReleases.offer(i->mon);if(accepted){i=pendingReleases.erase(i);storePendingReleases();}else{releaseMessage="Released Pokemon is waiting for the room to accept it.";++i;}}}
    if(!releaseBattleId.empty()){
        const bool battle=(r32(0x030030f4)&~1u)==gameAddress(0x08011100);releaseBattleEntered|=battle;
        if(battle&&(r8(0x02023e8a)&0x7f)==7&&!releaseBattleCaught){releaseBattleCaught=true;storeReleaseBattle();}
        if(safeWallet()&&(!releaseBattleStart||releaseBattleEntered||now>releaseBattleStart+300)){
            const auto population=releasePopulation();const auto found=std::find_if(population.begin(),population.end(),[](const auto& m){return m.id==releaseBattleId;});
            const bool ownClaim=found!=population.end()&&found->phase==ReleasePhase::Battle&&found->claimant==room->id();
            if(ownClaim&&!releaseBattleEntered)releaseBattleCaught=stillOwnReleased(found->pokemon,true);
            // Resolve the live capture claim. The next manual world save
            // persists the population alongside the trainer and receipt.

            if(releaseBattleSaving&&bus()->save().dirty())return;
            if(ownClaim){
                if(roomState.connected)room->finishReleased(releaseBattleId,releaseBattleCaught);else soloReleases.finish(releaseBattleId,room->id(),releaseBattleCaught);
            }
            if(found!=population.end()&&(found->phase!=ReleasePhase::Battle||!roomState.connected)){releaseBattleId.clear();releaseBattleHost.clear();releaseBattleEntered=releaseBattleSaving=releaseBattleCaught=false;storeReleaseBattle();}
        }
    }
    if(now<lastReleaseTick+6)return;const unsigned elapsed=unsigned(std::min(uint64_t(6),now-lastReleaseTick));lastReleaseTick=now;
    auto population=releasePopulation();if(!roomState.connected||roomState.hosting){std::vector<ReleasedMon> changes;
        for(auto& m:population)if(advanceRelease(m,elapsed,population))changes.push_back(m);
        if(roomState.connected)room->advanceReleased(changes);else {for(const auto& m:changes){const auto* old=soloReleases.find(m.id);soloReleases.advance(m,old&&old->phase!=m.phase);}if(!changes.empty()&&now%120<6)soloReleases.flush();}
    }
    population=releasePopulation();if(roomState.connected&&roomState.hosting)population=room->status().released;
    for(const auto& m:population){PlayerState p;p.active=m.phase!=ReleasePhase::Queued&&m.phase!=ReleasePhase::Battle&&m.phase!=ReleasePhase::Caught;p.mapGroup=m.position.group;p.mapNumber=m.position.map;p.elevation=m.position.elevation;p.x=m.position.x;p.y=m.position.y;p.pixelX=m.pixelX;p.pixelY=m.pixelY;p.follower=m.pokemon.species;p.followerShiny=isShiny(m.pokemon.personality,m.pokemon.trainerId);p.followerFacing=m.facing;p.followerFrame=m.frame;p.sequence=m.revision;p.sampleTime=uint32_t(clockMs());p.identity=std::stoull(m.id.substr(0,16),nullptr,16);
        if(m.phase==ReleasePhase::Hello||m.phase==ReleasePhase::Goodbye){p.offsetY=int8_t(-std::max(0,8-std::abs(18-int(m.tick))*8/18));const unsigned phase=unsigned(m.phase);if(m.tick<30&&heardRelease[m.id]!=phase&&sameScene(local,p)){callGuest(gameAddress(0x08071df0),m.pokemon.species,0);heardRelease[m.id]=phase;}}
        releaseMotion[m.id].push(p,clockMs());
    }
#ifdef FR_TEST_HARNESS
    if(now%30<6){std::ofstream f(diagnosticPath.parent_path()/"released-check.txt");f<<"frame="<<now<<" pending="<<pendingReleases.size()<<" saving="<<releaseSaving<<" battle="<<releaseBattleId<<" native="<<std::hex<<(r32(0x030030f4)&~1u)<<std::dec<<'\n';for(const auto& m:population)f<<"released="<<m.id<<" species="<<m.pokemon.species<<" pid="<<m.pokemon.personality<<" phase="<<unsigned(m.phase)<<" map="<<unsigned(m.position.group)<<','<<unsigned(m.position.map)<<" tile="<<m.position.x<<','<<m.position.y<<" pixel="<<m.pixelX<<','<<m.pixelY<<" face="<<unsigned(m.facing)<<" anim="<<unsigned(m.frame)<<" tick="<<m.tick<<" outside="<<m.ownerOutside<<'\n';f<<releaseMessage<<'\n';const auto st=r32(0x020397b0);if(validRam(st,0x2200))f<<"storage_state="<<unsigned(r8(st))<<" cursor="<<unsigned(r8(0x02039820))<<','<<unsigned(r8(0x02039821))<<'\n';f<<"native_enemy_species="<<callGuest(gameAddress(0x0803fbe8),0x0202402c,11)<<" native_enemy_pid="<<r32(0x0202402c)<<'\n';}
#endif
}
bool releasedEncounter(const PlayerState& p){
    if(!releaseBattleId.empty())return false;const auto point=releasePoint(p);
    for(const auto& m:releasePopulation())if(m.phase==ReleasePhase::Hidden&&m.position==point&&m.tick>=180){
        if(!callGuest(addresses.repel,m.pokemon.level))return false;
        releaseBattleStart=now;releaseBattleId=m.id;releaseBattleHost=roomState.connected?roomState.peers.front().id:std::string{};releaseBattleEntered=releaseBattleSaving=releaseBattleCaught=false;storeReleaseBattle();
        if(!(roomState.connected?room->claimReleased(m.id):soloReleases.claim(m.id,room->id(),point))){releaseBattleId.clear();releaseBattleHost.clear();storeReleaseBattle();return false;}
#ifdef FR_TEST_HARNESS
        chaseRequested=walkRequested=false;bus()->io().set_keyinput(0x3ff);
#endif
        PlayerState encounterPose=p;encounterPose.pixelX=m.pixelX;encounterPose.pixelY=m.pixelY;encounterPose.followerFacing=m.facing;encounterPose.followerFrame=m.frame;installReleasedEnemy(m.pokemon);beginBattlePresence(m.pokemon.species,0,&encounterPose);callGuest(addresses.startWild);std::printf("RELEASED_ENCOUNTER id=%s species=%u pid=%u\n",m.id.c_str(),m.pokemon.species,m.pokemon.personality);return true;
    }return false;
}
