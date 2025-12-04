#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <string>

// Regiões de memória
constexpr uint32_t RAM_INICIO = 0x00000;
constexpr uint32_t RAM_FIM = 0x7FFFF;

constexpr uint32_t VRAM_INICIO = 0x80000;
constexpr uint32_t VRAM_FIM = 0x8FFFF;

constexpr uint32_t EXP_INICIO = 0x90000;
constexpr uint32_t EXP_FIM = 0x9FBFF;

constexpr uint32_t IO_INICIO = 0x9FC00;
constexpr uint32_t IO_FIM = 0x9FFFF;

constexpr uint32_t IO_SAIDA_SERIAL = 0x9FC00;

constexpr uint32_t TAXA_ATUALIZACAO_VRAM = 20;

enum CODIGO_OP : uint32_t
{
    LUI = 0b0110111,
    AUIPC = 0b0010111,
    JAL = 0b1101111,
    JALR = 0b1100111,
    DESVIO = 0b1100011,
    CARREGAR = 0b0000011,
    ARMAZENAR = 0b0100011,
    ALU_IMEDIATO = 0b0010011,
    ALU_REGISTRO = 0b0110011
};

enum FUNCAO3 : uint32_t
{
    SOMA_SUB = 0b000,
    DESL_ESQ = 0b001,
    MENOR_QUE = 0b010,
    MENOR_QUE_U = 0b011,
    XOR_OP = 0b100,
    DESL_DIR = 0b101,
    OR_OP = 0b110,
    AND_OP = 0b111
};

class Barramento
{
public:
    std::vector<uint8_t> memoria;
    uint64_t ciclos = 0;

    Barramento(size_t tamanho_bytes)
    {
        memoria.resize(tamanho_bytes, 0);
    }

    uint32_t ler32(uint32_t endereco)
    {
        if (endereco >= IO_INICIO && endereco <= IO_FIM)
        {
            return ler_periferico(endereco);
        }

        uint32_t b0 = memoria[endereco];
        uint32_t b1 = memoria[endereco + 1];
        uint32_t b2 = memoria[endereco + 2];
        uint32_t b3 = memoria[endereco + 3];

        return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
    }

    void escrever32(uint32_t endereco, uint32_t dado)
    {
        if (endereco >= IO_INICIO && endereco <= IO_FIM)
        {
            escrever_periferico(endereco, dado);
            return;
        }

        memoria[endereco] = dado & 0xFF;
        memoria[endereco + 1] = (dado >> 8) & 0xFF;
        memoria[endereco + 2] = (dado >> 16) & 0xFF;
        memoria[endereco + 3] = (dado >> 24) & 0xFF;
    }

    uint32_t ler_periferico(uint32_t endereco)
    {
        return 0;
    }

    void escrever_periferico(uint32_t endereco, uint32_t dado)
    {
        if (endereco == IO_SAIDA_SERIAL)
        {
            std::cout << "[SERIAL] " << (char)dado << std::endl;
        }
    }

    void despejar_vram()
    {
        std::cout << "\n--- SAÍDA VRAM ---\n";

        for (uint32_t end = VRAM_INICIO; end <= VRAM_FIM; ++end)
        {
            char c = (char)memoria[end];
            if (c == 0)
                std::cout << '.';
            else
                std::cout << c;
        }

        std::cout << "\n------------------\n";
    }
};

class CPU
{
public:
    uint32_t pc = 0;
    uint32_t registradores[32]{};
    Barramento &barramento;
    uint32_t contador_instrucoes = 0;
    bool interrupcao_pendente = false;
    uint32_t mepc = 0;
    uint32_t endereco_rotina_interrupcao = 0x00010000;

    CPU(Barramento &b) : barramento(b) {}

    uint32_t buscar()
    {
        uint32_t inst = barramento.ler32(pc);
        pc += 4;
        return inst;
    }

    void disparar_interrupcao()
    {
        interrupcao_pendente = true;
    }

    struct Decodificada
    {
        uint32_t opcode, rd, rs1, rs2, funct3, funct7;
        int32_t imediato;
    };

