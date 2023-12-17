#================================================================
# Filename: main.s
# Author: Grant Clark
# Date: 10/19/2023
#
# Description
# The program prompts the user for an integer n which is used to
# create an n by n tic-tac-toe board. The computer plays first, and
# the user is prompted for a move. The game will continue until either
# a win, a loss, or a tie is decided.
#================================================================

##############################################
###                                        ###
###              TEXT SEGMENT              ###
###                                        ###
##############################################
        .text
        .globl main

### INPUTS:
### $a0 = value to be printed
### CHANGES: $v0
print_int:
        li      $v0, 1
        syscall
        jr      $ra

### INPUTS:
### $a0 = character to be printed
### CHANGES: $v0
print_char:
        li      $v0, 11
        syscall
        jr      $ra

### INPUTS:
### $a0 = address of string in data segment.
### CHANGES: $v0
print_string:
        li      $v0, 4
        syscall
        jr      $ra

### OUTPUTS:
### $v0 = integer from keyboard
### CHANGES: $v0
read_int:
        li      $v0, 5
        syscall
        jr      $ra

### INPUTS:
### $a0 = n
### $a1 = address of board.
### CHANGES: $a0, $a1, $t0, $t1
fill_board_with_spaces:
        mult    $a0, $a0
        mflo    $a0
        addu    $t0, $a0, $a1   # $t0 = Upper bound pointer of board array.
        li      $t1, ' '        # $t1 = space character (' ')
fill_board_with_spaces_loop:
        beq     $a1, $t0, fill_board_with_spaces_return
        sb      $t1, 0($a1)
        addiu   $a1, $a1, 1
        j       fill_board_with_spaces_loop
fill_board_with_spaces_return:  
        jr      $ra

### INPUTS:
### $a0 = n
### CHANGES: $t0, $a0
print_board_line:       
        addiu   $sp, $sp, -4
        sw      $ra, 0($sp)
        move    $t0, $a0        # $t0 = n
        li      $a0, '+'
        jal     print_char      # $t0 is not changed.
        move    $t1, $0         # $t1 = counter starting at 0.
print_board_line_loop:
        beq     $t1, $t0, print_board_line_return
        
        la      $a0, MINUS_PLUS
        jal     print_string    # "-+"

        addiu   $t1, $t1, 1

        j       print_board_line_loop
print_board_line_return:
        la      $a0, NEWLINE
        jal     print_string
        
        lw      $ra, 0($sp)
        addiu   $sp, $sp, 4
        jr      $ra
        
### INPUTS:
### $a0 = n
### $a1 = address of board
### CHANGES: $a0, $a1, $t0, $t1, $v0
print_board:    
        addiu   $sp, $sp, -16
# 0($sp) = $ra
# 4($sp) = $a0 = n
# 8($sp) = $a1
# 12($sp) = $t0 = Upper bound board pointer.
        sw      $ra, 0($sp)
        sw      $a0, 4($sp)
        
        jal     print_board_line # Sepatation line.
        
        lw      $a0, 4($sp)
        mult    $a0, $a0
        mflo    $t0
        addu    $t0, $t0, $a1   # $t0 = Upper bound pointer of board array.
print_board_loop:
        beq     $a1, $t0, print_board_return
        move    $t1, $0         # $t1 = counter
print_board_sub_loop:
        li      $a0, '|'
        jal     print_char
        
        lw      $a0, 4($sp)
        
        beq     $t1, $a0, print_board_sub_loop_end

        lb      $a0, 0($a1)
        jal     print_char
        
        addiu   $a1, $a1, 1
        addiu   $t1, $t1, 1
        j       print_board_sub_loop
print_board_sub_loop_end:
        sw      $a1, 8($sp)
        sw      $t0, 12($sp)

        la      $a0, NEWLINE
        jal     print_string

        lw      $a0, 4($sp)

        jal     print_board_line

        lw      $a0, 4($sp)
        lw      $a1, 8($sp)
        lw      $t0, 12($sp)
        
        j       print_board_loop
print_board_return:
        lw      $ra, 0($sp)
        addiu   $sp, $sp, 16
        jr      $ra

### INPUTS:
### $a0 = n
### $a1 = address of board
### OUTPUTS:
### $v0 = player row.
### $v1 = player column.
### CHANGES: $v0, $v1, $a0, $t0, $t1, $t2
get_player_move:
        addiu   $sp, $sp, -4
        sw      $ra, 0($sp)
        move    $t1, $a0        # Put n somewhere it wont get messed up
        li      $t2, ' '
