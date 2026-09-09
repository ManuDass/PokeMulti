#include "runtime/probe.hpp"
#include "rom/rom.hpp"
#include <bit>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace fr::runtime {
namespace {
uint32_t signExtend(uint32_t value, unsigned bits) {
    const auto sign = uint32_t{1} << (bits - 1);
    return (value ^ sign) - sign; // Defined unsigned arithmetic, including negative offsets.
}
uint32_t read(std::span<const uint8_t> rom, uint32_t address, size_t count) {
    if (address < RomBase || uint64_t{address - RomBase} + count > rom.size())
        throw std::runtime_error("Read outside the mapped ROM; RAM and hardware are not implemented.");
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i) value |= uint32_t{rom[address - RomBase + i]} << (8 * i);
    return value;
}
std::string hex(uint32_t value) {
    std::ostringstream out; out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value; return out.str();
}
const char* name(Op op) {
    switch (op) {
    case Op::Branch: return "B";
    case Op::BranchLink: return "BL";
    case Op::BranchExchange: return "BX";
    case Op::MoveImmediate: return "MOV immediate";
    case Op::LoadLiteral: return "LDR literal";
    default: return "unsupported";
    }
}
}
Decoded decodeArm(uint32_t word, uint32_t address) {
    Decoded d;
    d.condition = static_cast<uint8_t>(word >> 28);
    if (d.condition == 15) return d; // ARMv4T does not implement ARMv5 BLX-immediate.
    if ((word & 0x0E000000) == 0x0A000000) {
        d.op = (word & 0x01000000) ? Op::BranchLink : Op::Branch;
        d.value = address + 8 + (signExtend(word & 0xFFFFFF, 24) << 2);
    } else if ((word & 0x0FFFFFF0) == 0x012FFF10 && (word & 15) != 15) {
        d.op = Op::BranchExchange; d.source = static_cast<uint8_t>(word & 15);
    } else if ((word & 0x0FFF0000) == 0x03A00000 && ((word >> 12) & 15) != 15) {
        // MOV immediate with S=0; flag-setting/PC-writing forms deliberately remain unsupported.
        d.op = Op::MoveImmediate; d.destination = static_cast<uint8_t>((word >> 12) & 15);
        d.value = std::rotr(word & 255, static_cast<int>(((word >> 8) & 15) * 2));
    } else if ((word & 0x0F7F0000) == 0x051F0000 && ((word >> 12) & 15) != 15) {
        d.op = Op::LoadLiteral; d.destination = static_cast<uint8_t>((word >> 12) & 15);
        d.value = address + 8 + ((word & 0x00800000) ? (word & 0xFFF) : 0u - (word & 0xFFF));
    }
    return d;
}
Decoded decodeThumb(uint16_t word, uint32_t address) {
    Decoded d;
    if ((word & 0xF800) == 0xE000) { d.op = Op::Branch; d.value = address + 4 + (signExtend(word & 0x7FF, 11) << 1); }
    else if ((word & 0xF800) == 0x2000) {
        d.op = Op::MoveImmediate; d.destination = static_cast<uint8_t>((word >> 8) & 7); d.value = word & 255;
    } else if ((word & 0xFF87) == 0x4700 && ((word >> 3) & 15) != 15) {
        d.op = Op::BranchExchange; d.source = static_cast<uint8_t>((word >> 3) & 15);
    } else if ((word & 0xF800) == 0x4800) {
        d.op = Op::LoadLiteral; d.destination = static_cast<uint8_t>((word >> 8) & 7);
        d.value = ((address + 4) & ~3u) + (uint32_t{word & 255u} << 2);
    }
    return d;
}
bool conditionPassed(uint8_t c, const CpuState& s) {
    switch (c) {
    case 0: return s.zero; case 1: return !s.zero; case 2: return s.carry; case 3: return !s.carry;
    case 4: return s.negative; case 5: return !s.negative; case 6: return s.overflow; case 7: return !s.overflow;
    case 8: return s.carry && !s.zero; case 9: return !s.carry || s.zero;
    case 10: return s.negative == s.overflow; case 11: return s.negative != s.overflow;
    case 12: return !s.zero && s.negative == s.overflow; case 13: return s.zero || s.negative != s.overflow;
    case 14: return true; default: return false;
    }
}
StepResult step(std::span<const uint8_t> rom, CpuState& cpu) {
    const auto address = cpu.registers[15];
    const bool thumb = cpu.instructionSet == InstructionSet::Thumb;
    const uint32_t width = thumb ? 2u : 4u;
    try {
        if (address % width) return {false, "Misaligned instruction fetch at " + hex(address)};
        const auto word = read(rom, address, width);
        const auto d = thumb ? decodeThumb(static_cast<uint16_t>(word), address) : decodeArm(word, address);
        if (d.condition == 15) return {false, "Reserved ARMv4T condition at " + hex(address)};
        if (!conditionPassed(d.condition, cpu)) {
            cpu.registers[15] += width;
            return {true, hex(address) + " ARM condition failed; skipped"};
        }
        if (d.op == Op::Unsupported) return {false, "Unsupported " + std::string(thumb ? "Thumb" : "ARM") + " instruction at " + hex(address) + " (" + hex(word) + ")."};
        auto next = cpu;
        next.registers[15] += width;
        switch (d.op) {
        case Op::BranchLink: next.registers[14] = address + 4; [[fallthrough]];
        case Op::Branch: next.registers[15] = d.value; break;
        case Op::BranchExchange: {
            const auto target = cpu.registers[d.source];
            next.instructionSet = (target & 1) ? InstructionSet::Thumb : InstructionSet::Arm;
            next.registers[15] = target & ((target & 1) ? ~1u : ~3u);
            break;
        }
        case Op::MoveImmediate:
            next.registers[d.destination] = d.value;
            if (thumb) { next.negative = (d.value & 0x80000000) != 0; next.zero = d.value == 0; }
            break;
        case Op::LoadLiteral:
            if (d.value & 3) return {false, "Unaligned literal loads are not implemented."};
            next.registers[d.destination] = read(rom, d.value, 4);
            break;
        default: break;
        }
        cpu = next;
        return {true, hex(address) + (thumb ? " Thumb " : " ARM ") + name(d.op) + " -> PC " + hex(cpu.registers[15])};
    } catch (const std::exception& e) { return {false, e.what()}; }
}
ProbeResult probe(std::span<const uint8_t> rom, size_t budget) {
    if (budget == 0 || budget > 4096) throw std::runtime_error("Probe budget must be between 1 and 4096.");
    ProbeResult result;
    result.cpu.registers[15] = RomBase;
    for (size_t i = 0; i < budget; ++i) {
        const auto current = step(rom, result.cpu);
        if (!current.executed) { result.stopReason = current.message; return result; }
        ++result.executed; result.trace.push_back(current.message);
    }
    result.stopReason = "Instruction budget reached. This probe has no timing, graphics, BIOS, RAM, or game loop.";
    return result;
}
}