    Decodificada decodificar(uint32_t inst)
    {
        Decodificada d{};
        d.opcode = inst & 0x7F;
        d.rd = (inst >> 7) & 0x1F;
        d.funct3 = (inst >> 12) & 7;
        d.rs1 = (inst >> 15) & 0x1F;
        d.rs2 = (inst >> 20) & 0x1F;
        d.funct7 = (inst >> 25) & 0x7F;

        switch (d.opcode)
        {
        case ALU_IMEDIATO:
        case CARREGAR:
        case JALR:
            d.imediato = (int32_t)inst >> 20;
            break;

        case ARMAZENAR:
            d.imediato = ((inst >> 7) & 0x1F) | (((int32_t)inst >> 25) << 5);
            break;

        case DESVIO:
            d.imediato = (((inst >> 7) & 1) << 11) | (((inst >> 8) & 0xF) << 1) | (((inst >> 25) & 0x3F) << 5) | (((inst >> 31) & 1) << 12);
            if (d.imediato & 0x1000)
                d.imediato |= 0xFFFFE000;
            break;

        case LUI:
        case AUIPC:
            d.imediato = (int32_t)(inst & 0xFFFFF000);
            break;

        case JAL:
            d.imediato = (((inst >> 21) & 0x3FF) << 1) | (((inst >> 20) & 1) << 11) | (((inst >> 12) & 0xFF) << 12) | (((inst >> 31) & 1) << 20);
            if (d.imediato & 0x100000)
                d.imediato |= 0xFFE00000;
            break;
        }

        return d;
    }

