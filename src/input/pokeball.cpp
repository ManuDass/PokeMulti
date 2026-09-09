#include "input/pokeball.hpp"
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
namespace fr::input {
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;
namespace {
const guid inputService{L"6675e16c-f36d-4567-bb55-6b51e27a23e5"};
const guid inputCharacteristic{L"6675e16c-f36d-4567-bb55-6b51e27a23e6"};
const guid batteryService{L"0000180f-0000-1000-8000-00805f9b34fb"};
const guid batteryCharacteristic{L"00002a19-0000-1000-8000-00805f9b34fb"};
struct Cancelled {};
struct Shared {
    mutable std::mutex mutex;std::condition_variable wake;
    BallStatus status;uint64_t generation=0,target=0;int command=0;bool quit=false;
};
bool current(const std::shared_ptr<Shared>& s,uint64_t g){std::lock_guard lock(s->mutex);return !s->quit&&s->generation==g;}
void delay(const std::shared_ptr<Shared>& s,uint64_t g,int ms){
    std::unique_lock lock(s->mutex);s->wake.wait_for(lock,std::chrono::milliseconds(ms),[&]{return s->quit||s->generation!=g;});
    if(s->quit||s->generation!=g)throw Cancelled{};
}
template<class Operation> auto awaitResult(Operation op,const std::shared_ptr<Shared>& s,uint64_t g){
    const auto deadline=ballTime()+10000;
    try{while(op.Status()==AsyncStatus::Started){if(ballTime()>deadline)throw std::runtime_error("Bluetooth timed out. Wake the ball and try again.");delay(s,g,30);}
        if(!current(s,g))throw Cancelled{};return op.GetResults();
    }catch(...){try{op.Cancel();}catch(...){}throw;}
}
void message(const std::shared_ptr<Shared>& s,uint64_t g,BallPhase phase,const std::string& text){
    std::lock_guard lock(s->mutex);if(s->quit||s->generation!=g)return;
    s->status.phase=phase;s->status.message=text;
    if(phase==BallPhase::Error){s->status.sample={};s->status.lastReport=0;}
}
std::vector<uint8_t> bytes(const IBuffer& buffer){
    if(!buffer||buffer.Length()>512)return {};
    std::vector<uint8_t> b(buffer.Length());DataReader::FromBuffer(buffer).ReadBytes(b);return b;
}
struct Connection {
    BluetoothLEDevice device{nullptr};GattDeviceService inputService{nullptr},batteryService{nullptr};
    GattCharacteristic input{nullptr},battery{nullptr};GattSession session{nullptr};
    GattCharacteristic::ValueChanged_revoker reports,batteryReports;
    HANDLE exclusive=nullptr;
    ~Connection(){
        reports.revoke();batteryReports.revoke();input=nullptr;battery=nullptr;
        try{if(session){session.MaintainConnection(false);session.Close();}}catch(...){}
        try{if(inputService)inputService.Close();}catch(...){}
        try{if(batteryService)batteryService.Close();}catch(...){}
        try{if(device)device.Close();}catch(...){}
        if(exclusive){ReleaseMutex(exclusive);CloseHandle(exclusive);}
    }
};
void scanDevices(const std::shared_ptr<Shared>& s,uint64_t g){
    auto adapter=awaitResult(BluetoothAdapter::GetDefaultAsync(),s,g);
    if(!adapter||!adapter.IsLowEnergySupported())throw std::runtime_error("No Bluetooth LE adapter found on this PC.");
    const auto radio=awaitResult(adapter.GetRadioAsync(),s,g);
    if(radio&&radio.State()!=Windows::Devices::Radios::RadioState::On)throw std::runtime_error("Turn on Bluetooth in Windows, then scan again.");
    BluetoothLEAdvertisementWatcher watcher;
    watcher.ScanningMode(BluetoothLEScanningMode::Active);
    auto received=watcher.Received(auto_revoke,[weak=std::weak_ptr<Shared>(s),g](const auto&,const BluetoothLEAdvertisementReceivedEventArgs& e){
        try{
            if(e.Advertisement().LocalName()!=L"Pokemon PBP")return;
            if(auto p=weak.lock()){
                std::lock_guard lock(p->mutex);if(p->quit||p->generation!=g)return;
                auto& devices=p->status.devices;auto found=std::find_if(devices.begin(),devices.end(),[&](const auto& d){return d.address==e.BluetoothAddress();});
                if(found!=devices.end()){found->signal=e.RawSignalStrengthInDBm();return;}
                if(devices.size()>=16)return;char suffix[20];std::snprintf(suffix,sizeof(suffix),"%04llX",e.BluetoothAddress()&0xffff);
                devices.push_back({e.BluetoothAddress(),std::string("Pok").append("\xc3\xa9 Ball Plus / ")+suffix,e.RawSignalStrengthInDBm()});
            }
        }catch(...){} // Never unwind through a Windows notification callback.
    });
    auto stopped=watcher.Stopped(auto_revoke,[weak=std::weak_ptr<Shared>(s),g](const auto&,const BluetoothLEAdvertisementWatcherStoppedEventArgs& e){
        if(e.Error()!=BluetoothError::Success)if(auto p=weak.lock())message(p,g,BallPhase::Error,"Bluetooth scan stopped. Check the Windows Bluetooth settings.");
    });
    struct Stop {BluetoothLEAdvertisementWatcher w;~Stop(){try{w.Stop();}catch(...){}}} stop{watcher};
    watcher.Start();const auto end=ballTime()+8000;
    while(ballTime()<end)delay(s,g,100);
    std::lock_guard lock(s->mutex);if(s->generation!=g||s->quit)return;
    if(s->status.phase==BallPhase::Error)return;
    s->status.phase=BallPhase::Found;
    s->status.message=s->status.devices.empty()?"No ball found. Press its top button, then scan again.":"Choose your ball to connect.";
}
void connectDevice(const std::shared_ptr<Shared>& s,uint64_t g,uint64_t address){
    Connection c;
    wchar_t mutexName[80];swprintf_s(mutexName,L"Local\\PokeMulti-PBP-%012llX",address);
    c.exclusive=CreateMutexW(nullptr,FALSE,mutexName);
    if(!c.exclusive)throw std::runtime_error("Cannot reserve this controller.");
    const auto acquired=WaitForSingleObject(c.exclusive,0);
    if(acquired!=WAIT_OBJECT_0&&acquired!=WAIT_ABANDONED){CloseHandle(c.exclusive);c.exclusive=nullptr;throw std::runtime_error("This ball is being used by another PokeMulti window.");}
    c.device=awaitResult(BluetoothLEDevice::FromBluetoothAddressAsync(address),s,g);
    if(!c.device)throw std::runtime_error("Cannot open the ball. Wake it and close other apps using it.");
    const auto services=awaitResult(c.device.GetGattServicesForUuidAsync(inputService,BluetoothCacheMode::Uncached),s,g);
    if(services.Status()!=GattCommunicationStatus::Success||!services.Services().Size())throw std::runtime_error("Ball input is unavailable. Disconnect it from your phone or Switch and retry.");
    c.inputService=services.Services().GetAt(0);
    const auto chars=awaitResult(c.inputService.GetCharacteristicsForUuidAsync(inputCharacteristic,BluetoothCacheMode::Uncached),s,g);
    if(chars.Status()!=GattCommunicationStatus::Success||!chars.Characteristics().Size())throw std::runtime_error("This device does not expose Pok\xc3\xa9 Ball Plus controls.");
    c.input=chars.Characteristics().GetAt(0);
    if((c.input.CharacteristicProperties()&GattCharacteristicProperties::Notify)==GattCharacteristicProperties::None)throw std::runtime_error("The ball does not offer input notifications.");
    c.session=awaitResult(GattSession::FromDeviceIdAsync(c.device.BluetoothDeviceId()),s,g);
    if(c.session&&c.session.CanMaintainConnection())c.session.MaintainConnection(true);
    c.reports=c.input.ValueChanged(auto_revoke,[weak=std::weak_ptr<Shared>(s),g](const auto&,const GattValueChangedEventArgs& e){
        try{const auto report=decodeBall(bytes(e.CharacteristicValue()));if(!report)return;
            if(auto p=weak.lock()){std::lock_guard lock(p->mutex);if(p->quit||p->generation!=g)return;
                p->status.sample=*report;p->status.lastReport=ballTime();++p->status.reportCount;
            }
        }catch(...){}
    });
    const auto subscribed=awaitResult(c.input.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify),s,g);
    if(subscribed!=GattCommunicationStatus::Success)throw std::runtime_error("Could not enable ball input. Wake the controller and retry.");
    message(s,g,BallPhase::Waiting,"Connected; waiting for controller input...");
    const auto start=ballTime();
    while(true){
        delay(s,g,60);BallStatus snapshot;{std::lock_guard lock(s->mutex);snapshot=s->status;}
        if(snapshot.lastReport){message(s,g,BallPhase::Connected,"Connected / release controls to play");break;}
        if(ballTime()-start>8000)throw std::runtime_error("Connected, but no input arrived. Wake the ball and reconnect.");
    }
    // Battery is optional; its failure must not stop a working input stream.
    try{
        const auto bs=awaitResult(c.device.GetGattServicesForUuidAsync(batteryService,BluetoothCacheMode::Cached),s,g);
        if(bs.Status()==GattCommunicationStatus::Success&&bs.Services().Size()){
            c.batteryService=bs.Services().GetAt(0);
            const auto bc=awaitResult(c.batteryService.GetCharacteristicsForUuidAsync(batteryCharacteristic,BluetoothCacheMode::Cached),s,g);
            if(bc.Status()==GattCommunicationStatus::Success&&bc.Characteristics().Size()){
                c.battery=bc.Characteristics().GetAt(0);
                const auto setBattery=[weak=std::weak_ptr<Shared>(s),g](const IBuffer& value){
                    const auto b=bytes(value);if(b.empty()||b[0]>100)return;
                    if(auto p=weak.lock()){std::lock_guard lock(p->mutex);if(!p->quit&&p->generation==g)p->status.battery=b[0];}
                };
                const auto br=awaitResult(c.battery.ReadValueAsync(BluetoothCacheMode::Uncached),s,g);
                if(br.Status()==GattCommunicationStatus::Success)setBattery(br.Value());
                if((c.battery.CharacteristicProperties()&GattCharacteristicProperties::Notify)!=GattCharacteristicProperties::None){
                    c.batteryReports=c.battery.ValueChanged(auto_revoke,[setBattery](const auto&,const GattValueChangedEventArgs& e){try{setBattery(e.CharacteristicValue());}catch(...){}});
                    awaitResult(c.battery.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify),s,g);
                }
            }
        }
    }catch(const Cancelled&){throw;}catch(...){}
    for(;;){
        delay(s,g,100);uint64_t last;{std::lock_guard lock(s->mutex);last=s->status.lastReport;}
        if(c.device.ConnectionStatus()==BluetoothConnectionStatus::Disconnected)throw std::runtime_error("Ball disconnected. Wake it, then reconnect.");
        if(ballTime()-last>8000)throw std::runtime_error("Ball stopped sending input. Wake it, then reconnect.");
        message(s,g,ballTime()-last>750?BallPhase::Waiting:BallPhase::Connected,ballTime()-last>750?"Waiting for fresh controller input...":"Connected");
    }
}
}
struct Pokeball::Impl {
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();std::thread worker;
    Impl():worker([s=shared]{
        try{init_apartment(apartment_type::multi_threaded);
            for(;;){int task;uint64_t generation,address;
                {std::unique_lock lock(s->mutex);s->wake.wait(lock,[&]{return s->quit||s->command;});if(s->quit)break;
                    task=s->command;s->command=0;generation=s->generation;address=s->target;}
                try{if(task==1)scanDevices(s,generation);else if(task==2)connectDevice(s,generation,address);}
                catch(const Cancelled&){}
                catch(const hresult_error& e){message(s,generation,BallPhase::Error,"Windows Bluetooth: "+to_string(e.message()));}
                catch(const std::exception& e){message(s,generation,BallPhase::Error,e.what());}
            }uninit_apartment();
        }catch(...){std::lock_guard lock(s->mutex);s->status.phase=BallPhase::Error;s->status.message="Windows Bluetooth could not start.";s->quit=true;}
    }){}
    ~Impl(){{std::lock_guard lock(shared->mutex);shared->quit=true;++shared->generation;}shared->wake.notify_all();worker.join();}
    void request(int task,uint64_t target=0){
        {std::lock_guard lock(shared->mutex);if(shared->quit)return;++shared->generation;shared->command=task;shared->target=target;
            auto& st=shared->status;st.sample={};st.lastReport=st.reportCount=0;st.battery=-1;st.address=target;
            if(task==1){st.devices.clear();st.phase=BallPhase::Scanning;st.message="Scanning / press the ball's top button";}
            else if(task==2){st.phase=BallPhase::Connecting;st.message="Connecting to your ball...";}
            else{st.phase=BallPhase::Idle;st.message="Disconnected";}}
        shared->wake.notify_all();
    }
};
Pokeball::Pokeball():impl_(std::make_unique<Impl>()){}
Pokeball::~Pokeball()=default;
void Pokeball::scan(){impl_->request(1);}
void Pokeball::connect(uint64_t address){if(address)impl_->request(2,address);}
void Pokeball::disconnect(){impl_->request(3);}
BallStatus Pokeball::status()const{
    std::lock_guard lock(impl_->shared->mutex);auto status=impl_->shared->status;
    if(status.phase==BallPhase::Connected&&(!status.lastReport||ballTime()-status.lastReport>750)){
        status.phase=BallPhase::Waiting;status.message="Waiting for fresh controller input...";
    }return status;
}
}
