#include <iostream>
#include <cstdint>
#include <iomanip>
#include <string>
using namespace std;

static inline int32_t sign_extend(uint32_t val, int bits){
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((val ^ m) - m);
}

static inline uint32_t get_bits(uint32_t v, int hi, int lo){
    return (v >> lo) & ((1u << (hi - lo + 1)) - 1);
}

class Memoria {
public:
    static const uint32_t TAMANHO_TOTAL = 0xA0000;
    uint32_t memoria_dados[TAMANHO_TOTAL / 4];

    Memoria() {
        for (uint32_t i = 0; i < (TAMANHO_TOTAL / 4); i++)
            memoria_dados[i] = 0;
    }

    void escrever32(uint32_t endereco, uint32_t valor){
        if (endereco % 4 != 0) {
            endereco = endereco & ~0x3u;
        }
        uint32_t idx = endereco / 4;
        if (idx < (TAMANHO_TOTAL / 4))
            memoria_dados[idx] = valor;
    }

    uint32_t ler32(uint32_t endereco){
        if (endereco % 4 != 0) {
            endereco = endereco & ~0x3u;
        }
        uint32_t idx = endereco / 4;
        if (idx < (TAMANHO_TOTAL / 4))
            return memoria_dados[idx];
        return 0;
    }

    void mostrar_memoria_info(){
        cout << "\n==================== MEMÓRIA ====================\n";
        cout << "Tamanho total: 640 KB\n";
        cout << "Faixas de endereços:\n";
        cout << " - RAM:   0x00000  até 0x7FFFF\n";
        cout << " - VRAM:  0x80000  até 0x8FFFF\n";
        cout << " - I/O:   0x9FC00  até 0x9FFFF\n";
        cout << "=================================================\n\n";
    }
};

class Barramento {
private:
    uint32_t barramento_dados;
    uint32_t barramento_enderecos;
    uint8_t barramento_controle;
    
    Memoria* memoria;
    
public:
    enum Controle {
        IDLE = 0x00,
        READ = 0x01,
        WRITE = 0x02,
        IO = 0x04
    };
    
    Barramento(Memoria* mem) : memoria(mem), 
                                barramento_dados(0), 
                                barramento_enderecos(0), 
                                barramento_controle(IDLE) {
        cout << "Barramento inicializado:\n";
        cout << " - Barramento de Dados: 32 bits\n";
        cout << " - Barramento de Endereços: 32 bits\n";
        cout << " - Barramento de Controle: READ, WRITE, IO\n";
    }
    
    uint32_t ler(uint32_t endereco) {
        barramento_enderecos = endereco;
        
        barramento_controle = READ;
        
        barramento_dados = memoria->ler32(endereco);
        
        uint32_t dado = barramento_dados;
        
        barramento_controle = IDLE;
        
        return dado;
    }
    
    void escrever(uint32_t endereco, uint32_t valor) {
        barramento_enderecos = endereco;
        
        barramento_dados = valor;
        
        barramento_controle = WRITE;
        
        memoria->escrever32(endereco, valor);
        
        barramento_controle = IDLE;
    }
    
    void mostrar_estado() {
        cout << "\n========== ESTADO DO BARRAMENTO ==========\n";
        cout << "Barramento de Endereços: 0x" << hex << setw(8) << setfill('0') 
             << barramento_enderecos << dec << "\n";
        cout << "Barramento de Dados:     0x" << hex << setw(8) << setfill('0') 
             << barramento_dados << dec << "\n";
        cout << "Barramento de Controle:  ";
        
        if (barramento_controle == IDLE) cout << "IDLE";
        else {
            if (barramento_controle & READ) cout << "READ ";
            if (barramento_controle & WRITE) cout << "WRITE ";
            if (barramento_controle & IO) cout << "IO ";
        }
        cout << "\n==========================================\n\n";
    }
    
    uint32_t get_dados() const { return barramento_dados; }
    uint32_t get_endereco() const { return barramento_enderecos; }
    uint8_t get_controle() const { return barramento_controle; }
};

