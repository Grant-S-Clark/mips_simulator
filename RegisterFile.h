//   File: RegisterFile.h
// Author: Grant Clark
//   Date: 12/17/2023

#ifndef REGISTER_FILE_H
#define REGISTER_FILE_H

#include "Common.h"

/*
  $0, $zero:                 Always holds zero.
  $1, $at:                   Reserved for pseudo-instructions
  $2 - $3, $v0 - $v1:        Function return values.
  $4 - $7, $a0 - $a3:        Used for function arguments.
  $8 - $15, $t0 - $t7:       Temporary registers.
  $16 - $24, $s0 - $s7:      Saved registers.
  $24 - $25, $t8 - $t9:      More temporary registers.
  $26 - $27, $k0 - $k1:      Reserved for Kernel.
  $28, $gp:                  Global area pointer (base of global data segment)
  $29, $sp:                  Stack pointer.
  $30, $fp:                  Frame pointer.
  $31, $ra:                  Return address.
*/

class RegisterFile
{
public:
    RegisterFile()
    {
        for (int i = 32; i >= 0; --i)
            *(x_ + i) = 0;
    }
    
    int32_t operator[](const int i) const
    { return *(x_ + i); }

    // This does allow you to change ANY register value, even the zero register,
    // but I will simply just assume that you wont do that unless you are
    // a buffoon and allow it.
    int32_t & operator[](const int i)
    { return *(x_ + i); }

    int32_t hi() const
    { return hi_; }
    int32_t & hi()
    { return hi_; }

    int32_t lo() const
    { return lo_; }
    int32_t & lo()
    { return lo_; }
    
private:
    int32_t x_[32];
    int32_t hi_;
    int32_t lo_;
};

#endif
