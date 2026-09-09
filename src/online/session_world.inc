// Save transfers are bounded and confined to the authenticated player's world slot.
std::unique_ptr<fr::WorldPlayers> worldPlayers;
std::string expectedRom,credential;
std::vector<uint8_t> downloadedCheckpoint;
struct Transfer {uint32_t sequence=0,total=0;Bytes bytes;};
std::array<Transfer,MaxRoomPlayers> transfers{};
uint32_t saveSequence=0;
std::array<uint32_t,MaxRoomPlayers> committedRequests{};
void sendCheckpoint(int slot,uint32_t serial,const Bytes& data){
    for(size_t offset=0;offset<data.size()||offset==0;offset+=8192){
        Bytes b;dword(b,serial);dword(b,uint32_t(data.size()));dword(b,uint32_t(offset));dword(b,current.hosting?0:current.checkpointRequest);
        const auto end=std::min(data.size(),offset+8192);b.insert(b.end(),data.begin()+offset,data.begin()+end);
        sendTo(slot,PlayerCheckpoint,b);
    }
}
void readCheckpoint(Reader& r,int from){
    if(!current.managedWorld)throw std::runtime_error("No world save session.");
    const auto serial=r.u32(),total=r.u32(),offset=r.u32(),request=r.u32();
    if(current.hosting&&request>current.checkpointRequest)throw std::runtime_error("Invalid world save request receipt.");
    if(!serial||total>fr::WorldCheckpointLimit||offset>total||r.b.size()-r.p>8192||r.b.size()-r.p>total-offset)throw std::runtime_error("Invalid checkpoint chunk.");
    if(current.hosting&&(from<1||from>=current.capacity||!peer(from)||total==0))throw std::runtime_error("Invalid checkpoint sender.");
    if(!current.hosting&&current.checkpointReady)throw std::runtime_error("Unexpected world download.");
    auto& t=transfers[current.hosting?unsigned(from):0];
    if(offset==0){if(serial<=t.sequence)throw std::runtime_error("Stale checkpoint.");t={serial,total,{}};t.bytes.reserve(total);}
    if(t.sequence!=serial||t.total!=total||offset!=t.bytes.size())throw std::runtime_error("Out-of-order checkpoint.");
    t.bytes.insert(t.bytes.end(),r.b.begin()+r.p,r.b.end());r.p=r.b.size();
    if(t.bytes.size()!=total)return;
    if(total&&!fr::validCheckpoint(t.bytes))throw std::runtime_error("Damaged world checkpoint.");
    if(current.hosting){worldPlayers->commit(peer(from)->id,t.bytes);committedRequests[from]=request;Bytes ack;dword(ack,serial);sendTo(from,CheckpointAck,ack);}
    else{downloadedCheckpoint=t.bytes;current.checkpointReady=true;changed.notify_all();}
    t.bytes.clear();
}
void requestWorldSave(){
    if(!current.hosting||!current.managedWorld)return;
    Bytes b;dword(b,++current.checkpointRequest);broadcast(CheckpointRequest,b);
}