get_player_move_loop:   
        la      $a0, ROW_PROMPT
        jal     print_string
        jal     read_int

        move    $t0, $v0

        la      $a0, COL_PROMPT
        jal     print_string
        jal     read_int

        move    $v1, $v0
        move    $v0, $t0

        mult    $v0, $t1
        mflo    $t0
        addu    $t0, $t0, $v1
        addu    $t0, $t0, $a1
        lb      $t0, 0($t0)
        bne     $t0, $t2, get_player_move_loop
        
        lw      $ra, 0($sp)
        addiu   $ra, $ra, 4
        
        jr      $ra

### INPUTS:
### $a0 = address of board
### $a1 = offset ((row * n) + col)
### $a2 = char value
### CHANGES: $a0
place_char_on_board:
        addu    $a0, $a0, $a1
        sb      $a2, 0($a0)
        
        jr      $ra
        
### INPUTS:
### $a0 = n
### $a1 = address of board.
### CHANGES: $a1, $t0, $t1
first_move:
        addiu   $t0, $a0, -1
        li      $t1, 2
        div     $t0, $t1
        mflo    $t0
        
        mult    $t0, $a0
        mflo    $t1
        addu    $t1, $t1, $t0
        addu    $a1, $a1, $t1
        li      $t0, 'O'
        ## Store 'O' at position row = (n - 1) / 2, col = (n - 1) / 2)
        sb      $t0, 0($a1)
        
        jr      $ra

### INPUTS:
### $a0 = n
### $a1 = address of board.
### OUTPUTS:
### $v0 = 0, 1, 2, 3 (win, loss, tie, nothing)
### CHANGES: $v0, $t0 - $t9
win_loss_tie:
        ##### Check Rows #####
        
        move    $t0, $a1        # $t0 = board scanner pointer.
        mult    $a0, $a0
        mflo    $t9             
        addu    $t9, $t9, $a1   # $t9 = upper bound of board array.
        move    $t3, $0         # $t3 = total # of pieces, used to check for tie.
        li      $t5, ' '        # $t5 = ' '
        li      $t6, 'X'        # $t6 = 'X'
win_loss_tie_rows:
        beq     $t0, $t9, win_loss_tie_rows_end # Scanned all pieces
        addu    $t8, $t0, $a0                   # $t8 = upper bound for inner loop.
        move    $t1, $0                         # $t1 = CPU piece count
        move    $t2, $0                         # $t2 = player piece count
win_loss_tie_rows_loop:
        beq     $t0, $t8, win_loss_tie_rows_loop_end
        lb      $t4, 0($t0)     # $t4 = char at board location.
        beq     $t4, $t5, win_loss_tie_rows_loop_if_end # Space found at location.
        beq     $t4, $t6, win_loss_tie_rows_loop_X      # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        addiu   $t3, $t3, 1
        j       win_loss_tie_rows_loop_if_end
win_loss_tie_rows_loop_X:
        addiu   $t2, $t2, 1
        addiu   $t3, $t3, 1
win_loss_tie_rows_loop_if_end:   
        addiu   $t0, $t0, 1
        j       win_loss_tie_rows_loop
win_loss_tie_rows_loop_end:
        li      $v0, 0
        beq     $t1, $a0, win_loss_tie_return # CPU Win
        li      $v0, 1
        beq     $t2, $a0, win_loss_tie_return # CPU Loss
        
        j win_loss_tie_rows
        
win_loss_tie_rows_end:
        li      $v0, 2
        mflo    $t7                           # n^2 still in lo register.
        beq     $t3, $t7, win_loss_tie_return # Tie found, return 2.
        
        ###### Check Columns #####
        
        move    $t7, $a1        
        addiu   $t7, $t7, -1    # $t7 = board scanner pointer starting point.
        addu    $t9, $a1, $a0   # $t9 = upper bound for pointer.
win_loss_tie_cols:
        addiu   $t7, $t7, 1
        move    $t0, $t7
        beq     $t0, $t9, win_loss_tie_cols_end
        move    $t1, $0                         # $t1 = CPU piece count
        move    $t2, $0                         # $t2 = player piece count
        move    $t3, $0                         # $t3 = total count for loop condition.
