//   File: Simulator.h
// Author: Grant Clark
//   Date: 12/17/2023

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "Common.h"
#include "RegisterFile.h"

const unsigned int TEXT_SEGMENT_SIZE = 1000000;
const unsigned int DATA_SEGMENT_SIZE = 1000000;
const unsigned int MAX_HEAP = 1000000; // Grows upwards.
const unsigned int MAX_STACK = 1000000; // Grows downwards.

const unsigned int TOTAL_INSTRUCTIONS = 53;

// FN means it is a function, so it gets the R (register) encoding scheme
// OP means an operation, so it has an opcode and gets the I (immediate) encoding scheme.
// Some OP instructions get the jump encoding scheme if they are jumps.
// Some FN instructions have their own special encodings like syscall or mtlo.
enum Instruction
{
    ADD = 0,  // FN, R
    ADDI,     // OP, I
    ADDIU,    // OP, I
    ADDU,     // FN, R
    AND,      // FN, R
    ANDI,     // OP, I
    BEQ,      // OP, I
    BNE,      // OP, I
    J,        // OP, J
    JAL,      // OP, J
    JR,       // FN, R
    LBU,      // OP, I
    LHU,      // OP, I
    LUI,      // OP, I
    LW,       // OP, I (PSEUDOINSTRUCTION WHEN SECOND ARGUMENT IS A LABEL!!!)
    NOR,      // FN, R
    OR,       // FN, R
    ORI,      // OP, I
    SLT,      // FN, R
    SLTI,     // OP, I
    SLTIU,    // OP, I
    SLTU,     // FN, R
    SLL,      // FN, R
    SRL,      // FN, R
    SB,       // OP, I
    SC,       // OP, I
    SH,       // OP, I
    SW,       // OP, I
    SUB,      // FN, R
    SUBU,     // FN, R
    DIV,      // FN, R
    DIVU,     // FN, R
    MFHI,     // FN, R
    MFLO,     // FN, R
    MULT,     // FN, R
    MULTU,    // FN, R
    SRA,      // FN, R
    SLLV,     // FN, R
    SRAV,     // FN, R
    SRLV,     // FN, R
    XOR,      // FN, R
    XORI,     // OP, I
    BGTZ,     // OP, I
    BLEZ,     // OP, I
    JALR,     // FN, SPECIAL
    LB,       // OP, I
    LH,       // OP, I
    MTHI,     // FN, SPECIAL
    MTLO,     // FN, SPECIAL
    SYSCALL,  // FN, SPECIAL
    SEQ,
    BGEZ,
    BLTZ,

    // PSEUDOINSTRUCTIONS
    MOVE,
    LI,
    // LOAD WORD WITH LABEL
    LA,
    BLT,
    BLE,
    BGT,
    BGE
};


// Structure used to keep track of instructions
// and where they are in memory.
struct InputAddressPair
{
    std::string input;
    uint32_t address;
};

// Structure to keep track of a list of instructions
// to be encoded, or data to be put into the data segment.
struct StrsAddressSegmentLine
{
    std::vector< std::string > strs;
    uint32_t address;
    bool text;
    unsigned int line;
};

// Simple enum to keep track of which segment you are in.
enum MipsSegment
{
    NONE, // Not in a segment, awaiting segment change.
    TEXT, // In the text segment.
    DATA  // In the data segment.
};

