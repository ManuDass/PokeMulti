#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace fr::runtime {
inline constexpr char RuntimeVersion[] = "0.1.0";
enum class InstructionSet { Arm, Thumb };
enum class Op { Unsupported, Branch, BranchLink, BranchExchange, MoveImmediate, LoadLiteral };
struct Decoded {
    Op op = Op::Unsupported;
    uint8_t condition = 14, destination = 0, source = 0;
    uint32_t value = 0;
};
struct CpuState {
    // PC is the current fetch address; architectural PC reads add the pipeline bias.
    std::array<uint32_t,16> registers{};
    InstructionSet instructionSet = InstructionSet::Arm;
    bool negative = false, zero = false, carry = false, overflow = false;
};
struct StepResult { bool executed = false; std::string message; };
struct ProbeResult { size_t executed = 0; CpuState cpu; std::vector<std::string> trace; std::string stopReason; };
Decoded decodeArm(uint32_t word, uint32_t address);
Decoded decodeThumb(uint16_t word, uint32_t address);
bool conditionPassed(uint8_t condition, const CpuState& cpu);
StepResult step(std::span<const uint8_t> rom, CpuState& cpu);
ProbeResult probe(std::span<const uint8_t> rom, size_t budget = 32);
}
