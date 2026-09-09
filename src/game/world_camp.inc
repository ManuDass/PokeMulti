// Game-thread camp lifecycle. No save data, map tiles or source art are modified.
CampSimulation localCamp;
std::array<Image,3> campFrames;
std::array<std::pair<CampState,std::vector<CampEdge>>,MaxRoomPlayers> campBorders;
std::array<std::array<MotionTimeline,6>,MaxRoomPlayers> campMotion;
bool campRequested=false,campPending=false,campWasOnline=false;
uint32_t campIdentity=0;uint64_t campBegan=0;
std::string lastCampNotice;
std::string campMessage="Find a clear lawn, then set up camp.";
bool campingSlot(unsigned slot){return slot<MaxRoomPlayers&&(slot==roomSlot?localCamp.state.id&&!campPending:roomState.connected&&roomState.world.camps[slot].id);}
bool campBlocked(int x,int y){
    if(local.active&&localCamp.state.id&&localCamp.state.group==local.mapGroup&&localCamp.state.map==local.mapNumber&&campContains(localCamp.state,x,y))return true;
    if(roomState.connected)for(unsigned i=0;i<4;++i){const auto& c=roomState.world.camps[i];if(i!=roomSlot&&c.group==local.mapGroup&&c.map==local.mapNumber&&campContains(c,x,y))return true;}
    return false;
}
bool campLawn(int x,int y){
    const auto layout=r32(0x02036dfc);if(!validRom(layout,24))return false;
    const auto width=r32(layout),height=r32(layout+4),grid=r32(layout+12);
    if(!width||!height||width>512||height>512||x<0||y<0||unsigned(x)>=width||unsigned(y)>=height||!validRom(grid,size_t(width)*height*2))return false;
    // The live grid includes cut trees and other script changes. Use its current
    // metatile and collision/elevation instead of the ROM's initial map layout.
    const unsigned liveWidth=width+15,liveHeight=height+14;
    if(size_t(liveWidth)*liveHeight*2>0x5000)return false;
    const uint16_t value=r16(0x02031dfc+2*((y+7)*liveWidth+x+7));
    const unsigned id=value&1023;const auto primary=r32(layout+16);
    const auto tileset=r32(layout+(id<640?16:20));if(!validRom(tileset,24))return false;
    const auto attr=r32(tileset+20)+4*(id<640?id:id-640);if(!validRom(attr,4))return false;
    return campGrass(r8(0x02036dfc+23),r8(0x02036dfc+21)!=0,primary==0x082d4a94u+(codeOffset()?0x70u:0u),value,uint16_t(r32(attr)&511),local.elevation);
}
bool campEventTile(int x,int y){
    const auto events=r32(0x02036e00);if(!validRom(events,20))return true;
    // Leave warps, coordinate scripts and hidden-item/sign interactions accessible.
    for(unsigned kind=1;kind<=3;++kind){const unsigned count=r8(events+kind),stride=kind==1?8:kind==2?16:12;const auto list=r32(events+4+kind*4);if(count>128||!validRom(list,count*stride))return count!=0;
        for(unsigned j=0;j<count;++j)if(s16(list+j*stride)==x&&s16(list+j*stride+2)==y)return true;}
    return false;
}
bool campClear(int x,int y){
    if(!campLawn(x,y)||worldOccupied(x,y)||campEventTile(x,y))return false;
    for(unsigned i=0;i<16;++i){const auto a=0x02036e38+i*36;const auto flags=r32(a);if((flags&1)&&!(flags&0x2000)&&s16(a+20)-7==x&&s16(a+22)-7==y)return false;}
    return true;
}
void endCamp(const char* message,bool notify=true){
    localCamp={};campPending=false;campMessage=message;lastCampNotice=campMessage;
    if(notify)queueFieldNotice("camp",campMessage);
    followerPath.clear();
}
void loadCampArt(){
    if(!campFrames[0].empty())return;
    std::array<Image,3> frames;
    for(unsigned i=0;i<3;++i){const auto p=localAsset("Camp/Tent_f"+std::to_string(i+1)+".png");if(p.empty())throw std::runtime_error("Camp tent frames are missing. Restore the Camp folder.");frames[i]=readImage(p);if(frames[i].width!=56||frames[i].height!=39)throw std::runtime_error("Camp frames must retain their original 56 x 39 size.");}
    campFrames=std::move(frames);
}
std::vector<uint16_t> campParty(){
    std::vector<uint16_t> party;const unsigned count=r8(0x02024029);if(count>6)return party;
    for(unsigned i=0;i<count;++i){const auto mon=0x02024284+i*100;const auto species=callGuest(0x0803fbe8+codeOffset(),mon,11);const auto egg=callGuest(0x0803fbe8+codeOffset(),mon,45);
        if(species&&species<=411&&!egg)party.push_back(uint16_t(species));}
    return party;
}
void updateCamp(){
    if(localCamp.state.id){
        if(!local.active||local.mapGroup!=localCamp.state.group||local.mapNumber!=localCamp.state.map||replaying()||nativeLinkScene()||campWasOnline!=roomState.connected)
            endCamp("Camp packed away as your adventure resumed.",false);
        // Check the trainer's moving ground position after native movement.
        // Collision previews and walking into an obstacle cannot pack a camp.
        // Ignore sprite jump/bob offsets: this is the position on the ground.
        else if(!campWalkable(localCamp.state,(int(local.pixelX)+8)/16,(int(local.pixelY)+8)/16))
            endCamp("Camp packed away as you left the campsite.",false);
    }
    if(campRequested){campRequested=false;lastCampNotice.clear();
        if(localCamp.state.id)endCamp("Camp packed away. Let's go!");
        else if(!safeStory()||r8(0x0203707b)==1||(avatarFlags&0x1e))campMessage="Finish moving or the current scene before camping on foot.";
        else if(!campLawn(local.x,local.y))campMessage="You can't camp here! Find a clear patch of short grass outside.";
        else try{
            loadCampArt();const auto party=campParty();bool placed=false;
            if(party.empty())campMessage="Bring a Pokemon in your party to camp.";
            else {
                std::vector<CampCell> candidates;
                for(int y=local.y-5;y<=local.y+2;++y)for(int x=local.x-5;x<=local.x+2;++x){const int cx=x+2,cy=y+1;const int distance=std::abs(cx-local.x)+std::abs(cy-local.y);if(distance>=2&&distance<=5)candidates.push_back({x,y});}
                auto score=[&](CampCell p){int dx=p.x+2-local.x,dy=p.y+1-local.y;int front=local.facing==1?dy:local.facing==2?-dy:local.facing==3?-dx:dx;return (std::abs(dx)+std::abs(dy))*10+(front>0?0:15);};
                std::stable_sort(candidates.begin(),candidates.end(),[&](auto a,auto b){return score(a)<score(b);});
                for(const auto p:candidates){CampState camp;camp.id=++campIdentity;if(!camp.id)camp.id=++campIdentity;camp.group=local.mapGroup;camp.map=local.mapNumber;camp.elevation=local.elevation;camp.x=int16_t(p.x);camp.y=int16_t(p.y);
                    if(!campFootprint(camp,campClear))continue;auto ground=campGround(camp,[](int x,int y){return campLawn(x,y)&&!campEventTile(x,y);},CampCell{local.x,local.y});
                    // Keep the trainer's square free when initially placing the party.
                    std::stable_partition(ground.begin(),ground.end(),[](auto p){return !(p.x==local.x&&p.y==local.y)&&campClear(p.x,p.y);});
                    if(std::count_if(ground.begin(),ground.end(),[](auto p){return campClear(p.x,p.y);})<party.size()||!localCamp.start(camp,party,ground))continue;
                    campPending=roomState.connected;campWasOnline=roomState.connected;campBegan=now;placed=true;campMessage=campPending?"Reserving this campsite...":"Walk outside the green outline to pack up automatically.";break;
                }
                if(!placed)campMessage="The tent needs a clear patch of grass, an open entrance, and room for your party.";
            }
        }catch(const std::exception& e){campMessage=e.what();}
    }
    if(!campPending&&campMessage!=lastCampNotice&&campMessage!="Find a clear lawn, then set up camp."){lastCampNotice=campMessage;queueFieldNotice("camp",campMessage);}
    if(localCamp.state.id&&campPending){
        const auto id=localCamp.state.id;
        if(roomState.world.campDecisions[roomSlot]==id){if(roomState.world.camps[roomSlot].id==id){campPending=false;campMessage="Walk outside the green outline to pack up automatically.";}else endCamp("That campsite is occupied. Try another clear patch of grass.");}
        else if(now-campBegan>300)endCamp("The room could not reserve this campsite. Try again.");
    }
    if(localCamp.state.id&&!campPending&&roomState.connected&&roomState.world.campDecisions[roomSlot]==localCamp.state.id&&!roomState.world.camps[roomSlot].id)endCamp("The room released this campsite. Set up again to continue camping.");
    if(localCamp.state.id&&!campPending){localCamp.update(campClear);local.followerVisible=false;}
    for(unsigned slot=0;slot<MaxRoomPlayers;++slot){const auto& c=roomState.world.camps[slot];for(unsigned i=0;i<6;++i){if(!roomState.connected||slot==roomSlot||!c.id||i>=c.party.size()){campMotion[slot][i].clear();continue;}
        const auto& mon=c.party[i];PlayerState p;p.active=true;p.mapGroup=c.group;p.mapNumber=c.map;p.elevation=c.elevation;p.x=int16_t(mon.x/16);p.y=int16_t(mon.y/16);p.pixelX=mon.x;p.pixelY=mon.y;p.follower=mon.species;p.followerFacing=mon.facing;p.followerFrame=mon.mood==2?uint8_t((c.tick/12)%4):mon.frame;p.offsetY=mon.mood==1?int8_t(-std::abs(4-int(c.tick%16)/2)):0;p.flip=mon.mood;p.sequence=c.sequence;p.sampleTime=c.sampleTime;p.identity=(uint64_t(c.id)<<8)|slot;campMotion[slot][i].push(p,clockMs());}}
#ifdef FR_TEST_HARNESS
    if(now%60==0&&!diagnosticPath.empty()){
        std::ofstream f(diagnosticPath.parent_path()/"camp-check.txt");f<<"frame="<<now<<" active="<<(localCamp.state.id!=0)<<" pending="<<campPending<<" lawn="<<(local.active&&campLawn(local.x,local.y))<<" message="<<campMessage<<'\n';
        auto dump=[&](unsigned slot,const CampState& c){if(!c.id)return;f<<"camp="<<slot<<" id="<<c.id<<" map="<<unsigned(c.group)<<','<<unsigned(c.map)<<" tile="<<c.x<<','<<c.y<<" tick="<<c.tick<<" party="<<c.party.size()<<'\n';f<<"ground="<<slot;for(auto row:c.ground)f<<','<<row;f<<'\n';for(const auto& p:c.party)f<<"mon="<<slot<<" species="<<p.species<<" pixel="<<p.x<<','<<p.y<<" face="<<unsigned(p.facing)<<" frame="<<unsigned(p.frame)<<" mood="<<unsigned(p.mood)<<'\n';};
        if(roomState.connected){for(unsigned i=0;i<4;++i)dump(i,roomState.world.camps[i]);}else dump(roomSlot,localCamp.state);
    }
#endif
}
int fieldCollisionHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!local.active||replaying()||nativeLinkScene())return 0;
    const int x=int16_t(cpu->R[1])-7,y=int16_t(cpu->R[2])-7;
    const unsigned player=r8(0x0203707d);
    // The perimeter confines camp Pokemon, not trainers. Native terrain and
    // the physical tent still collide; leaving the area ends the camp above.
    if(campBlocked(x,y))return guestReturn(cpu,4);
    if(roomState.connected&&player<16&&cpu->R[0]==0x02036e38+player*36&&safeStory()&&ownStarter()){
        // Native terrain, elevation, actor and previous-tile collision win first.
        const auto collision=callGuest(address,cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3]);
        if(collision)return guestReturn(cpu,collision);
        if(blockStoryGate(x,y))return guestReturn(cpu,4);
        return guestReturn(cpu,0);
    }
    return 0;
}
void drawCamps(uint8_t* rgb,unsigned w,unsigned h,int dx,int dy){
    if(!localCamp.state.id&&std::none_of(roomState.world.camps.begin(),roomState.world.camps.end(),[](const auto& c){return c.id!=0;}))return;
    try{loadCampArt();}catch(...){return;}
    for(unsigned slot=0;slot<MaxRoomPlayers;++slot){const auto& c=slot==roomSlot?localCamp.state:roomState.world.camps[slot];
        if(!c.id||(slot==roomSlot&&campPending)||c.group!=local.mapGroup||c.map!=local.mapNumber||c.elevation!=local.elevation||(!roomState.connected&&slot!=roomSlot))continue;
        // A ground-only perimeter sits beneath every same-plane object and BG1
        // foreground pixel. Native menus, windows, fades and palette effects
        // still go through the existing latched compositor.
        const auto* ppu=gbarecomp::active_ppu();const auto* context=ppu?ppu->obj_composition_frame():nullptr;
        auto& border=campBorders[slot];if(border.first.id!=c.id||border.first.x!=c.x||border.first.y!=c.y||border.first.ground!=c.ground)border={c,campPerimeter(c)};
        if(context){drawingKey=campGroundKey(r16(0x0400000c)&3);
            constexpr uint32_t rgba=0xfface89e;constexpr uint16_t color=uint16_t((158>>3)|((232>>3)<<5)|((172>>3)<<10));
            for(const auto& edge:border.second)if(campLawn(edge.cell.x,edge.cell.y))for(int n=0;n<16;++n)for(int inset=0;inset<CampOutlineWidth;++inset){
                const int px=edge.cell.x*16+dx+(edge.direction==2?inset:edge.direction==3?15-inset:n);
                const int py=edge.cell.y*16+dy+(edge.direction==0?15-inset:edge.direction==1?inset:n);
                if(px<0||py<0||px>=240||py>=160)continue;const auto& native=context->pixels[py*240+px];
                if(campGroundVisible(native))hostPixel(px,py,color,rgba);
            }
        }
        const auto& art=campFrames[(c.tick/12)%3];const int x=c.x*16+4+dx,y=c.y*16+9+dy;
        drawingKey=spriteKey(c.elevation,y+art.height,80+slot);
        for(int yy=0;yy<art.height;++yy)for(int xx=0;xx<art.width;++xx){const auto rgba=art.pixels[yy*art.width+xx];if(!(rgba>>24))continue;const uint16_t color=uint16_t(((rgba&255)>>3)|((((rgba>>8)&255)>>3)<<5)|((((rgba>>16)&255)>>3)<<10));hostPixel(x+xx,y+yy,color,rgba);}
        for(unsigned i=0;i<c.party.size();++i){const auto& mon=c.party[i];PlayerState p;
            if(slot==roomSlot){p.active=true;p.pixelX=mon.x;p.pixelY=mon.y;p.follower=mon.species;p.followerFacing=mon.facing;p.followerFrame=mon.mood==2?uint8_t((c.tick/12)%4):mon.frame;p.offsetY=mon.mood==1?int8_t(-std::abs(4-int(c.tick%16)/2)):0;}
            else p=campMotion[slot][i].sample(clockMs());if(!p.active)continue;
            drawingKey=spriteKey(c.elevation,p.pixelY+dy+16,96+slot*6+i);follower(rgb,w,h,p,p.pixelX+dx+8,p.pixelY+dy+p.offsetY);
        }
    }
}