class DispositivoES {
private:
    Memoria* memoria;
    static const uint32_t VRAM_INICIO = 0x80000;
    static const uint32_t VRAM_FIM = 0x8FFFF;
    
public:
    DispositivoES(Memoria* mem) : memoria(mem) {}
    
    void exibir_vram() {
        cout << "\n╔════════════════════════════════════════════════════════╗\n";
        cout << "║           SAÍDA DE VÍDEO (VRAM - E/S)                  ║\n";
        cout << "╚════════════════════════════════════════════════════════╝\n";
        cout << "Endereço 0x80000 - 0x8FFFF:\n";
        cout << "┌────────────────────────────────────────────────────────┐\n│ ";
        
        int caracteres_linha = 0;
        bool tem_conteudo = false;
        
        for (uint32_t addr = VRAM_INICIO; addr <= VRAM_FIM; addr += 4) {
            uint32_t word = memoria->ler32(addr);
            
            for (int i = 0; i < 4; i++) {
                uint8_t byte = (word >> (i * 8)) & 0xFF;
                
                if (byte != 0) {
                    tem_conteudo = true;
                    
                    if (byte >= 32 && byte <= 126) {
                        cout << (char)byte;
                    } else if (byte == 10) {
                        cout << "\n│ ";
                        caracteres_linha = 0;
                        continue;
                    } else {
                        cout << '.';
                    }
                    
                    caracteres_linha++;
                    
                    if (caracteres_linha >= 54) {
                        cout << "\n│ ";
                        caracteres_linha = 0;
                    }
                }
            }
        }
        
        if (!tem_conteudo) {
            cout << "[VRAM vazia - sem conteúdo para exibir]";
        }
        
        cout << "\n└────────────────────────────────────────────────────────┘\n\n";
    }
    
    bool eh_endereco_vram(uint32_t endereco) {
        return (endereco >= VRAM_INICIO && endereco <= VRAM_FIM);
    }
};

class CPU {
public:
    int32_t regs[32] = {0};
    uint32_t pc = 0;
    Barramento* barramento;
    uint32_t contador_instrucoes = 0;

    CPU(Barramento* bus) : barramento(bus) {
        regs[0] = 0;
    }

