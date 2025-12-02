#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <string>

constexpr uint32_t RAM_START = 0x00000;
constexpr uint32_t RAM_END   = 0x7FFFF;

constexpr uint32_t VRAM_START = 0x80000;
constexpr uint32_t VRAM_END   = 0x8FFFF;

constexpr uint32_t EXP_START = 0x90000;
constexpr uint32_t EXP_END   = 0x9FBFF;

constexpr uint32_t IO_START = 0x9FC00;
constexpr uint32_t IO_END   = 0x9FFFF;

constexpr uint32_t IO_SERIAL_OUT = 0x9FC00;

constexpr uint32_t VRAM_REFRESH_RATE = 20;

enum OPCODE : uint32_t {
    LUI     = 0b0110111,
    AUIPC   = 0b0010111,
    JAL     = 0b1101111,
    JALR    = 0b1100111,
    BRANCH  = 0b1100011,
    LOAD    = 0b0000011,
    STORE   = 0b0100011,
    ALU_IMM = 0b0010011,
    ALU_REG = 0b0110011
};

enum FUNCT3 : uint32_t {
    ADD_SUB = 0b000,
    SLL     = 0b001,
    SLT     = 0b010,
    SLTU    = 0b011,
    XOR_OP  = 0b100,
    SR_OP   = 0b101,
    OR_OP   = 0b110,
    AND_OP  = 0b111
};

class Bus {
public:
    std::vector<uint8_t> memory;  
    uint64_t cycles = 0;

    Bus(size_t size_bytes) {
        memory.resize(size_bytes, 0);
    }

    uint32_t read32(uint32_t address) {
        if(address >= IO_START && address <= IO_END) {
            return read_peripheral(address);
        }

        uint32_t b0 = memory[address];
        uint32_t b1 = memory[address+1];
        uint32_t b2 = memory[address+2];
        uint32_t b3 = memory[address+3];

        return (b3<<24) | (b2<<16) | (b1<<8) | b0;
    }

    void write32(uint32_t address, uint32_t data) {
        if(address >= IO_START && address <= IO_END) {
            write_peripheral(address, data);
            return;
        }

        memory[address]   =  data & 0xFF;
        memory[address+1] = (data >> 8)  & 0xFF;
        memory[address+2] = (data >> 16) & 0xFF;
        memory[address+3] = (data >> 24) & 0xFF;
    }

    uint32_t read_peripheral(uint32_t address) {
        return 0;
    }

    void write_peripheral(uint32_t address, uint32_t data) {
        if(address == IO_SERIAL_OUT) {
            std::cout << "[SERIAL] " << (char)data << std::endl;
        }
    }

    void dump_vram() {
        std::cout << "\n--- VRAM OUTPUT ---\n";

        for(uint32_t addr = VRAM_START; addr <= VRAM_END; ++addr) {
            char c = (char)memory[addr];
            if(c == 0) std::cout << '.';
            else std::cout << c;
        }

        std::cout << "\n--------------------\n";
    }
};

class CPU {
public:
    uint32_t pc = 0;
    uint32_t regs[32] {};
    Bus& bus;
    uint32_t instr_count = 0;

    CPU(Bus& b) : bus(b) {}

    uint32_t fetch() {
        uint32_t inst = bus.read32(pc);
        pc += 4;
        return inst;
    }

    struct Decoded {
        uint32_t opcode, rd, rs1, rs2, funct3, funct7;
        int32_t imm;
    };

    Decoded decode(uint32_t inst) {
        Decoded d {};
        d.opcode = inst & 0x7F;
        d.rd     = (inst >> 7) & 0x1F;
        d.funct3 = (inst >> 12) & 7;
        d.rs1    = (inst >> 15) & 0x1F;
        d.rs2    = (inst >> 20) & 0x1F;
        d.funct7 = (inst >> 25) & 0x7F;

        switch(d.opcode) {
            case ALU_IMM:
            case LOAD:
            case JALR:
                d.imm = (int32_t)inst >> 20;
                break;

            case STORE:
                d.imm = ((inst >> 7) & 0x1F) | (((int32_t)inst >> 25) << 5);
                break;

            case BRANCH:
                d.imm = (((inst >> 7) & 1) << 11)
                      | (((inst >> 8) & 0xF) << 1)
                      | (((inst >> 25) & 0x3F) << 5)
                      | (((inst >> 31) & 1) << 12);
                if(d.imm & 0x1000) d.imm |= 0xFFFFE000; 
                break;

            case LUI:
            case AUIPC:
                d.imm = (int32_t)(inst & 0xFFFFF000);
                break;

            case JAL:
                d.imm = (((inst >> 21) & 0x3FF) << 1)
                      | (((inst >> 20) & 1)    << 11)
                      | (((inst >> 12) & 0xFF) << 12)
                      | (((inst >> 31) & 1)    << 20);
                if(d.imm & 0x100000) d.imm |= 0xFFE00000;
                break;
        }

        return d;
    }

    void step() {
        uint32_t inst = fetch();
        Decoded d = decode(inst);

        instr_count++;

        switch(d.opcode) {

            case LUI:
                regs[d.rd] = d.imm;
                break;

            case AUIPC:
                regs[d.rd] = pc + d.imm - 4;
                break;

            case JAL:
                regs[d.rd] = pc;
                pc += d.imm - 4;
                break;

            case JALR: {
                uint32_t t = pc;
                pc = (regs[d.rs1] + d.imm) & ~1;
                regs[d.rd] = t;
            }
            break;

            case LOAD: {
                uint32_t addr = regs[d.rs1] + d.imm;
                regs[d.rd] = bus.read32(addr);
            }
            break;

            case STORE: {
                uint32_t addr = regs[d.rs1] + d.imm;
                bus.write32(addr, regs[d.rs2]);
            }
            break;

            case BRANCH:
                if((d.funct3 == 0 && regs[d.rs1] == regs[d.rs2]) ||     // BEQ
                   (d.funct3 == 1 && regs[d.rs1] != regs[d.rs2])) {     // BNE
                    pc += d.imm - 4;
                }
                break;

            case ALU_IMM:
                switch(d.funct3) {
                    case ADD_SUB: regs[d.rd] = regs[d.rs1] + d.imm; break;
                    case OR_OP:   regs[d.rd] = regs[d.rs1] | d.imm; break;
                    case AND_OP:  regs[d.rd] = regs[d.rs1] & d.imm; break;
                }
                break;

            case ALU_REG:
                if(d.funct7 == 0) {
                    switch(d.funct3) {
                        case ADD_SUB: regs[d.rd] = regs[d.rs1] + regs[d.rs2]; break;
                        case OR_OP:   regs[d.rd] = regs[d.rs1] | regs[d.rs2]; break;
                        case AND_OP:  regs[d.rd] = regs[d.rs1] & regs[d.rs2]; break;
                    }
                }
                break;
        }

        regs[0] = 0;

        if(instr_count % VRAM_REFRESH_RATE == 0)
            bus.dump_vram();
    }
};

int main() {
    Bus bus(1024*1024);
    CPU cpu(bus);

    bus.write32(IO_SERIAL_OUT, 'H');
    bus.write32(IO_SERIAL_OUT, 'E');
    bus.write32(IO_SERIAL_OUT, 'L');
    bus.write32(IO_SERIAL_OUT, 'L');
    bus.write32(IO_SERIAL_OUT, 'O');

    return 0;
}