win_loss_tie_cols_loop:
        beq     $t3, $a0, win_loss_tie_cols_loop_end
        lb      $t4, 0($t0)     # $t4 = char at board location.
        beq     $t4, $t5, win_loss_tie_cols_loop_if_end # Space found at location.
        beq     $t4, $t6, win_loss_tie_cols_loop_X      # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       win_loss_tie_cols_loop_if_end
win_loss_tie_cols_loop_X:
        addiu   $t2, $t2, 1
win_loss_tie_cols_loop_if_end:
        addu    $t0, $t0, $a0   # $t0 += n
        addiu   $t3, $t3, 1     # $t3 += 1
        j       win_loss_tie_cols_loop
win_loss_tie_cols_loop_end:
        li      $v0, 0
        beq     $t1, $a0, win_loss_tie_return # CPU Win
        li      $v0, 1
        beq     $t2, $a0, win_loss_tie_return # CPU Loss
        
        j win_loss_tie_cols
win_loss_tie_cols_end:
        
        ##### Check left to right diagonal #####
        move    $t0, $a1
        move    $t9, $a0
        addiu   $t9, $t9, 1
        move    $t8, $t9        # $t8 = n + 1
        mult    $t9, $a0
        mflo    $t9
        addu    $t9, $t9, $a1   # $t9 = $a1 + n * (n + 1)
        move    $t1, $0         # $t1 = CPU piece count
        move    $t2, $0         # $t2 = player piece count

win_loss_tie_ltr:               # Left to right diagonal.
        beq     $t0, $t9, win_loss_tie_ltr_end
        lb      $t4, 0($t0)
        beq     $t4, $t5, win_loss_tie_ltr_if_end       # Space found at location.
        beq     $t4, $t6, win_loss_tie_ltr_X            # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       win_loss_tie_ltr_if_end
win_loss_tie_ltr_X:
        addiu   $t2, $t2, 1
win_loss_tie_ltr_if_end:
        addu    $t0, $t0, $t8   # $t0 += n + 1
        j      win_loss_tie_ltr 
win_loss_tie_ltr_end:
        li      $v0, 0
        beq     $t1, $a0, win_loss_tie_return # CPU Win
        li      $v0, 1
        beq     $t2, $a0, win_loss_tie_return # CPU Loss

        ##### Check right to left diagonal #####
        move    $t0, $a0
        addiu   $t0, $t0, -1
        move    $t9, $t0
        move    $t8, $t0        # $t8 = n - 1
        addu    $t0, $t0, $a1   # $t0 = $a1 + (n - 1)
        mult    $t9, $a0
        mflo    $t9
        addu    $t9, $t9, $t8
        addu    $t9, $t9, $a1   # $t9 = $a1 + (n + 1) * (n - 1)
        move    $t1, $0         # $t1 = CPU piece count
        move    $t2, $0         # $t2 = player piece count

win_loss_tie_rtl:               # Left to right diagonal.
        beq     $t0, $t9, win_loss_tie_rtl_end
        lb      $t4, 0($t0)
        beq     $t4, $t5, win_loss_tie_rtl_if_end       # Space found at location.
        beq     $t4, $t6, win_loss_tie_rtl_X            # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       win_loss_tie_rtl_if_end
win_loss_tie_rtl_X:
        addiu   $t2, $t2, 1
win_loss_tie_rtl_if_end:
        addu    $t0, $t0, $t8   # $t0 += n - 1
        j       win_loss_tie_rtl
win_loss_tie_rtl_end:
        li      $v0, 0
        beq     $t1, $a0, win_loss_tie_return # CPU Win
        li      $v0, 1
        beq     $t2, $a0, win_loss_tie_return # CPU Loss

        
        li      $v0, 3          # Never jumped to return, game still going.
win_loss_tie_return:    
        jr      $ra


### INPUTS:
### $a0 = n
### $a1 = address of board
### OUTPUTS:
### $v0 = row
### $v1 = column
### CHANGES: $v0, $v1, $t0, $t1, $t2, $t9
find_open_space:
        move    $t0, $a1        # $t0 = board scanner pointer.
        mult    $a0, $a0
        mflo    $t9
        addu    $t9, $t9, $a1   # $t9 = upppder scanner bound.
        li      $t2, ' '
