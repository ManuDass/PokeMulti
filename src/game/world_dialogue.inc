// Native field messages use the ROM's selected frame, font, text printer,
// pagination and A/B dismissal. Never interrupt an existing script or battle.
struct FieldNotice {std::string key,text;};
std::deque<FieldNotice> fieldNotices;
uint32_t noticeAllocation=0;uint64_t noticeStarted=0;
std::string noticeText;
void queueFieldNotice(std::string key,std::string text){
    if(text.empty())return;
    for(auto& n:fieldNotices)if(n.key==key){n.text=std::move(text);return;}
    if(fieldNotices.size()>=32)return;
    fieldNotices.push_back({std::move(key),std::move(text)});
}
uint32_t makeFieldNoticeScript(const std::string& message,bool releaseObjects=false){
    if(noticeAllocation)return 0;
    const auto text=dialogueText(message,[](uint8_t glyph){return unsigned(std::clamp<int>(r8(gameAddress(0x08207300)+glyph),1,16));});
    const auto mem=callGuest(gameAddress(0x08002bb0),uint32_t(text.size()+16));
    if(!validRam(mem,uint32_t(text.size()+16)))return 0;
    const uint8_t script[]{0xc7,2,0x67,uint8_t(mem+16),uint8_t((mem+16)>>8),uint8_t((mem+16)>>16),uint8_t((mem+16)>>24),0x66,0x6d,uint8_t(releaseObjects?0x6b:0x68),0x02};
    for(unsigned i=0;i<sizeof(script);++i)bus()->write8(mem+i,script[i]);
    for(unsigned i=0;i<text.size();++i)bus()->write8(mem+16+i,text[i]);
    noticeText=message;noticeAllocation=mem;noticeStarted=now;
    std::printf("FIELD_NOTICE %s\n",noticeText.c_str());return mem;
}
bool fieldUiBusy();
void updateFieldNotices(){
    if(noticeAllocation){
        if(now>noticeStarted+2&&safeStory()){
            callGuest(gameAddress(0x08002bc4),noticeAllocation);noticeAllocation=0;noticeText.clear();
        }else return;
    }
    if(fieldUiBusy())return;
    if(fieldNotices.empty()||!safeStory()||encounterToken||!pendingStory.empty()||nativeLinkScene())return;
    if(local.pixelX!=local.x*16||local.pixelY!=local.y*16)return;
    const auto script=makeFieldNoticeScript(fieldNotices.front().text);if(!script)return;
    fieldNotices.pop_front();callGuest(gameAddress(0x08069ae4),script);
}