    void executar(uint32_t inst) {
        contador_instrucoes++;
        uint32_t opcode = inst & 0x7F;

        switch (opcode) {

        // ---------------- R ----------------
        case 0x33: {
            uint32_t rd     = get_bits(inst,11,7);
            uint32_t funct3 = get_bits(inst,14,12);
            uint32_t rs1    = get_bits(inst,19,15);
            uint32_t rs2    = get_bits(inst,24,20);
            uint32_t funct7 = get_bits(inst,31,25);

        switch (funct3) {
            case 0x0:
                if (funct7 == 0x00) {
                    regs[rd] = regs[rs1] + regs[rs2];
                    cout << "ADD x" << rd << " = x" << rs1 << " + x" << rs2 << "\n";
                } else if (funct7 == 0x20) {
                    regs[rd] = regs[rs1] - regs[rs2];
                    cout << "SUB x" << rd << " = x" << rs1 << " - x" << rs2 << "\n";
                }
                break;

            case 0x1:
                regs[rd] = (int32_t)((uint32_t)regs[rs1] << (regs[rs2] & 0x1F));
                cout << "SLL x" << rd << " = x" << rs1 << " << x" << rs2 << "\n";
                break;

            case 0x5:
                if (funct7 == 0x00) {
                    regs[rd] = (int32_t)((uint32_t)regs[rs1] >> (regs[rs2] & 0x1F));
                    cout << "SRL x" << rd << " = x" << rs1 << " >>u x" << rs2 << "\n";
                } else if (funct7 == 0x20) {
                    regs[rd] = regs[rs1] >> (regs[rs2] & 0x1F);
                    cout << "SRA x" << rd << " = x" << rs1 << " >>s x" << rs2 << "\n";
                }
                break;

            case 0x6:
                regs[rd] = regs[rs1] | regs[rs2];
                cout << "OR x" << rd << " = x" << rs1 << " | x" << rs2 << "\n";
                break;

            case 0x7:
                regs[rd] = regs[rs1] & regs[rs2];
                cout << "AND x" << rd << " = x" << rs1 << " & x" << rs2 << "\n";
                break;

            case 0x4:
                regs[rd] = regs[rs1] ^ regs[rs2];
                cout << "XOR x" << rd << " = x" << rs1 << " ^ x" << rs2 << "\n";
                break;

            case 0x2:
                regs[rd] = (regs[rs1] < regs[rs2]) ? 1 : 0;
                cout << "SLT x" << rd << " = (x" << rs1 << " < x" << rs2 << ")\n";
                break;

            case 0x3:
                regs[rd] = ((uint32_t)regs[rs1] < (uint32_t)regs[rs2]) ? 1 : 0;
                cout << "SLTU x" << rd << " = (ux" << rs1 << " < ux" << rs2 << ")\n";
                break;

            default:
                cout << "R-type funct3 não implementado: " << funct3 << "\n";
            }
        }
        break;

        case 0x13: {
            uint32_t rd     = get_bits(inst,11,7);
            uint32_t funct3 = get_bits(inst,14,12);
            uint32_t rs1    = get_bits(inst,19,15);
            int32_t imm     = sign_extend(get_bits(inst,31,20), 12);

            switch (funct3) {
            case 0x0:
                regs[rd] = regs[rs1] + imm;
                cout << "ADDI x" << rd << " = x" << rs1 << " + " << imm << "\n";
                break;

            case 0x6:
                regs[rd] = regs[rs1] | imm;
                cout << "ORI x" << rd << " = x" << rs1 << " | " << imm << "\n";
                break;

            case 0x7:
                regs[rd] = regs[rs1] & imm;
                cout << "ANDI x" << rd << " = x" << rs1 << " & " << imm << "\n";
                break;

            case 0x1: {
                uint32_t sh = get_bits(inst,24,20);
                regs[rd] = (int32_t)((uint32_t)regs[rs1] << sh);
                cout << "SLLI x" << rd << " = x" << rs1 << " << " << sh << "\n";
                break;
            }

            case 0x5: {
                uint32_t sh = get_bits(inst,24,20);
                uint32_t funct7 = get_bits(inst,31,25);

                if (funct7 == 0x00) {
                    regs[rd] = (int32_t)((uint32_t)regs[rs1] >> sh);
                    cout << "SRLI x" << rd << " = x" << rs1 << " >>u " << sh << "\n";
                } else {
                    regs[rd] = regs[rs1] >> sh;
                    cout << "SRAI x" << rd << " = x" << rs1 << " >>s " << sh << "\n";
                }
                break;
            }

            default:
                cout << "I-type funct3 não implementado: " << funct3 << "\n";
            }
        }
        break;

        case 0x63: {
            uint32_t funct3 = get_bits(inst,14,12);
            uint32_t rs1 = get_bits(inst,19,15);
            uint32_t rs2 = get_bits(inst,24,20);

            uint32_t imm = (get_bits(inst,31,31) << 12)
                         | (get_bits(inst,7,7) << 11)
                         | (get_bits(inst,30,25) << 5)
                         | (get_bits(inst,11,8) << 1);

            int32_t soff = sign_extend(imm, 13);
            bool take = false;

            switch (funct3) {
            case 0x0: take = (regs[rs1] == regs[rs2]); cout << "BEQ\n"; break;
            case 0x1: take = (regs[rs1] != regs[rs2]); cout << "BNE\n"; break;
            case 0x4: take = (regs[rs1] < regs[rs2]);  cout << "BLT\n"; break;
            case 0x5: take = (regs[rs1] >= regs[rs2]); cout << "BGE\n"; break;
            case 0x6: take = ((uint32_t)regs[rs1] <  (uint32_t)regs[rs2]); cout << "BLTU\n"; break;
            case 0x7: take = ((uint32_t)regs[rs1] >= (uint32_t)regs[rs2]); cout << "BGEU\n"; break;
            default:
                cout << "Branch funct3 desconhecido.\n";
            }

            if (take) {
                pc = (int32_t)pc + soff;
                cout << "Branch taken -> pc = 0x" << hex << pc << dec << "\n";
                regs[0] = 0;
                return;
            }
        }
        break;

        case 0x6F: {
            uint32_t rd = get_bits(inst,11,7);
            uint32_t imm = (get_bits(inst,31,31) << 20)
                         | (get_bits(inst,19,12) << 12)
                         | (get_bits(inst,20,20) << 11)
                         | (get_bits(inst,30,21) << 1);

            int32_t soff = sign_extend(imm, 21);

            regs[rd] = pc + 4;
            pc = (uint32_t)((int32_t)pc + soff);
            cout << "JAL x" << rd << " -> pc = 0x" << hex << pc << dec << "\n";
            regs[0] = 0;
            return;
        }
        break;

        case 0x37: {
            uint32_t rd = get_bits(inst,11,7);
            uint32_t imm20 = get_bits(inst,31,12);
            int32_t val = (int32_t)(imm20 << 12);
            regs[rd] = val;
            cout << "LUI x" << rd << " = 0x" << hex << (uint32_t)val << dec << "\n";
        }
        break;

        case 0x17: {
            uint32_t rd = get_bits(inst,11,7);
            uint32_t imm20 = get_bits(inst,31,12);
            uint32_t val = imm20 << 12;
            regs[rd] = (int32_t)(pc + val);
            cout << "AUIPC x" << rd << " = pc + 0x" << hex << val << dec << "\n";
        }
        break;
        
        case 0x03: {
            uint32_t rd = get_bits(inst, 11, 7);
            uint32_t funct3 = get_bits(inst, 14, 12);
            uint32_t rs1 = get_bits(inst, 19, 15);
            int32_t imm = sign_extend(get_bits(inst, 31, 20), 12);
            
            uint32_t endereco = (uint32_t)((int32_t)regs[rs1] + imm);
            
            if (funct3 == 0x2) {
                regs[rd] = (int32_t)barramento->ler(endereco);
                cout << "LW x" << rd << " = MEM[x" << rs1 << " + " << imm 
                     << "] = MEM[0x" << hex << endereco << "] = 0x" 
                     << (uint32_t)regs[rd] << dec << "\n";
            }
        }
        break;
        
        case 0x23: {
            uint32_t funct3 = get_bits(inst, 14, 12);
            uint32_t rs1 = get_bits(inst, 19, 15);
            uint32_t rs2 = get_bits(inst, 24, 20);
            
            uint32_t imm = (get_bits(inst, 31, 25) << 5) | get_bits(inst, 11, 7);
            int32_t offset = sign_extend(imm, 12);
            
            uint32_t endereco = (uint32_t)((int32_t)regs[rs1] + offset);
            
            if (funct3 == 0x2) {
                barramento->escrever(endereco, (uint32_t)regs[rs2]);
                cout << "SW MEM[x" << rs1 << " + " << offset 
                     << "] = MEM[0x" << hex << endereco << "] = x" << dec << rs2 
                     << " (0x" << hex << (uint32_t)regs[rs2] << dec << ")\n";
            }
        }
        break;

        default:
            cout << "Opcode não implementado!\n";
        }

        regs[0] = 0;
        pc += 4;
    }
};