find_open_space_loop:
        beq     $t0, $t9, find_open_space_return
        lb      $t1, 0($t0)
        bne     $t1, $t2, find_open_space_jump
        subu    $t0, $t0, $a1
        divu    $t0, $a0
        mflo    $v0
        mfhi    $v1
        j       find_open_space_return
find_open_space_jump:
        addiu   $t0, $t0, 1
        j       find_open_space_loop
find_open_space_return: 
        jr      $ra


### INPUTS:
### $a0 = n
### $a1 = address of board.
### OUTPUTS:
### $v0 = row
### $v1 = column
### CHANGES: $v0, $t0 - $t9
find_block:
        ##### Check Rows #####
        
        move    $t0, $a1        # $t0 = board scanner pointer.
        mult    $a0, $a0
        mflo    $t9             
        addu    $t9, $t9, $a1   # $t9 = upper bound of board array.
        li      $t5, ' '        # $t5 = ' '
        li      $t6, 'X'        # $t6 = 'X'
find_block_rows:
        beq     $t0, $t9, find_block_rows_end   # Scanned all pieces
        addu    $t8, $t0, $a0                   # $t8 = upper bound for inner loop.
        move    $t1, $0                         # $t1 = CPU piece count
        move    $t2, $0                         # $t2 = player piece count
find_block_rows_loop:
        beq     $t0, $t8, find_block_rows_loop_end
        lb      $t4, 0($t0)     # $t4 = char at board location.
        beq     $t4, $t5, find_block_rows_loop_space  # Space found at location.
        beq     $t4, $t6, find_block_rows_loop_X      # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_block_rows_loop_if_end
find_block_rows_loop_space:
        subu    $t3, $t0, $a1
        divu    $t3, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_block_rows_loop_if_end
find_block_rows_loop_X:
        addiu   $t2, $t2, 1
find_block_rows_loop_if_end:   
        addiu   $t0, $t0, 1
        j       find_block_rows_loop
find_block_rows_loop_end:
        move    $t3, $a0
        addiu   $t3, $t3, -1
        bne     $t3, $t2, find_block_rows   # Move not found ($t2 != n - 1)
        beq     $t1, $0, find_block_return # Move found ($t1 == 0)
        
        j find_block_rows
        
find_block_rows_end:
        
        ###### Check Columns #####
        
        move    $t7, $a1        
        addiu   $t7, $t7, -1    # $t7 = board scanner pointer starting point.
        addu    $t9, $a1, $a0   # $t9 = upper bound for pointer.
find_block_cols:
        addiu   $t7, $t7, 1
        move    $t0, $t7
        beq     $t0, $t9, find_block_cols_end
        move    $t1, $0                         # $t1 = CPU piece count
        move    $t2, $0                         # $t2 = player piece count
        move    $t3, $0                         # $t3 = total count for loop condition.
find_block_cols_loop:
        beq     $t3, $a0, find_block_cols_loop_end
        lb      $t4, 0($t0)     # $t4 = char at board location.
        beq     $t4, $t5, find_block_cols_loop_space # Space found at location.
        beq     $t4, $t6, find_block_cols_loop_X     # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_block_cols_loop_if_end
find_block_cols_loop_space:
        subu    $t4, $t0, $a1
        divu    $t4, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_block_cols_loop_if_end
find_block_cols_loop_X:
        addiu   $t2, $t2, 1
find_block_cols_loop_if_end:
        addu    $t0, $t0, $a0   # $t0 += n
        addiu   $t3, $t3, 1     # $t3 += 1
        j       find_block_cols_loop
find_block_cols_loop_end:
        move    $t4, $a0
        addiu   $t4, $t4, -1
        bne     $t4, $t2, find_block_cols # Move not found ($t2 != n - 1)
        beq     $t1, $0, find_block_return # Move found ($t1 == 0)
        
        j find_block_cols
find_block_cols_end:
        
        ##### Check left to right diagonal #####
        move    $t0, $a1
        move    $t9, $a0
        addiu   $t9, $t9, 1
        move    $t8, $t9        # $t8 = n + 1
        mult    $t9, $a0
        mflo    $t9
        addu    $t9, $t9, $a1   # $t9 = $a1 + n * (n + 1)
        move    $t1, $0         # $t1 = CPU piece count
        move    $t2, $0         # $t2 = player piece count

find_block_ltr:               # Left to right diagonal.
        beq     $t0, $t9, find_block_ltr_end
        lb      $t4, 0($t0)
        beq     $t4, $t5, find_block_ltr_space        # Space found at location.
        beq     $t4, $t6, find_block_ltr_X            # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_block_ltr_if_end
