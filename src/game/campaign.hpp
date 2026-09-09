#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace fr::game {
// World proofs travel over the story channel but never overwrite a trainer's
// earned badges, victories or personal receipt for a shared essential tool.
inline bool personalStoryState(uint16_t id){
    if((id>=0x500&&id<0x800)||(id>=0x820&&id<=0x827)||id==0x82c)return true;
    switch(id){case 0x237:case 0x238:case 0x239:case 0x23a:case 0x23b:case 0x2ef:
    case 0x254:case 0x297:case 0x231:case 0x293:case 0x259:case 0x29a:case 0x24e:case 0x298:
    case 0x29b:case 0x23f:case 0x234:case 0x23d:case 0x250:
    case 0x2a6:case 0x036:case 0x037:case 0x189:case 0x192:return true;default:return false;}
}
inline bool personalChallengeMap(uint8_t g,uint8_t m){
    return (g==1&&m>=75&&m<=80)||(g==5&&m==1)||(g==6&&m==2)||(g==7&&m==5)
        ||(g==9&&m==6)||(g==10&&m==16)||(g==11&&m==3)||(g==12&&m==0)||(g==14&&m==3);
}
struct CampaignObjective {uint16_t id,proof,minimum;const char* title;const char* update;};
inline constexpr CampaignObjective campaignObjectives[]{
 {1,0x4057,1,"Oak's parcel collected","Oak's parcel is on its way to Pallet Town."},
 {2,0x4057,2,"Oak's parcel delivered","Oak received his parcel. The Pokedex is ready for everyone!"},
 {3,0x233,1,"Bill rescued","Bill is human again! The S.S. Anne is now available."},
 {4,0x23c,1,"Mr. Fuji rescued","Mr. Fuji is safe. The Poke Flute is now available."},
 {5,0x053,1,"Silph Co. rescued","Team Rocket has left Silph Co.!"},
 {6,0x037,1,"Silph Scope recovered","The campaign recovered the Silph Scope."},
 {7,0x192,1,"Card Key recovered","The campaign can now unlock Silph Co. doors."},
 {8,0x036,1,"Lift Key recovered","The Rocket Hideout elevator is now available."},
 {9,0x2a6,1,"Tea obtained","The campaign can now pass the Saffron guards."},
 {10,0x4052,1,"Cerulean rival defeated","The rival encounter in Cerulean is complete."},
 {11,0x407d,1,"Stolen TM recovered","The Cerulean Rocket encounter is complete."},
 {12,0x2a3,1,"Lostelle rescued","Lostelle is safe!"},
 {13,0x237,1,"Cut learned","Cut is available to the campaign."},
 {14,0x238,1,"Fly learned","Fly is available to the campaign."},
 {15,0x239,1,"Surf learned","Surf is available to the campaign."},
 {16,0x23a,1,"Strength learned","Strength is available to the campaign."},
 {17,0x23b,1,"Flash learned","Flash is available to the campaign."},
 {18,0x2ef,1,"Rock Smash learned","Rock Smash is available to the campaign."},
 {19,0x189,1,"Gold Teeth recovered","The Warden's Gold Teeth have been found."},
 {32,0x820,1,"Pewter Gym cleared","Pewter Gym was cleared. Everyone can continue east."},
 {33,0x821,1,"Cerulean Gym cleared","Cerulean Gym was cleared. Campaign travel access advanced."},
 {34,0x822,1,"Vermilion Gym cleared","Vermilion Gym was cleared. Campaign travel access advanced."},
 {35,0x823,1,"Celadon Gym cleared","Celadon Gym was cleared. Campaign travel access advanced."},
 {36,0x824,1,"Fuchsia Gym cleared","Fuchsia Gym was cleared. Campaign travel access advanced."},
 {37,0x825,1,"Saffron Gym cleared","Saffron Gym was cleared. Campaign travel access advanced."},
 {38,0x826,1,"Cinnabar Gym cleared","Cinnabar Gym was cleared. Campaign travel access advanced."},
 {39,0x827,1,"Viridian Gym cleared","Viridian Gym was cleared. Campaign travel access advanced."}
};
inline const CampaignObjective* campaignObjective(uint16_t id){for(const auto& r:campaignObjectives)if(r.id==id)return &r;return nullptr;}
struct CampaignCompletion {uint16_t objective=0;uint32_t sequence=0;std::string actor;};
struct CampaignState {std::string id;uint32_t sequence=0;std::vector<CampaignCompletion> completed;};
}