bool test_memoria_basica(Barramento& bus) {
    cout << "\n[Teste] Memória básica (escrita/leitura 32-bit)\n";
    uint32_t addr = 0x00010;
    uint32_t valor = 0xDEADBEEF;
    bus.escrever(addr, valor);
    uint32_t lido = bus.ler(addr);
    if (lido == valor) {
        cout << "PASS: valor escrito 0x" << hex << valor << " lido 0x" << lido << dec << "\n";
        return true;
    } else {
        cout << "FAIL: esperado 0x" << hex << valor << " lido 0x" << lido << dec << "\n";
        return false;
    }
}

bool test_vram_e_exibicao(Barramento& bus, DispositivoES& dev) {
    cout << "\n[Teste] VRAM e exibição (escrever string e exibir)\n";
    const string s = "TEST-VRAM\n";
    uint32_t base = 0x80000; // VRAM inicio
    // Escrever cada 4 bytes como uma word (simples)
    for (size_t i = 0; i < s.size(); i += 4) {
        uint32_t w = 0;
        for (int b = 0; b < 4; ++b) {
            size_t idx = i + b;
            uint8_t ch = (idx < s.size()) ? static_cast<uint8_t>(s[idx]) : 0;
            w |= (uint32_t)ch << (b * 8);
        }
        bus.escrever(base + static_cast<uint32_t>(i), w);
    }
    // Ler de volta e verificar
    bool ok = true;
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t word = bus.ler(base + static_cast<uint32_t>(i & ~3u));
        uint8_t ch = (word >> ((i & 3) * 8)) & 0xFF;
        if (ch != static_cast<uint8_t>(s[i])) {
            ok = false;
            cout << "FAIL: VRAM mismatch at offset " << i << " expected '" << s[i] << "' got '" << (char)ch << "'\n";
            break;
        }
    }
    if (ok) {
        cout << "PASS: VRAM escrita corretamente. Exibindo VRAM:\n";
        dev.exibir_vram();
    }
    return ok;
}