find_block_ltr_space:
        subu    $t3, $t0, $a1
        divu    $t3, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_block_ltr_if_end
find_block_ltr_X:
        addiu   $t2, $t2, 1
find_block_ltr_if_end:
        addu    $t0, $t0, $t8   # $t0 += n + 1
        j       find_block_ltr
find_block_ltr_end:
        move    $t3, $a0
        addiu   $t3, $t3, -1
        bne     $t3, $t2, find_block_ltr_fail   # Move not found ($t2 != n - 1)
        beq     $t1, $0, find_block_return      # Move found ($t1 == 0)
        
find_block_ltr_fail:
        
        ##### Check right to left diagonal #####
        move    $t0, $a0
        addiu   $t0, $t0, -1
        move    $t9, $t0
        move    $t8, $t0        # $t8 = n - 1
        addu    $t0, $t0, $a1   # $t0 = $a1 + (n - 1)
        mult    $t9, $a0
        mflo    $t9
        addu    $t9, $t9, $t8
        addu    $t9, $t9, $a1   # $t9 = $a1 + (n + 1) * (n - 1)
        move    $t1, $0         # $t1 = CPU piece count
        move    $t2, $0         # $t2 = player piece count

find_block_rtl:               # Left to right diagonal.
        beq     $t0, $t9, find_block_rtl_end
        lb      $t4, 0($t0)
        beq     $t4, $t5, find_block_rtl_space        # Space found at location.
        beq     $t4, $t6, find_block_rtl_X            # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_block_rtl_if_end
find_block_rtl_space:
        subu    $t3, $t0, $a1
        divu    $t3, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_block_rtl_if_end
find_block_rtl_X:
        addiu   $t2, $t2, 1
find_block_rtl_if_end:
        addu    $t0, $t0, $t8   # $t0 += n - 1
        j       find_block_rtl
find_block_rtl_end:
        move    $t3, $a0
        addiu   $t3, $t3, -1
        bne     $t3, $t2, find_block_rtl_fail   # Move not found ($t2 != n - 1)
        beq     $t1, $0, find_block_return      # Move found ($t1 == 0)

find_block_rtl_fail:    
        li      $v0, -1          # Never jumped to return, move not found, $v0 = -1.
find_block_return:    
        jr      $ra

### INPUTS:
### $a0 = n
### $a1 = address of board.
### OUTPUTS:
### $v0 = row
### $v1 = column
### CHANGES: $v0, $t0 - $t9
find_win:
        ##### Check Rows #####
        
        move    $t0, $a1        # $t0 = board scanner pointer.
        mult    $a0, $a0
        mflo    $t9             
        addu    $t9, $t9, $a1   # $t9 = upper bound of board array.
        li      $t5, ' '        # $t5 = ' '
        li      $t6, 'X'        # $t6 = 'X'
find_win_rows:
        beq     $t0, $t9, find_win_rows_end   # Scanned all pieces
        addu    $t8, $t0, $a0                   # $t8 = upper bound for inner loop.
        move    $t1, $0                         # $t1 = CPU piece count
        move    $t2, $0                         # $t2 = player piece count
find_win_rows_loop:
        beq     $t0, $t8, find_win_rows_loop_end
        lb      $t4, 0($t0)     # $t4 = char at board location.
        beq     $t4, $t5, find_win_rows_loop_space  # Space found at location.
        beq     $t4, $t6, find_win_rows_loop_X      # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_win_rows_loop_if_end
find_win_rows_loop_space:
        subu    $t3, $t0, $a1
        divu    $t3, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_win_rows_loop_if_end
find_win_rows_loop_X:
        addiu   $t2, $t2, 1
find_win_rows_loop_if_end:   
        addiu   $t0, $t0, 1
        j       find_win_rows_loop
find_win_rows_loop_end:
        move    $t3, $a0
        addiu   $t3, $t3, -1
        bne     $t3, $t1, find_win_rows   # Move not found ($t1 != n - 1)
        beq     $t2, $0, find_win_return # Move found ($t2 == 0)
        
        j find_win_rows
        
find_win_rows_end:
        
        ###### Check Columns #####
        
        move    $t7, $a1        
        addiu   $t7, $t7, -1    # $t7 = board scanner pointer starting point.
        addu    $t9, $a1, $a0   # $t9 = upper bound for pointer.
