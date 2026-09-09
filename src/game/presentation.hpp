#pragma once
#include <cstdint>
namespace fr::game {
inline bool questLogPlayback(uint8_t state,uint32_t playback){return state==2||state==3||playback==1||playback==3;}
// Field loading/door callbacks also run while the native palette is black.
// Full-screen inventory, PC, summary, title and recap callbacks are excluded.
inline bool fieldSceneTransition(uint32_t callback,uint32_t codeOffset){
    switch((callback&~1u)-codeOffset){
    case 0x080565a8:case 0x080565b4:case 0x080566a4:case 0x0805671c:case 0x0805674c:
    case 0x080c9bd0:case 0x080c9bfc:
    case 0x080567dc:case 0x08056808:case 0x080568a8:case 0x080568c4:case 0x080568e0:
        return true;
    default:return false;
    }
}
// Exact supported FireRed callbacks. Full-screen Bag/party/summary screens
// must remain visible; only battle setup, fades and field return hold 3D.
inline bool battleSceneTransition(uint32_t callback,uint32_t codeOffset){
    callback=(callback&~1u)-codeOffset;
    switch(callback){
    case 0x0800fd9c:case 0x0800fe24:case 0x08010508:case 0x08011100:
    case 0x080565a8:case 0x080565b4:case 0x080566a4:case 0x0805671c:case 0x0805674c:
    case 0x080567dc:case 0x08056808:case 0x080568c4:case 0x080568e0:
    case 0x080777e8:case 0x0807fb40:case 0x0807fba0:case 0x0807fbf0:
    case 0x080804ac:case 0x08080558:case 0x080a0f4c:case 0x08108cf0:case 0x08128184:
        return true;
    default:return false;
    }
}
}