bool test_cpu_load_store(Barramento& bus, CPU& cpu) {
    cout << "\n[Teste] CPU Load/Store (LW/SW)\n";
    uint32_t addr = 0x0000;
    uint32_t imm1 = 0x100 & 0xFFF;
    uint32_t addi_x1 = (imm1 << 20) | (0 << 15) | (0 << 12) | (1 << 7) | 0x13;
    bus.escrever(addr, addi_x1); addr += 4;
    uint32_t addi_x2 = (0x42 << 20) | (0 << 15) | (0 << 12) | (2 << 7) | 0x13;
    bus.escrever(addr, addi_x2); addr += 4;
    uint32_t sw = (0 << 25) | (2 << 20) | (1 << 15) | (0x2 << 12) | (0 << 7) | 0x23;
    bus.escrever(addr, sw); addr += 4;
    uint32_t lw = (0 << 20) | (1 << 15) | (0x2 << 12) | (3 << 7) | 0x03;
    bus.escrever(addr, lw); addr += 4;
    uint32_t jal_loop = 0x0000006F;
    bus.escrever(addr, jal_loop); addr += 4;

    cpu.pc = 0x0000;
    for (int i = 0; i < 32; ++i) cpu.regs[i] = 0;

    int steps = 0;
    bool ok = true;
    while (steps < 10) {
        uint32_t instr = bus.ler(cpu.pc);
        if (instr == 0x0000006F) break;
        cpu.executar(instr);
        steps++;
    }
    if (cpu.regs[3] == 0x42) {
        cout << "PASS: LW/SW funcionaram. x3 = 0x" << hex << cpu.regs[3] << dec << "\n";
    } else {
        cout << "FAIL: LW/SW falharam. x3 = 0x" << hex << cpu.regs[3] << dec << " (esperado 0x42)\n";
        ok = false;
    }
    return ok;
}

void rodar_testes(Barramento& bus, DispositivoES& dev, CPU& cpu) {
    cout << "\n================ INICIANDO TESTES AUTOMATIZADOS ================\n";
    int total = 0, passed = 0;
    total++; if (test_memoria_basica(bus)) passed++;
    total++; if (test_vram_e_exibicao(bus, dev)) passed++;
    total++; if (test_cpu_load_store(bus, cpu)) passed++;

    cout << "\n================ RESULTADO DOS TESTES ================\n";
    cout << "Total: " << total << "  Passaram: " << passed << "  Falharam: " << (total - passed) << "\n";
    cout << "=====================================================\n\n";
}