    void passo()
    {
        if (interrupcao_pendente)
        {
            mepc = pc;
            pc = endereco_rotina_interrupcao;
            interrupcao_pendente = false;
        }

        uint32_t inst = buscar();

        if (inst == 0xFFFFFFFF)
        {
            pc = mepc;
            return;
        }
        Decodificada d = decodificar(inst);

        contador_instrucoes++;
        if (contador_instrucoes % 1000 == 0)
        {
            disparar_interrupcao();
        }

        switch (d.opcode)
        {

        case LUI:
            registradores[d.rd] = d.imediato;
            break;

        case AUIPC:
            registradores[d.rd] = pc + d.imediato - 4;
            break;

        case JAL:
            registradores[d.rd] = pc;
            pc += d.imediato - 4;
            break;

        case JALR:
        {
            uint32_t temp = pc;
            pc = (registradores[d.rs1] + d.imediato) & ~1;
            registradores[d.rd] = temp;
        }
        break;

        case CARREGAR:
        {
            uint32_t end = registradores[d.rs1] + d.imediato;

            uint32_t raw = barramento.ler32(end);

            switch (d.funct3)
            {
            case 0b000:
                registradores[d.rd] = (int8_t)(raw & 0xFF);
                break;

            case 0b001:
                registradores[d.rd] = (int16_t)(raw & 0xFFFF);
                break;

            case 0b010:
                registradores[d.rd] = raw;
                break;

            case 0b100:
                registradores[d.rd] = (uint8_t)(raw & 0xFF);
                break;

            case 0b101:
                registradores[d.rd] = (uint16_t)(raw & 0xFFFF);
                break;
            }
        }
        break;

        case ARMAZENAR:
        {
            uint32_t end = registradores[d.rs1] + d.imediato;

            switch (d.funct3)
            {

            case 0b000:
            {
                uint32_t w = barramento.ler32(end);
                w = (w & ~0xFF) | (registradores[d.rs2] & 0xFF);
                barramento.escrever32(end, w);
            }
            break;

            case 0b001:
            {
                uint32_t w = barramento.ler32(end);
                w = (w & ~0xFFFF) | (registradores[d.rs2] & 0xFFFF);
                barramento.escrever32(end, w);
            }
            break;

            case 0b010:
                barramento.escrever32(end, registradores[d.rs2]);
                break;
            }
        }
        break;

        case DESVIO:

            switch (d.funct3)
            {

            case 0b000:
                if (registradores[d.rs1] == registradores[d.rs2])
                    pc += d.imediato - 4;
                break;

            case 0b001:
                if (registradores[d.rs1] != registradores[d.rs2])
                    pc += d.imediato - 4;
                break;

            case 0b100:
                if ((int32_t)registradores[d.rs1] < (int32_t)registradores[d.rs2])
                    pc += d.imediato - 4;
                break;

            case 0b101:
                if ((int32_t)registradores[d.rs1] >= (int32_t)registradores[d.rs2])
                    pc += d.imediato - 4;
                break;

            case 0b110:
                if (registradores[d.rs1] < registradores[d.rs2])
                    pc += d.imediato - 4;
                break;

            case 0b111:
                if (registradores[d.rs1] >= registradores[d.rs2])
                    pc += d.imediato - 4;
                break;
            }
            break;

        case ALU_IMEDIATO:
            switch (d.funct3)
            {

            case SOMA_SUB:
                registradores[d.rd] = registradores[d.rs1] + d.imediato;
                break;

            case MENOR_QUE:
                registradores[d.rd] = ((int32_t)registradores[d.rs1] < (int32_t)d.imediato);
                break;

            case MENOR_QUE_U:
                registradores[d.rd] = (registradores[d.rs1] < (uint32_t)d.imediato);
                break;

            case XOR_OP:
                registradores[d.rd] = registradores[d.rs1] ^ d.imediato;
                break;

            case OR_OP:
                registradores[d.rd] = registradores[d.rs1] | d.imediato;
                break;

            case AND_OP:
                registradores[d.rd] = registradores[d.rs1] & d.imediato;
                break;

            case DESL_ESQ:
                registradores[d.rd] = registradores[d.rs1] << (d.rs2);
                break;

            case DESL_DIR:
                if (d.funct7 == 0b0000000)
                    registradores[d.rd] = registradores[d.rs1] >> d.rs2;
                else if (d.funct7 == 0b0100000)
                    registradores[d.rd] = ((int32_t)registradores[d.rs1]) >> d.rs2;
                break;
            }
            break;

        case ALU_REGISTRO:

            switch (d.funct3)
            {

            case SOMA_SUB:
                if (d.funct7 == 0b0000000)
                    registradores[d.rd] = registradores[d.rs1] + registradores[d.rs2];

                else if (d.funct7 == 0b0100000)
                    registradores[d.rd] = registradores[d.rs1] - registradores[d.rs2];

                break;

            case DESL_ESQ:
                registradores[d.rd] = registradores[d.rs1] << (registradores[d.rs2] & 0x1F);
                break;

            case MENOR_QUE:
                registradores[d.rd] = ((int32_t)registradores[d.rs1] < (int32_t)registradores[d.rs2]);
                break;

            case MENOR_QUE_U:
                registradores[d.rd] = (registradores[d.rs1] < registradores[d.rs2]);
                break;

            case XOR_OP:
                registradores[d.rd] = registradores[d.rs1] ^ registradores[d.rs2];
                break;

            case DESL_DIR:
                if (d.funct7 == 0b0000000) // SRL
                    registradores[d.rd] = registradores[d.rs1] >> (registradores[d.rs2] & 0x1F);

                else if (d.funct7 == 0b0100000) // SRA
                    registradores[d.rd] = ((int32_t)registradores[d.rs1]) >> (registradores[d.rs2] & 0x1F);

                break;

            case OR_OP:
                registradores[d.rd] = registradores[d.rs1] | registradores[d.rs2];
                break;

            case AND_OP:
                registradores[d.rd] = registradores[d.rs1] & registradores[d.rs2];
                break;
            }
            break;
        }

        registradores[0] = 0;

        if (contador_instrucoes % TAXA_ATUALIZACAO_VRAM == 0)
            barramento.despejar_vram();
    }
};

int main()
{
    Barramento barramento(1024 * 1024);
    CPU cpu(barramento);

    barramento.escrever32(IO_SAIDA_SERIAL, 'H');
    barramento.escrever32(IO_SAIDA_SERIAL, 'E');
    barramento.escrever32(IO_SAIDA_SERIAL, 'L');
    barramento.escrever32(IO_SAIDA_SERIAL, 'L');
    barramento.escrever32(IO_SAIDA_SERIAL, 'O');

    return 0;
}