class Simulator
{
public:
    Simulator() :
        running(false),
        current_segment(NONE),
        text_seg_addr(0x00040000),
        data_seg_addr(0x10010000),
        sim_currently_pseudo(false),
        entrypoint_label(""),
        entrypoint_addr(0),
        ins_executions{ // Method pointer array initialization.
        &Simulator::ins_add,
        &Simulator::ins_addi,
        &Simulator::ins_addiu,
        &Simulator::ins_addu,
        &Simulator::ins_and,
        &Simulator::ins_andi,
        &Simulator::ins_beq,
        &Simulator::ins_bne,
        &Simulator::ins_j,
        &Simulator::ins_jal,
        &Simulator::ins_jr,
        &Simulator::ins_lbu,
        &Simulator::ins_lhu,
        &Simulator::ins_lui,
        &Simulator::ins_lw,
        &Simulator::ins_nor,
        &Simulator::ins_or,
        &Simulator::ins_ori,
        &Simulator::ins_slt,
        &Simulator::ins_slti,
        &Simulator::ins_sltiu,
        &Simulator::ins_sltu,
        &Simulator::ins_sll,
        &Simulator::ins_srl,
        &Simulator::ins_sb,
        &Simulator::ins_sc,
        &Simulator::ins_sh,
        &Simulator::ins_sw,
        &Simulator::ins_sub,
        &Simulator::ins_subu,
        &Simulator::ins_div,
        &Simulator::ins_divu,
        &Simulator::ins_mfhi,
        &Simulator::ins_mflo,
        &Simulator::ins_mult,
        &Simulator::ins_multu,
        &Simulator::ins_sra,
        &Simulator::ins_sllv,
        &Simulator::ins_srav,
        &Simulator::ins_srlv,
        &Simulator::ins_xor,
        &Simulator::ins_xori,
        &Simulator::ins_bgtz,
        &Simulator::ins_blez,
        &Simulator::ins_jalr,
        &Simulator::ins_lb,
        &Simulator::ins_lh,
        &Simulator::ins_mthi,
        &Simulator::ins_mtlo,
        &Simulator::ins_syscall,
        &Simulator::ins_seq,
        &Simulator::ins_bgez,
        &Simulator::ins_bltz,
        }
    {
        // Init the stack pointer to be at the end of the stack.
        regs[29] = STACK_END - 1;
    }

    ~Simulator()
    {}

    // Function called by main to run the MIPS simulation.
    void run();

private:
    ///////////////////////////////
    //////////  OBJECTS  //////////
    ///////////////////////////////

    bool sim_mode;
    bool running;
    RegisterFile regs;
    MipsSegment current_segment;
    bool sim_currently_pseudo;

    const uint32_t TEXT_START = 0x00040000;
    uint32_t text[TEXT_SEGMENT_SIZE];
    
    // First address is 0x10010000.
    const uint32_t DATA_START = 0x10010000;
    uint8_t data[DATA_SEGMENT_SIZE];

    // First address is at 0x10040000 (goes up)
    const uint32_t HEAP_START = 0x10040000;
    uint32_t heap_ptr = HEAP_START;
    uint8_t heap[MAX_HEAP];

    // First address is at 0x7ffffe00 (goes down)
    const uint32_t STACK_END = 0x7ffffe00;
    const uint32_t STACK_START = STACK_END - MAX_STACK;
    uint8_t stack[MAX_STACK];

    // Addresses to be used during computation.
    uint32_t text_seg_addr;
    uint32_t data_seg_addr;
    uint32_t program_counter;
    std::string entrypoint_label;
    uint32_t entrypoint_addr;

    std::unordered_map< std::string, uint32_t > labels;
    
    // This vector of instructions and encodings
    // is stored in the order they are addressed in,
    // starting with 0x00400000 and going up from there.
    // Every instruction is of size 0x00000004.
    std::vector< InputAddressPair > input_addr;

    // A vector or instructions to be encoded as well as data
    // to be put into the data segment after finding the
    // values of the input labels. Used by read file mode.
    std::vector< StrsAddressSegmentLine > to_be_encoded;

    // valid inputs to save to file in interpreter mode.
    std::vector< std::string > valid_sim_inputs;

    // An array of all of the members to execute certain
    // instructions. Defined in constructor.
    void (Simulator::*ins_executions[TOTAL_INSTRUCTIONS])(const uint32_t &);
        
    ///////////////////////////////
    ////////// FUNCTIONS //////////
    ///////////////////////////////
    
    // Interpreter mode functions.
    void run_simulator_mode();
    bool valid_label(const std::string & s) const; // shared
    void interpret_input(std::string input);
    std::string uint_to_reg(const unsigned int) const; // shared
    std::string char_value_str(const unsigned int val) const; // shared
    void print_simulator_help() const;
    void print_registers() const;
    void print_labels() const;
    void print_data_segment() const;

    // Read mode functions.
    void run_read_mode();
    void run_file(std::ifstream & file);
    void prepare_read_input(std::string input, unsigned int line);
    void read_handle_pseudo(const Instruction & ins,
                            const std::vector< std::string > & s,
                            const uint32_t & addr,
                            const unsigned int line,
                            std::unordered_map< uint32_t, unsigned int > & line_numbers);
    void read_format_ascii_escape_sequences(std::string & s) const;
    
    // Shared functions.
    inline
    std::string mips_segment_str(MipsSegment mips_segment)
    {
        switch (mips_segment)
        {
            case TEXT:
                return "TEXT";
            case DATA:
                return "DATA";
        }

        return "";
    }