void carregar_programa_completo(Barramento& barramento) {
    cout << "\n========== CARREGANDO PROGRAMA DE TESTE COMPLETO ==========\n";
    cout << "Programa: Demonstração de instruções e escrita em VRAM\n";
    cout << "===========================================================\n\n";
    
    uint32_t addr = 0x00000;
    
    // x10 (a0) = 5 (número para calcular fatorial) -> ADDI x10,x0,5
    barramento.escrever(addr, 0x00500513); addr += 4;
    // x11 (a1) = 1 -> ADDI x11,x0,1
    barramento.escrever(addr, 0x00100593); addr += 4;
    // x12 (a2) = 1 -> ADDI x12,x0,1
    barramento.escrever(addr, 0x00100613); addr += 4;
    // BLT x10,x12, +16 -> 0x00C54863
    barramento.escrever(addr, 0x00C54863); addr += 4;
    // ADD x11,x11,x12
    barramento.escrever(addr, 0x00C585B3); addr += 4;
    // ADDI x12,x12,1
    barramento.escrever(addr, 0x00160613); addr += 4;
    // JAL x0, -16 -> loop
    barramento.escrever(addr, 0xFF1FF06F); addr += 4;
    // LUI x13,0x80
    barramento.escrever(addr, 0x000806B7); addr += 4;
    // ADDI x14,x0,'F'
    barramento.escrever(addr, 0x04600713); addr += 4;
    // SW x14,0(x13)
    barramento.escrever(addr, 0x00E6A023); addr += 4;
    // ADDI x14,x0,'A'
    barramento.escrever(addr, 0x04100713); addr += 4;
    // SW x14,4(x13)
    barramento.escrever(addr, 0x00E6A223); addr += 4;
    // ADDI x14,x0,'T'
    barramento.escrever(addr, 0x05400713); addr += 4;
    // SW x14,8(x13)
    barramento.escrever(addr, 0x00E6A423); addr += 4;
    // ADDI x14,x0,'='
    barramento.escrever(addr, 0x03D00713); addr += 4;
    // SW x14,12(x13)
    barramento.escrever(addr, 0x00E6A623); addr += 4;
    // ADDI x14,x0,' '
    barramento.escrever(addr, 0x02000713); addr += 4;
    // SW x14,16(x13)
    barramento.escrever(addr, 0x00E6A823); addr += 4;
    // ADDI x15,x11,0 (copiar resultado)
    barramento.escrever(addr, 0x00058793); addr += 4;
    // ADDI x14,x0,'1'
    barramento.escrever(addr, 0x03100713); addr += 4;
    // SW x14,20(x13)
    barramento.escrever(addr, 0x00E6AA23); addr += 4;
    // JAL x0,0 (loop infinito)
    barramento.escrever(addr, 0x0000006F); addr += 4;
    
    cout << "Programa carregado: " << (addr/4) << " instruções\n";
    cout << "Tamanho: " << addr << " bytes\n\n";
}

