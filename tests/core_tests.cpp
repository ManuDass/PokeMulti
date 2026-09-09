#include "rom/rom.hpp"
#include "runtime/probe.hpp"
#include "frontend/profile.hpp"
#include "platform/text.hpp"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
namespace {
int passed = 0, failed = 0;
void expect(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
void test(const char* name, const std::function<void()>& fn) {
    try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
}
void rejects(const std::function<void()>& fn) {
    bool threw = false;
    try { fn(); } catch (const std::exception&) { threw = true; }
    expect(threw,"Invalid input was accepted.");
}
void word(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (size_t i=0;i<4;++i) bytes.at(offset+i)=static_cast<uint8_t>(value >> (8*i));
}
void checksum(std::vector<uint8_t>& bytes) {
    uint8_t sum=0;
    for (size_t i=0xA0;i<=0xBC;++i) sum=static_cast<uint8_t>(sum-bytes[i]);
    bytes[0xBD]=static_cast<uint8_t>(sum-0x19);
}
std::vector<uint8_t> headerFixture() {
    // Original zero-filled test fixture. No Nintendo logo, ROM code, art or assets.
    std::vector<uint8_t> bytes(fr::FireRedRomSize);
    const std::string title="POKEMON FIRE", code="BPRE";
    std::copy(title.begin(),title.end(),bytes.begin()+0xA0);
    std::copy(code.begin(),code.end(),bytes.begin()+0xAC);
    bytes[0xB0]='0'; bytes[0xB1]='1'; bytes[0xB2]=0x96; checksum(bytes);
    return bytes;
}
struct Scratch {
    std::filesystem::path root=std::filesystem::temp_directory_path()/("fr-core-tests-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    Scratch() { std::filesystem::create_directories(root); }
    ~Scratch() { std::error_code ec; std::filesystem::remove_all(root,ec); }
};
}
int main() {
    using namespace fr;
    using namespace fr::runtime;
    test("SHA-256 empty known answer",[] { expect(digest({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA-256 mismatch"); });
    test("SHA-256 and SHA-1 abc known answers",[] {
        const uint8_t bytes[]{'a','b','c'};
        expect(digest(bytes)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 mismatch");
        expect(digest(bytes,false)=="a9993e364706816aba3e25717850c26c9cd0d89d","SHA-1 mismatch");
    });
    test("SHA-256 multi-chunk known answer",[] {
        std::vector<uint8_t> bytes(1000000,'a');
        expect(digest(bytes)=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","Long SHA-256 mismatch");
    });
    test("Published revisions and mismatched revision header",[] {
        expect(identifyRevision("41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc",0)==FireRedRevision::FireRed_US_10,"v1.0 identity");
        expect(identifyRevision("dd5945db9b930750cb39d00c84da8571feebf417",1)==FireRedRevision::FireRed_US_11,"v1.1 identity");
        expect(identifyRevision("dd5945db9b930750cb39d00c84da8571feebf417",0)==FireRedRevision::Unsupported,"Mismatched revision accepted");
        expect(identifyRevision(std::string(40,'0'),0)==FireRedRevision::Unsupported,"Unknown hash accepted");
    });
    test("Truncated ROM has diagnostic hash",[] { auto r=inspectRom({}); expect(!r.supported() && r.sha256.size()==64 && !r.error.empty(),"Missing rejection"); });
    test("Forged valid header does not grant support",[] { auto b=headerFixture(); auto r=inspectRom(b); expect(!r.supported() && r.error.find("Modified")!=std::string::npos,"Header-only acceptance"); });
    test("Invalid header checksum rejected",[] { auto b=headerFixture(); b[0xBD]^=1; expect(inspectRom(b).error.find("checksum")!=std::string::npos,"Bad checksum accepted"); });
    test("LeafGreen header rejected",[] { auto b=headerFixture(); b[0xAF]='G'; checksum(b); expect(!inspectRom(b).supported(),"LeafGreen accepted"); });
    test("Oversized and short input rejected",[] {
        std::vector<uint8_t> shortRom(256); expect(!inspectRom(shortRom).supported(),"Short input accepted");
        std::vector<uint8_t> large(32*1024*1024+1); rejects([&] { inspectRom(large); });
    });
    test("Unicode and quoted profile round trip with replacement",[] {
        Scratch s;
        Profile p{s.root/L"名字 with spaces.gba",std::string(64,'a'),"Trainer \"A\""};
        saveProfile(s.root/"profile.cfg",p);
        expect(loadProfile(s.root/"profile.cfg")==p,"Profile round trip failed");
        p.playerName="旅人"; saveProfile(s.root/"profile.cfg",p);
        expect(loadProfile(s.root/"profile.cfg")==p,"Atomic replacement failed");
        expect(!loadProfile(s.root/"missing.cfg"),"Missing profile should not exist");
    });
    test("Username edit preserves ROM, identity and world save bytes",[] {
        Scratch s;Profile p{s.root/L"local.gba",std::string(64,'a'),"Original"};
        saveProfile(s.root/"profile.cfg",p);
        for(const auto* name:{"identity.cfg","identity.key","checkpoint.pmsv","friends.cfg"}){std::ofstream f(s.root/name,std::ios::binary);f<<"unchanged-personal-data";}
        auto renamed=renameProfile(s.root/"profile.cfg","New \"Name\"");
        expect(renamed.playerName=="New \"Name\""&&renamed.romPath==p.romPath&&renamed.romSha256==p.romSha256,"Rename changed profile ownership");
        expect(loadProfile(s.root/"profile.cfg")==renamed,"Rename did not persist");
        for(const auto* name:{"identity.cfg","identity.key","checkpoint.pmsv","friends.cfg"}){std::ifstream f(s.root/name,std::ios::binary);std::string value((std::istreambuf_iterator<char>(f)),{});expect(value=="unchanged-personal-data","Rename changed personal data");}
        rejects([&]{renameProfile(s.root/"profile.cfg","   ");});
        expect(loadProfile(s.root/"profile.cfg")==renamed,"Rejected name damaged existing profile");
    });
    test("Malformed profile preserves existing data on save rejection",[] {
        Scratch s; Profile p{s.root/"local.gba",std::string(64,'a'),"Trainer"};
        saveProfile(s.root/"profile.cfg",p);
        auto bad=p; bad.romSha256="invalid"; rejects([&] { saveProfile(s.root/"profile.cfg",bad); });
        expect(loadProfile(s.root/"profile.cfg")==p,"Existing profile damaged");
        bad=p; bad.playerName=" \n "; rejects([&] { validateProfile(bad); });
        bad=p; bad.romPath=L"relative.gba"; rejects([&] { validateProfile(bad); });
    });
    test("Duplicate unknown and oversized profiles rejected",[] {
        Scratch s; auto file=s.root/"bad.cfg";
        { std::ofstream out(file); out << "schema \"1\"\nschema \"1\"\n"; }
        rejects([&] { loadProfile(file); });
        { std::ofstream out(file); out << "schema \"99\"\n"; }
        rejects([&] { loadProfile(file); });
        { std::ofstream out(file); out << std::string(17000,'a'); }
        rejects([&] { loadProfile(file); });
    });
    test("Invalid UTF-8 rejected",[] { rejects([] { widen(std::string("\xFF",1)); }); });
    test("ARM branch PC bias, backwards branch and BL link",[] {
        expect(decodeArm(0xEA000002,RomBase).value==RomBase+16,"ARM PC+8 incorrect");
        expect(decodeArm(0xEAFFFFFE,RomBase).value==RomBase,"Signed ARM branch incorrect");
        std::vector<uint8_t> bytes(32); word(bytes,0,0xEB000002);
        CpuState cpu; cpu.registers[15]=RomBase;
        expect(step(bytes,cpu).executed && cpu.registers[14]==RomBase+4 && cpu.registers[15]==RomBase+16,"BL behavior incorrect");
    });
    test("ARM rotated MOV and unsupported flag-writing form",[] {
        const auto d=decodeArm(0xE3A00480,RomBase);
        expect(d.op==Op::MoveImmediate && d.value==0x80000000,"Rotated immediate incorrect");
        expect(decodeArm(0xE3B00001,RomBase).op==Op::Unsupported,"Unsupported MOVS decoded");
        expect(decodeArm(0xE3A0F001,RomBase).op==Op::Unsupported,"PC-writing MOV accepted");
    });
    test("ARM to Thumb interworking and Thumb MOV flags",[] {
        std::vector<uint8_t> bytes(32); word(bytes,0,0xE12FFF10); word(bytes,16,0x00002100);
        CpuState cpu; cpu.registers[15]=RomBase; cpu.registers[0]=RomBase+17; cpu.carry=true; cpu.overflow=true;
        expect(step(bytes,cpu).executed && cpu.instructionSet==InstructionSet::Thumb && cpu.registers[15]==RomBase+16,"ARM BX failed");
        expect(step(bytes,cpu).executed && cpu.zero && !cpu.negative && cpu.carry && cpu.overflow,"Thumb MOV flags failed");
    });
    test("Thumb backwards branch and Thumb to ARM BX",[] {
        expect(decodeThumb(0xE7FE,RomBase).value==RomBase,"Thumb signed branch incorrect");
        std::vector<uint8_t> bytes(32); word(bytes,0,0x00004708);
        CpuState cpu; cpu.instructionSet=InstructionSet::Thumb; cpu.registers[15]=RomBase; cpu.registers[1]=RomBase+16;
        expect(step(bytes,cpu).executed && cpu.instructionSet==InstructionSet::Arm && cpu.registers[15]==RomBase+16,"Thumb BX failed");
        expect(decodeThumb(0x4780,RomBase).op==Op::Unsupported,"ARMv5 BLX accepted");
    });
    test("ARM and Thumb literal load pipeline alignment",[] {
        std::vector<uint8_t> bytes(32); word(bytes,0,0xE59F0004); word(bytes,12,0x12345678);
        CpuState cpu; cpu.registers[15]=RomBase;
        expect(step(bytes,cpu).executed && cpu.registers[0]==0x12345678,"ARM literal failed");
        word(bytes,0,0x48020000);
        cpu.registers[15]=RomBase+2; cpu.instructionSet=InstructionSet::Thumb;
        expect(step(bytes,cpu).executed && cpu.registers[0]==0x12345678,"Thumb aligned PC literal failed");
    });
    test("Condition predicates and failed condition skip",[] {
        CpuState cpu; cpu.zero=true; cpu.carry=true;
        expect(conditionPassed(0,cpu) && !conditionPassed(1,cpu) && !conditionPassed(8,cpu) && conditionPassed(9,cpu),"Condition flags incorrect");
        expect(conditionPassed(10,cpu) && !conditionPassed(11,cpu) && !conditionPassed(12,cpu) && conditionPassed(13,cpu),"Signed conditions incorrect");
        std::vector<uint8_t> bytes(8); word(bytes,0,0x13A00005); cpu.registers[15]=RomBase;
        expect(step(bytes,cpu).executed && cpu.registers[0]==0 && cpu.registers[15]==RomBase+4,"Condition skip failed");
    });
    test("Unknown opcode and invalid memory preserve CPU state",[] {
        std::vector<uint8_t> bytes(8); word(bytes,0,0xEFFFFFFF);
        CpuState cpu; cpu.registers[15]=RomBase;
        expect(!step(bytes,cpu).executed && cpu.registers[15]==RomBase,"Unknown opcode mutated state");
        word(bytes,0,0xE59F0FFF);
        expect(!step(bytes,cpu).executed && cpu.registers[15]==RomBase,"Invalid load mutated state");
        cpu.registers[15]=RomBase+1; expect(!step(bytes,cpu).executed,"Misaligned fetch accepted");
        cpu.registers[15]=0x02000000; expect(!step(bytes,cpu).executed,"Unmapped RAM accepted");
    });
    test("Finite probe budget and reserved ARMv4T instruction",[] {
        std::vector<uint8_t> bytes(4); word(bytes,0,0xEAFFFFFE);
        const auto p=probe(bytes,4); expect(p.executed==4 && p.cpu.registers[15]==RomBase,"Loop budget failed");
        rejects([&] { probe(bytes,0); }); rejects([&] { probe(bytes,4097); });
        word(bytes,0,0xFA000000); expect(probe(bytes).executed==0,"Reserved ARMv4T instruction accepted");
    });
    std::cout << passed << " passed, " << failed << " failed.\n";
    return failed ? 1 : 0;
}