find_win_cols:
        addiu   $t7, $t7, 1
        move    $t0, $t7
        beq     $t0, $t9, find_win_cols_end
        move    $t1, $0                         # $t1 = CPU piece count
        move    $t2, $0                         # $t2 = player piece count
        move    $t3, $0                         # $t3 = total count for loop condition.
find_win_cols_loop:
        beq     $t3, $a0, find_win_cols_loop_end
        lb      $t4, 0($t0)     # $t4 = char at board location.
        beq     $t4, $t5, find_win_cols_loop_space # Space found at location.
        beq     $t4, $t6, find_win_cols_loop_X     # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_win_cols_loop_if_end
find_win_cols_loop_space:
        subu    $t4, $t0, $a1
        divu    $t4, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_win_cols_loop_if_end
find_win_cols_loop_X:
        addiu   $t2, $t2, 1
find_win_cols_loop_if_end:
        addu    $t0, $t0, $a0   # $t0 += n
        addiu   $t3, $t3, 1     # $t3 += 1
        j       find_win_cols_loop
find_win_cols_loop_end:
        move    $t4, $a0
        addiu   $t4, $t4, -1
        bne     $t4, $t1, find_win_cols # Move not found ($t1 != n - 1)
        beq     $t2, $0, find_win_return # Move found ($t2 == 0)
        
        j find_win_cols
find_win_cols_end:
        
        ##### Check left to right diagonal #####
        move    $t0, $a1
        move    $t9, $a0
        addiu   $t9, $t9, 1
        move    $t8, $t9        # $t8 = n + 1
        mult    $t9, $a0
        mflo    $t9
        addu    $t9, $t9, $a1   # $t9 = $a1 + n * (n + 1)
        move    $t1, $0         # $t1 = CPU piece count
        move    $t2, $0         # $t2 = player piece count

find_win_ltr:               # Left to right diagonal.
        beq     $t0, $t9, find_win_ltr_end
        lb      $t4, 0($t0)
        beq     $t4, $t5, find_win_ltr_space        # Space found at location.
        beq     $t4, $t6, find_win_ltr_X            # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_win_ltr_if_end
find_win_ltr_space:
        subu    $t3, $t0, $a1
        divu    $t3, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_win_ltr_if_end
find_win_ltr_X:
        addiu   $t2, $t2, 1
find_win_ltr_if_end:
        addu    $t0, $t0, $t8   # $t0 += n + 1
        j       find_win_ltr
find_win_ltr_end:
        move    $t3, $a0
        addiu   $t3, $t3, -1
        bne     $t3, $t1, find_win_ltr_fail   # Move not found ($t1 != n - 1)
        beq     $t2, $0, find_win_return      # Move found ($t2 == 0)
        
find_win_ltr_fail:
        
        ##### Check right to left diagonal #####
        move    $t0, $a0
        addiu   $t0, $t0, -1
        move    $t9, $t0
        move    $t8, $t0        # $t8 = n - 1
        addu    $t0, $t0, $a1   # $t0 = $a1 + (n - 1)
        mult    $t9, $a0
        mflo    $t9
        addu    $t9, $t9, $t8
        addu    $t9, $t9, $a1   # $t9 = $a1 + (n + 1) * (n - 1)
        move    $t1, $0         # $t1 = CPU piece count
        move    $t2, $0         # $t2 = player piece count

find_win_rtl:               # Left to right diagonal.
        beq     $t0, $t9, find_win_rtl_end
        lb      $t4, 0($t0)
        beq     $t4, $t5, find_win_rtl_space        # Space found at location.
        beq     $t4, $t6, find_win_rtl_X            # X found at location
        ## 'O' found at location.
        addiu   $t1, $t1, 1
        j       find_win_rtl_if_end
find_win_rtl_space:
        subu    $t3, $t0, $a1
        divu    $t3, $a0
        mflo    $v0     
        mfhi    $v1             # Capture row col of space.
        j       find_win_rtl_if_end
find_win_rtl_X:
        addiu   $t2, $t2, 1
find_win_rtl_if_end:
        addu    $t0, $t0, $t8   # $t0 += n - 1
        j       find_win_rtl
find_win_rtl_end:
        move    $t3, $a0
        addiu   $t3, $t3, -1
        bne     $t3, $t1, find_win_rtl_fail   # Move not found ($t1 != n - 1)
        beq     $t2, $0, find_win_return      # Move found ($t2 == 0)