// =======================================================
// MAIN
// =======================================================
int main(){
    Memoria memoria;
    Barramento barramento(&memoria);
    CPU cpu(&barramento);
    DispositivoES dispositivo_es(&memoria);
    
    const int INSTRUCOES_POR_ES = 10;  // Exibir VRAM a cada 10 instruções
    const int MAX_INSTRUCOES = 200;     // Limite de segurança

    cout << "_____________________________________________________________\n";
    cout << "          SIMULADOR DE COMPUTADOR RISC-V 32-bit              \n";
    cout << "                    Arquitetura RV32I                        \n";
    cout << "_____________________________________________________________\n\n";

    memoria.mostrar_memoria_info();

    cout << "======================= CPU ============================\n";
    cout << "Registradores: 32 x 32-bit (x0-x31)\n";
    cout << "PC inicial: 0x00000000\n";
    cout << "Instruções implementadas:\n";
    cout << " • Tipo R: ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU\n";
    cout << " • Tipo I: ADDI, ANDI, ORI, SLLI, SRLI, SRAI, LW\n";
    cout << " • Tipo S: SW\n";
    cout << " • Tipo B: BEQ, BNE, BLT, BGE, BLTU, BGEU\n";
    cout << " • Tipo U: LUI, AUIPC\n";
    cout << " • Tipo J: JAL\n";
    cout << "========================================================\n\n";

    // Carregar programa de teste (instruções)
    carregar_programa_completo(barramento);

    // Rodar testes automáticos antes da execução principal
    rodar_testes(barramento, dispositivo_es, cpu);

    cout << "=============== INICIANDO EXECUÇÃO ===============\n";
    cout << "Configuração de E/S: Exibir VRAM a cada " 
         << INSTRUCOES_POR_ES << " instruções\n";
    cout << "Limite de segurança: " << MAX_INSTRUCOES << " instruções\n\n";

    // Loop de execução
    bool executando = true;
    int instrucoes_executadas = 0;

    while (executando && instrucoes_executadas < MAX_INSTRUCOES) {
        uint32_t instr = barramento.ler(cpu.pc);
        
        // Detectar loop infinito (JAL x0, 0)
        if (instr == 0x0000006F) {
            cout << "\n[STOP] Loop infinito detectado - encerrando execução.\n";
            break;
        }
        
        cout << "\n─────────────────────────────────────────────────────\n";
        cout << "Instrução #" << (instrucoes_executadas + 1) << "\n";
        cout << "PC: 0x" << hex << setw(8) << setfill('0') << cpu.pc << dec;
        cout << " | Opcode: 0x" << hex << setw(8) << setfill('0') << instr << dec << "\n";
        
        cpu.executar(instr);
        instrucoes_executadas++;
        
        // E/S PROGRAMADA: Exibe VRAM periodicamente
        if (cpu.contador_instrucoes % INSTRUCOES_POR_ES == 0) {
            cout << "\n>>> INTERRUPÇÃO DE E/S (a cada " << INSTRUCOES_POR_ES << " instruções) <<<\n";
            dispositivo_es.exibir_vram();
        }
    }

    // Exibição final
    cout << "\n\n";
    cout << "_____________________________________________________________\n";
    cout << "                    EXECUÇÃO FINALIZADA                      \n";
    cout << "_____________________________________________________________\n\n";

    cout << "============= ESTADO FINAL DA VRAM =============\n";
    dispositivo_es.exibir_vram();

    cout << "================ ESTADO FINAL DA CPU ================\n";
    cout << "Registradores (apenas não-zero):\n";
    for (int i = 0; i < 32; i++) {
        if (cpu.regs[i] != 0) {
            cout << "  x" << setw(2) << i << " (";            
            if (i == 10) cout << "a0";
            else if (i == 11) cout << "a1";
            else if (i == 12) cout << "a2";
            else if (i == 13) cout << "a3";
            else if (i == 14) cout << "a4";
            else if (i == 15) cout << "a5";
            else cout << "  ";
            
            cout << "): " << setw(10) << cpu.regs[i] 
                 << " (0x" << hex << setw(8) << setfill('0') 
                 << (uint32_t)cpu.regs[i] << dec << ")\n";
        }
    }
    
    cout << "\nPC final: 0x" << hex << setw(8) << setfill('0') 
         << cpu.pc << dec << "\n";
    cout << "Total de instruções executadas: " << instrucoes_executadas << "\n";
    cout << "====================================================\n\n";

    // Estatísticas
    cout << "================ ESTATÍSTICAS DO SISTEMA ================\n";
    cout << "Operações de memória realizadas via barramento\n";
    cout << "VRAM utilizada para saída de caracteres ASCII\n";
    cout << "E/S programada com polling a cada " << INSTRUCOES_POR_ES << " instruções\n";
    cout << "=========================================================\n";

    return 0;
}