    std::string truncate_comments(std::string s) const;
    std::string strip_useless_whitespace(std::string s) const;
    std::string get_numerical_register_str(std::string s) const;
    std::string replace_registers(std::string s) const;

    // "4($31)" --> { "$31", "4" }.
    std::vector< std::string > split_immediate_and_register(const std::string & s) const;
    std::vector< std::string > split_input(const std::string & s) const;

    // "0x42" --> "66"
    // "0b1001" --> "9"
    // "'\n'" --> "10"
    void format_immediates(std::vector< std::string > & strs) const;
    
    // Replace labels with their numerical values for computation.
    void replace_labels(std::vector< std::string > & strs) const;

    // Adding values to data segment.
    void add_to_data_segment(const std::vector< std::string > & strs);

    // Pseudoinstruction handling
    Instruction is_pseudo(const std::string & s, const std::string & label) const;
    void handle_pseudo(const Instruction & s, const std::vector< std::string > & strs);
    
    // Encoding
    Instruction get_instruction(const std::string & s) const;
    uint8_t get_opcode(const Instruction & instruction) const;
    uint32_t encode(const std::vector< std::string > & args) const;

    // Decoding and Execution
    Instruction get_instruction(const uint8_t target, const bool is_funct) const;
    void execute(const uint32_t & encoded);

    // Function for determining the locations of store and load instructions.
    // Also will give the starting address of that segment for computation.
    void get_location_and_start(const uint32_t & addr,
                                uint8_t * & location,
                                unsigned int & start);
    
    // Individual instruction executions
    void ins_add(const uint32_t & encoded);
    void ins_addi(const uint32_t & encoded);
    void ins_addiu(const uint32_t & encoded);
    void ins_addu(const uint32_t & encoded);
    void ins_and(const uint32_t & encoded);
    void ins_andi(const uint32_t & encoded);
    void ins_beq(const uint32_t & encoded);
    void ins_bne(const uint32_t & encoded);
    void ins_j(const uint32_t & encoded);
    void ins_jal(const uint32_t & encoded);
    void ins_jr(const uint32_t & encoded);
    void ins_lbu(const uint32_t & encoded);
    void ins_lhu(const uint32_t & encoded);
    void ins_lui(const uint32_t & encoded);
    void ins_lw(const uint32_t & encoded);
    void ins_nor(const uint32_t & encoded);
    void ins_or(const uint32_t & encoded);
    void ins_ori(const uint32_t & encoded);
    void ins_slt(const uint32_t & encoded);
    void ins_slti(const uint32_t & encoded);
    void ins_sltiu(const uint32_t & encoded);
    void ins_sltu(const uint32_t & encoded);
    void ins_sll(const uint32_t & encoded);
    void ins_srl(const uint32_t & encoded);
    void ins_sb(const uint32_t & encoded);
    void ins_sc(const uint32_t & encoded);
    void ins_sh(const uint32_t & encoded);
    void ins_sw(const uint32_t & encoded);
    void ins_sub(const uint32_t & encoded);
    void ins_subu(const uint32_t & encoded);
    void ins_div(const uint32_t & encoded);
    void ins_divu(const uint32_t & encoded);
    void ins_mfhi(const uint32_t & encoded);
    void ins_mflo(const uint32_t & encoded);
    void ins_mult(const uint32_t & encoded);
    void ins_multu(const uint32_t & encoded);
    void ins_sra(const uint32_t & encoded);
    void ins_sllv(const uint32_t & encoded);
    void ins_srav(const uint32_t & encoded);
    void ins_srlv(const uint32_t & encoded);
    void ins_xor(const uint32_t & encoded);
    void ins_xori(const uint32_t & encoded);
    void ins_bgtz(const uint32_t & encoded);
    void ins_blez(const uint32_t & encoded);
    void ins_jalr(const uint32_t & encoded);
    void ins_lb(const uint32_t & encoded);
    void ins_lh(const uint32_t & encoded);
    void ins_mthi(const uint32_t & encoded);
    void ins_mtlo(const uint32_t & encoded);
    void ins_syscall(const uint32_t & encoded);
    void ins_seq(const uint32_t & encoded);
    void ins_bgez(const uint32_t & encoded);
    void ins_bltz(const uint32_t & encoded);
};

#endif