find_win_rtl_fail:    
        li      $v0, -1          # Never jumped to return, move not found, $v0 = -1.
find_win_return:    
        jr      $ra

        
### INPUTS:
### $a0 = n
### $a1 = address of board.
### OUTPUTS:
### $v0 = row
### $v1 = column
### CHANGES: $v0, $t0 - $t9
cpu_move:
        addiu   $sp, $sp, -4
        sw      $ra, 0($sp)

        ## CHECK FOR WINNING MOVE
        jal     find_win
        li      $t0, -1
        bne     $v0, $t0, cpu_move_return # If a winning move was found.

        ## CHECK FOR BLOCKING MOVE
        jal     find_block
        li      $t0, -1
        bne     $v0, $t0, cpu_move_return # If a blocking move was found.

        ## CHECK FOR OPEN SPACE TO PLAY
        jal     find_open_space
        
cpu_move_return:        
        lw      $ra, 0($sp)
        addiu   $sp, $sp, 4
        jr      $ra
        
#####################
### MAIN FUNCTION ###
#####################
main:   
        la      $a0, GREETING
        jal     print_string

        jal     read_int
        move    $s0, $v0        # $s0 = n

        li      $s1, 1
        beq     $s0, $0, main_end

        move    $a0, $s0
        la      $a1, BOARD
        jal     fill_board_with_spaces

        la      $a0, FIRST_MOVE
        jal     print_string

        move    $a0, $s0
        la      $a1, BOARD
        jal     first_move
        
        la      $a1, BOARD
        jal     print_board

        move    $s1, $0
        
        li      $t0, 1
        beq     $s0, $t0, main_end # If the board is 1x1, CPU wins.

main_loop:
        move    $a0, $s0
        la      $a1, BOARD
        jal     get_player_move         # Get player move
       
        multu   $s0, $v0
        mflo    $a1
        addu    $a1, $a1, $v1
        la      $a0, BOARD
        li      $a2, 'X'
        jal     place_char_on_board     # Place player move

        move    $a0, $s0
        la      $a1, BOARD
        jal     print_board     # Print the board.

        move    $a0, $s0
        la      $a1, BOARD
        jal     win_loss_tie    # Check for game end state.

        move    $s1, $v0
        li      $t0, 3
        bne     $s1, $t0, main_end # If a game end state found, break game loop.

        jal     cpu_move        # Get CPU move

        multu   $s0, $v0
        mflo    $a1
        addu    $a1, $a1, $v1
        la      $a0, BOARD
        li      $a2, 'O'
        jal     place_char_on_board     # Place CPU move

        move    $a0, $s0
        la      $a1, BOARD
        jal     print_board     # Print the board.

        move    $a0, $s0
        la      $a1, BOARD
        jal     win_loss_tie    # Check for game end state.

        move    $s1, $v0
        li      $t0, 3
        bne     $s1, $t0, main_end # If a game end state found, break game loop.

        j       main_loop
        
main_end:
        # $s1 = 0, 1, 2 (CPU_WIN, CPU_DRAW, CPU_LOSS)
        la      $a0, ENDING_STRINGS
        addu    $s1, $s1, $s1
        addu    $s1, $s1, $s1
        addu    $a0, $s1, $a0
        lw      $a0, 0($a0)

        jal     print_string    # Ending message.
        
        li      $v0, 10
        syscall                 # END OF PROGRAM


##############################################
###                                        ###
###              DATA SEGMENT              ###
###                                        ###
##############################################
        .data

        ## String constants.
NEWLINE:        .asciiz "\n"
GREETING:       .asciiz "Let's play a game of tic-tac-toe.\nEnter n:"
FIRST_MOVE:     .asciiz "I'll go first.\n"
CPU_WIN:        .asciiz "I'm the winner!\n"
CPU_LOSS:       .asciiz "You are the winner!\n"
CPU_DRAW:       .asciiz "We have a draw!\n"
ROW_PROMPT:     .asciiz "Enter a row: "
COL_PROMPT:     .asciiz "Enter a column: "
MINUS_PLUS:     .asciiz "-+"

ENDING_STRINGS: .word CPU_WIN CPU_LOSS CPU_DRAW
        
        ## 10000 characters for the board, so max n
        ## is going to be 100.
BOARD:  .space 10000
