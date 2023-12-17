This is a simple MIPS assembly simulator/interpreter that mimics the MIPS architecture and functionality written in C++.
To build, simply compile with g++ using "g++ *.cpp" and you will be given your executable.

The following instructions are supported:
  ADD, ADDI, ADDIU, ADDU, AND, ANDI, BEQ, BNE, J, JAL, JR, LBU,
  LHU, LUI, LW, NOR, OR, ORI, SLT, SLTI, SLTIU, SLTU, SLL, SRL,
  SB, SC, SH, SW, SUB, SUBU, DIV, DIVU, MFHI, MFLO, MULT, MULTU,
  SRA, SLLV, SRAV, SRLV, XOR, XORI, BGTZ, BLEZ, JALR, LB, LH, MTHI,
  MTLO, SYSCALL, SEQ, BGEZ, BLTZ.

I also support the following pseudoinstructions:
  MOVE, LI, LA, LW (when used with a label), BLT, BLE, BGT, BGE.
