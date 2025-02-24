//   File: Simulator.cpp
// Author: Grant Clark
//   Date: 12/17/2023

#include "Simulator.h"


// Main runner function, lets user decide what mode to run
// in and begins execution.
void Simulator::run()
{
    std::string input;

    while (true)
    {
        std::cout << "[S]tart simulator\n[R]ead from file\n[Q]uit\nMode: ";
        std::getline(std::cin, input);
            
        switch (input[0])
        {
            case 's':
            case 'S':
                running = true;
                sim_mode = true;
                run_simulator_mode();
                return;
            case 'r':
            case 'R':
                running = true;
                sim_mode = false;
                run_read_mode();
                return;
            case 'q':
            case 'Q':
                return;
            default:
                std::cout << "Invalid mode.\n\n";
        }
    }

    return;
}

//////////////////////////////////////
///// Interpreter mode functions /////
//////////////////////////////////////

// Run the inpterpreter mode of the simulator.
void Simulator::run_simulator_mode()
{
    // Default to text segment
    current_segment = TEXT;
    
    program_counter = text_seg_addr;

    std::cout << "? - help\n";
    
    while (running)
    {
        std::cout << '[' << mips_segment_str(current_segment) << "] "
                  << "0x" << std::setfill('0') << std::setw(8)
                  << std::hex << program_counter << std::dec
                  << " >>> ";
        
        std::string input;
        std::getline(std::cin, input);

        // Attempt to interpret the input and inform user about
        // errors.
        try
        {
            interpret_input(input);
        }
        
        catch (SimulatorError & e)
        {
            std::cout << "Simulator Error: " << e.what() << std::endl;
        }
        catch (std::invalid_argument & e)
        {
            std::cout << "Invalid argument to function: " << e.what() << std::endl;
        }
    }
    
    return;
}


// Make sure labels only contain valid characters and dont
// start with a digit.
bool Simulator::valid_label(const std::string & s) const
{
    // Cannot begin with a digit.
    if (isdigit(s[0]))
        return false;
        
    unsigned int n = s.size();
    for (unsigned int i = 0; i < n; ++i)
        if (!isalnum(s[i]) && s[i] != '_' && s[i] != '.')
            return false;

    return true;
}

// Handle an input from the user in interpreter mode.
void Simulator::interpret_input(std::string input)
{
    // Get rid of comments and make sure whitespace is
    // consistent.
    input = truncate_comments(input);
    input = strip_useless_whitespace(input);

    // Ignore empty string
    if (input.size() == 0)
        return;

    // Handle non-mips simulator commands.
    if (input == "?")
    {
        print_simulator_help();
    }
    
    else if (input == "regs")
    {
        print_registers();
    }

    else if (input == "labels")
    {
        print_labels();
    }

    else if (input == "data")
    {
        print_data_segment();
    }

    // Non-mips command for jumping to a previously entered instruction
    // Without locking yourself into an infinite loop using a jump.
    // This will only acccept hexadecimal addresses.
    else if (input.substr(0, 4) == "goto")
    {
        std::string addr_str = input.substr(5);
        uint32_t addr = std::stoi(addr_str, 0, 16);
        if (addr >= 0x00040000 && addr < 0x10010000 && addr % 4 == 0 // Make sure valid and within text segment.
            && program_counter > addr) // You cant goto a point in front of the program counter
        {
            uint32_t curr_pc = program_counter;
            program_counter = addr;

            // Execute instructions in the text segment from addr to the program counter's value
            while (program_counter != curr_pc)
            {
                execute(text[(program_counter - TEXT_START) >> 2]);
            }
        }
        else
            throw SimulatorError("Invalid hexadecimal goto address.");
    }

    // Command for saving to file.
    else if (input.substr(0, 6) == "saveto")
    {
        // Get the filename to save to.
        std::string filename = input.substr(7);
        std::cout << "filename=" << filename << std::endl;

        std::ofstream ofs;
        ofs.open(filename, std::ofstream::out | std::ofstream::trunc);
        if (ofs.fail())
            throw SimulatorError("Invalid filename for saveto command.");

        // Put all the valid commands entered so far into the file.
        for (const std::string & s : valid_sim_inputs)
            ofs << s << '\n';
        ofs.close();
    }

    // Handle MIPS inputs
    else
    {
        // Save the base input for later (for saving to file) and turn registers into their
        // numerical forms (i.e. $sp --> 29)
        std::string saved_input = input;
        input = replace_registers(input);

        // Isolate each part of the input to work with.
        std::vector< std::string > strs = split_input(input);

        // Handle a label at the beginning of the instruction
        // ex: "labelname: addiu $s0, $s0, 42"
        if (strs[0].back() == ':')
        {
            // Make sure the label is formatted correctly
            if (!valid_label(strs[0].substr(0, strs[0].size() - 1)))
                throw SimulatorError("Invalid label.");
        
            // Put label into the label hashtable
            strs[0].pop_back();
            if (labels.find(strs[0]) != labels.end())
                throw SimulatorError("Duplicate label.");
            labels[strs[0]] = program_counter;

            // Append onto input list.
            valid_sim_inputs.push_back(strs[0] + ":");

            // Remove label from input for storage of the
            // string of the command seperately if it was not
            // the only thing input.
            if (strs.size() > 1)
            {
                saved_input = saved_input.substr(strs[0].size() + 2);
            }
            
            // Remove the label from the strings and continue.
            strs.erase(strs.begin());
        }

        // Make sure there is still stuff to do if a label was removed.
        if (!strs.empty())
        {
            // Change hexidecimal and binary numbers into base 10.
            format_immediates(strs);

            // Handle the pseudoinstructions if necessary.
            Instruction pseudo = Instruction(0);
            if (current_segment == TEXT)
            {
                // Check for pseudoinstructions
                pseudo = is_pseudo(strs[0], strs.back());
                if (pseudo)
                {
                    sim_currently_pseudo = true;
                    handle_pseudo(pseudo, strs);
                    sim_currently_pseudo = false;
            
                    // Append onto input list.
                    valid_sim_inputs.push_back(saved_input);
                }
            }

            // If there was no pseudoinstruction handled
            if (!pseudo)
            {
                // Format the labels into base 10 numbers from map.
                replace_labels(strs);

                // '.' input, check to see if it is a segment change or
                // if it denotes an entrypoint (".globl main"). If not,
                // check to see if you are in the data segment and act
                // accordingly depending on the input.
                if (strs[0][0] == '.')
                {
                    // Segment change
                    if (strs.size() == 1)
                    {
                        if (strs[0] == ".text")
                        {
                            // Do nothing if already in text segment.
                            if (current_segment == TEXT)
                            {
                                // Append onto input list.
                                valid_sim_inputs.push_back(saved_input);
                            }

                            // Switch to text segment.
                            else
                            {
                                current_segment = TEXT;
                                data_seg_addr = program_counter;
                                program_counter = text_seg_addr;
                            }
                        }

                        else if (strs[0] == ".data")
                        {
                            // Do nothing if already in data segment.
                            if (current_segment == DATA)
                            {
                                // Append onto input list.
                                valid_sim_inputs.push_back(saved_input);
                                return;
                            }

                            // Switch to data segment.
                            else
                            {
                                current_segment = DATA;
                                text_seg_addr = program_counter;
                                program_counter = data_seg_addr;
                            }
                        }

                        // Append onto input list.
                        valid_sim_inputs.push_back(saved_input);
                    }

                    // ".globl < label > "
                    // Simply just put this onto the input list.
                    else if (strs.size() == 2 && strs[0] == ".globl")
                    {
                        if (!valid_label(strs[1]))
                            throw SimulatorError("Invalid label.");

                        // Append onto input list.
                        valid_sim_inputs.push_back(saved_input);
                    }

                    // If the input did not fall into the above categories, it is a data segment input.
                    else
                    {
                        add_to_data_segment(strs);
                    }
                }

                // No special '.' character, handle text segment input
                else
                {
                    // If you get here somehow, you must have created an invalid data segment
                    // input that did not get captured correctly (forgot the '.'?).
                    if (current_segment != TEXT)
                    {
                        throw SimulatorError("Invalid data segment input.");
                    }

                    // Perform encoding and execute.
                    uint32_t encoding = encode(strs);

                    // DEBUGGING
                    // std::cout << std::bitset<32>(encoding) << std::endl;

                    // Save where the input instruction lives in the text segment.
                    input_addr.push_back({input, program_counter});

                    // Attempt execution of the encoded instruction.
                    try
                    {
                        uint32_t old_pc = program_counter;
                        execute(encoding);
            
                        // Store the encoding
                        // >> 2 is used to index the text segment
                        // because instruction addresses are 4 bytes large.
                        text[(old_pc - TEXT_START) >> 2] = encoding;
            
                        // if you jumped, execute instructions at that location
                        // until you are back where we are supposed to be.
                        while (program_counter != old_pc + 4)
                        {
                            execute(text[(program_counter - TEXT_START) >> 2]);
                        }
                    }
                    catch (SimulatorError & e)
                    {
                        input_addr.pop_back();
                        throw e;
                    }
                    catch (std::invalid_argument & e)
                    {
                        input_addr.pop_back();
                        throw e;
                    }

                    // If this instruction was not produced via a pseudo instruction,
                    // append it onto the input list.
                    if (!sim_currently_pseudo)
                        valid_sim_inputs.push_back(saved_input);
                } // else (for handling text segment input)
            } // if (!pseudo)
        } // if (!strs.empty())
    } // else (for handling mips instructions)
    
    return;
}

// 31 --> "$ra"
std::string Simulator::uint_to_reg(const unsigned int reg) const
{
    // Array to store the values.
    static const std::string uint_to_reg_arr[32] =
        {"$zero","$at","$v0","$v1",
         "$a0","$a1","$a2","$a3",
         "$t0","$t1","$t2","$t3",
         "$t4","$t5","$t6","$t7",
         "$s0","$s1","$s2","$s3",
         "$s4","$s5","$s6","$s7",
         "$t8","$t9","$k0","$k1",
         "$gp","$sp","$fp","$ra" };
   
    if (reg > 31)
    {
        throw SimulatorError("Invalid register number sent to int_to_reg().");
    }

    return uint_to_reg_arr[reg];
}

// Display the interpreter help menu
void Simulator::print_simulator_help() const
{
    std::cout << std::setfill('=') << std::setw(65) << '\n';
    std::cout << "HELP\n";
    std::cout << std::setw(65) << '\n';

    std::cout << "\"?\" - Print this help screen.\n";
    std::cout << "\"regs\" - Print the register information screen.\n";
    std::cout << "\"data\" - Print the data segment information screen.\n";
    std::cout << "\"labels\" - Print the label information screen.\n";
    std::cout << "\"saveto <filename>\" - Save all previous inputs into a file.\n";
    std::cout << "\"goto <hexadecimal address>\" - Jump to a previously input\n\tinstruction without saving a jump instruction.\n";
    std::cout << "To stop the program, perform syscall 10. i.e. \"li $v0, 10\"\n\tthen \"syscall\".\n";
    return;
}

// 97 --> "'a'", 10 --> "'\\n'"
std::string Simulator::char_value_str(const unsigned int val) const
{
    switch (val)
    {
        case 0:
            return "'\\0'";
        case 9:
            return "'\\t'";
        case 10:
            return "'\\n'";
        case 11:
            return "'\\v'";
        default:
            return { '\'', char(val), '\'' };
    }
}

// Print human-readable table of the registers
// in interpreter mode
void Simulator::print_registers() const
{
    std::cout << std::setfill('=') << std::setw(65) << '\n';
    std::cout << "REGISTERS\n";
    std::cout << std::setw(65) << '\n';
    std::cout << std::setfill(' ')
              << std::setw(12) << "reg number" << '|'
              << std::setw(12) << "reg name" << '|'
              << std::setw(12) << "value (int)" << '|'
              << std::setw(12) << "value (hex)" << '|'
              << std::setw(12) << "value (char)" << '\n';
    std::cout << std::setfill('-')
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '\n';
    std::cout << std::setfill(' ');
    unsigned int reg_w;
    for (unsigned int i = 0; i < 32; ++i)
    {
        reg_w = (i < 10 ? 11 : 10);
        std::cout << std::setw(reg_w) << '$' << i << '|';
        std::cout << std::setw(12) << uint_to_reg(i) << '|';
        std::cout << std::setw(12) << regs[i] << '|';
        std::cout << "  0x" << std::setw(8) << std::hex << std::setfill('0')
                  << regs[i] << std::setfill(' ') << std::dec << '|';
        std::cout << std::setw(12) << char_value_str(regs[i]) << '\n';
    }
    // Hi register
    std::cout << std::setw(12) << "N/A" << '|';
    std::cout << std::setw(12) << "$hi" << '|';
    std::cout << std::setw(12) << regs.hi() << '|';
    std::cout << "  0x" << std::setw(8) << std::hex << std::setfill('0')
              << regs.hi() << std::setfill(' ') << std::dec << '|';
    std::cout << std::setw(12) << char_value_str(regs.hi()) << '\n';
    
    // Lo register
    std::cout << std::setw(12) << "N/A" << '|';
    std::cout << std::setw(12) << "$lo" << '|';
    std::cout << std::setw(12) << regs.lo() << '|';
    std::cout << "  0x" << std::setw(8) << std::hex << std::setfill('0')
              << regs.lo() << std::setfill(' ') << std::dec << '|';
    std::cout << std::setw(12) << char_value_str(regs.lo()) << '\n';
    
    std::cout << std::setfill('-')
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '\n';
    std::cout << std::setfill(' ');

    return;
}


// Print a list of human-readable labels in the interpreter mode
void Simulator::print_labels() const
{
    std::cout << std::setfill('=') << std::setw(65) << '\n';
    std::cout << "LABELS\n";
    std::cout << std::setw(65) << '\n';
    std::cout << std::setfill(' ') << std::hex;
    unsigned int max = 0;
    for (const std::pair< std::string, uint32_t > & p : labels)
        if (p.first.size() > max)
            max = p.first.size();
    ++max;
    for (const std::pair< std::string, uint32_t > & p : labels)
    {
        std::cout << "0x" << std::setfill('0') << std::setw(8)
                  << p.second << '|' << std::setfill(' ')
                  << std::setw(max) << p.first << '\n';
    }
    std::cout << std::dec;
    
    return;
}

// Display human-readable table of the data segment in interpreter mode.
// Your data will still be located in the correct spots, but
// if this printed each and every byte the output will be EXTREMELY
// long, so I opted to print out multiples of 4 so you can see
// any .word values that are input.
void Simulator::print_data_segment() const
{
    uint32_t max_addr = (current_segment == DATA ? program_counter : data_seg_addr);

    std::cout << std::setfill('=') << std::setw(65) << '\n';
    std::cout << "DATA SEGMENT\n";
    std::cout << std::setw(65) << '\n';

    std::cout << std::setfill(' ')
              << std::setw(12) << "addr (hex)" << '|'
              << std::setw(12) << "addr (int)" << '|'
              << std::setw(12) << "value (int)" << '|'
              << std::setw(12) << "value (hex)" << '|'
              << std::setw(12) << "value (char)" << '\n';
    std::cout << std::setfill('-')
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '\n';
    std::cout << std::setfill(' ');
    
    for (uint32_t addr = DATA_START; addr <= max_addr; addr += 4)
    {
        std::cout << std::hex << std::setw(12) << addr << '|'
                  << std::dec << std::setw(12) << addr << '|';

        // Compute value of the 4 bytes at this location.
        uint32_t val = data[addr - DATA_START] << 24;
        val |= data[addr - DATA_START + 1] << 16;
        val |= data[addr - DATA_START + 2] << 8;
        val |= data[addr - DATA_START + 3];

        std::cout << std::setw(12) << val << '|'
                  << std::hex << std::setw(12) << val << '|';

        std::string s = char_value_str(data[addr - DATA_START]);
        s = s.substr(1, s.size() / 2);
        std::cout << std::setw(3) << s;
        s = char_value_str(data[addr - DATA_START + 1]);
        s = s.substr(1, s.size() / 2);
        std::cout << std::setw(3) << s;
        s = char_value_str(data[addr - DATA_START + 2]);
        s = s.substr(1, s.size() / 2);
        std::cout << std::setw(3) << s;
        s = char_value_str(data[addr - DATA_START + 3]);
        s = s.substr(1, s.size() / 2);
        std::cout << std::setw(3) << s;
        std::cout << std::dec << '\n';
    }

    std::cout << std::setfill('-')
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '+'
              << std::setw(13) << '\n';
    std::cout << std::setfill(' ');
    
    return;
}

///////////////////////////////
///// Read mode functions /////
///////////////////////////////

// Read from file and execute mips instructions.
void Simulator::run_read_mode()
{
    current_segment = NONE;
    
    std::string filename = "";
    std::ifstream file;
    while (true)
    {
        // Open file
        std::cout << "Input file to run: ";
        std::getline(std::cin, filename);
        file.open(filename, std::ios::in);
        if (!file.fail())
            break;
    }

    try
    {
        // Attempt to execute program.
        run_file(file);
    }
    catch (SimulatorError & e)
    {
        std::cout << e.what() << std::endl;
    }
    file.close();
    
    return;
}

// Given a stream to a file, load the file contents and
// then run through the instructions after they have all been
// loaded.
void Simulator::run_file(std::ifstream & file)
{
    std::string line;
    unsigned int i = 1;
    while (std::getline(file, line))
    {
        try
        {
            // Prepare the line to be encoded
            prepare_read_input(line, i);
            ++i;
        }
        catch (SimulatorError & e)
        {
            throw SimulatorError("(Line " + std::to_string(i) + ") Simulator Error: " + e.what());
        }
        catch (std::invalid_argument & e)
        {
            throw SimulatorError("(Line " + std::to_string(i) + ") Invalid argument to function: " + e.what());
        }
    }

    // Validate the entrypoint
    if (labels.find(entrypoint_label) != labels.end())
        entrypoint_addr = labels.find(entrypoint_label)->second;
    else
        throw SimulatorError("No entrypoint defined. (.globl <label>)");

    // Perform computation on all data segment entries.
    program_counter = 0x10010000;
    for (StrsAddressSegmentLine & data : to_be_encoded)
    {
        if (!data.text)
        {
            replace_labels(data.strs);
            add_to_data_segment(data.strs);
        }
    }
    
    // Replace labels in the instructions with their values.
    for (StrsAddressSegmentLine & data : to_be_encoded)
        for (std::string & s : data.strs)
            if (labels.find(s) != labels.end())
                s = std::to_string(labels.find(s)->second);

    std::unordered_map< uint32_t, unsigned int > line_numbers;
    
    // Encode the instructions in the text segment.
    for (StrsAddressSegmentLine & data : to_be_encoded)
    {
        if (data.text)
        {
            program_counter = data.address;
            Instruction pseudo = is_pseudo(data.strs[0], data.strs.back());
            if (pseudo)
            {
                read_handle_pseudo(pseudo, data.strs, data.address, data.line, line_numbers);
            }
            else
            {
                text[(program_counter - TEXT_START) >> 2] = encode(data.strs);
                line_numbers[program_counter] = data.line;
            }
        }
    }

    // Set entrypoint
    program_counter = entrypoint_addr;

    // Execute instructions until an error occurs or the program exits.
    while (running)
    {
        try
        {
            execute(text[(program_counter - TEXT_START) >> 2]);
        }
        catch (SimulatorError & e)
        {
            throw SimulatorError(e.what() + " (line " + std::to_string(line_numbers[program_counter]) + ").");
        }
    }
    
    return;
}


void Simulator::prepare_read_input(std::string input, unsigned int line)
{
    // Get rid of comments and get rid of extra whitespace.
    input = truncate_comments(input);
    input = strip_useless_whitespace(input);

    // Ignore empty lines.
    if (input.size() == 0)
        return;

    // Get numerical register values.
    input = replace_registers(input);

    // Split input into individual parts to process
    std::vector< std::string > strs = split_input(input);

    // Handle labels
    if (strs[0].back() == ':')
    {
        if (current_segment == NONE)
            throw SimulatorError("Labels cannot exist outside of a segment.");

        strs[0].pop_back();
        if (!valid_label(strs[0]))
            throw SimulatorError("Invalid label.");
        
        if (labels.find(strs[0]) != labels.end())
            throw SimulatorError("Duplicate label.");

        // Save the label with address
        labels[strs[0]] = program_counter;

        // Remove the label from the strings and continue.
        strs.erase(strs.begin());
    }

    // Make sure there wasnt only a label
    if (!strs.empty())
    {
        // Change binary and hex numbers into base 10 for consistent computation.
        format_immediates(strs);

        // Handle the pseudoinstructions if necessary.
        if (current_segment == TEXT)
        {
            // Check for pseudoinstructions, throw errors if they are not formatted correctly.
            Instruction pseudo = is_pseudo(strs[0], strs.back());
            if (pseudo)
            {
                switch (pseudo)
                {
                    case MOVE:
                        if (strs.size() != 3)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        // Only one instruction, dont move PC.
                        break;
                    case LI:
                        if (strs.size() != 3)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 4; // Read mode will force 2 instructions for consistency.
                        break;
                    case LW:
                        if (strs.size() != 3)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 8;
                        break;
                    case LA:
                        if (strs.size() != 3)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 4;
                        break;
                    case BLT:
                        if (strs.size() != 4)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 4;
                        break;
                    case BLE:
                        if (strs.size() != 4)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 4;
                        break;
                    case BGT:
                        if (strs.size() != 4)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 4;
                        break;
                    case BGE:
                        if (strs.size() != 4)
                            throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
                        program_counter += 4;
                        break;
                }
            } // if (pseudo)
        } // if (current_segment == TEXT)

        // '.' input, check to see if it is a segment change or
        // if it denotes an entrypoint (".globl main"). If not,
        // check to see if you are in the data segment and act
        // accordingly depending on the input.
        if (strs[0][0] == '.')
        {
            // Segment change
            if (strs.size() == 1)
            {
                if (strs[0] == ".text")
                {
                    switch (current_segment)
                    {
                        case TEXT: // Ignore
                            break;
                        case DATA:
                            current_segment = TEXT;
                            data_seg_addr = program_counter;
                            program_counter = text_seg_addr;
                            break;
                        case NONE:
                            current_segment = TEXT;
                            program_counter = text_seg_addr;
                            break;
                    }
                }

                else if (strs[0] == ".data")
                {
                    switch (current_segment)
                    {
                        case TEXT:
                            current_segment = DATA;
                            text_seg_addr = program_counter;
                            program_counter = data_seg_addr;
                            break;
                        case DATA: // Ignore
                            break;
                        case NONE:
                            current_segment = DATA;
                            program_counter = data_seg_addr;
                    }
                }
            }

            // ".globl < label > "
            // Simply just put this onto the input list.
            else if (strs.size() == 2 && strs[0] == ".globl")
            {
                if (!valid_label(strs[1]))
                    throw SimulatorError("Invalid label.");
                if (entrypoint_label != "")
                    throw SimulatorError("Entrypoint already set (Duplicate .globl).");

                // Set the entrypoint label.
                entrypoint_label = strs[1];

                return;
            }

            else
            {
                // Move forward prorgam counter based on how much data
                // is input for label computation.
                std::string type = strs[0];
                if (type == ".word")
                {
                    program_counter += (strs.size() - 1) * 4;
                }
                else if (type == ".half")
                {
                    program_counter += (strs.size() - 1) * 2;
                }
                else if (type == ".byte")
                {
                    program_counter += (strs.size() - 1);
                }
                else if (type == ".space")
                {
                    program_counter += std::stoi(strs[1]);
                }
                else if (type == ".asciiz")
                {
                    read_format_ascii_escape_sequences(strs[1]);
                    program_counter += strs[1].size() - 1;
                }
                else if (type == ".ascii")
                {
                    read_format_ascii_escape_sequences(strs[1]);
                    program_counter += strs[1].size() - 2;
                }
                else
                    throw SimulatorError("Unsupported data segment data type.");

                // store data segment input with address 0, address
                // will get worked out in the data storage.
                to_be_encoded.push_back({strs, 0, false, line});
            }
        } // if (strs[0][0] == '.')

        // Handle text segment inputs
        else if (current_segment = TEXT)
        {
            // Grab instruction to be encoded later.
            to_be_encoded.push_back({strs, program_counter, true, line});
            program_counter += 4;
        }

        // If you somehow made it here, you have a very interesting input in the
        // data segment.
        else
            SimulatorError("Invalid data segment input.");
       
    } // if (!strs.empty())

    return;
}


// Read file mode handling of pseudoinstructions
void Simulator::read_handle_pseudo(const Instruction & ins,
                                   const std::vector< std::string > & strs,
                                   const uint32_t & addr,
                                   const unsigned int line,
                                   std::unordered_map< uint32_t, unsigned int > & line_numbers)
{
    uint32_t immediate;
    uint16_t param;

    // Split each pseudoinstruction into its composite
    // instructions and encode them.
    switch (ins)
    {
        case MOVE:
            line_numbers[addr] = line;
            text[(addr - TEXT_START) >> 2] = encode({"addu", strs[1], "$0", strs[2]});
            break;
        case LI:
            immediate = std::stoi(strs[2]);
            param = immediate & 0b1111111111111111;
            text[(addr - TEXT_START - 4) >> 2] = encode({"ori", strs[1], "$0", std::to_string(param)});
            param = immediate >> 16;
            text[(addr - TEXT_START) >> 2] = encode({"lui", strs[1], std::to_string(param)});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            break;
        case LW:
            immediate = labels[strs[2]];
            param = immediate & 0b1111111111111111;
            text[(addr - TEXT_START - 8) >> 2] = encode({"ori", "$1", "$0", std::to_string(param)});
            param = immediate >> 16;
            text[(addr - TEXT_START - 4) >> 2] = encode({"lui", "$1", std::to_string(param)});
            text[(addr - TEXT_START) >> 2] = encode({"lw", strs[1], "$1", "0"});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            line_numbers[addr - 8] = line;
            break;
        case LA:
            immediate = std::stoi(strs[2]);
            param = immediate & 0b1111111111111111;
            text[(addr - TEXT_START - 4) >> 2] = encode({"ori", strs[1], "$0", std::to_string(param)});
            param = immediate >> 16;
            text[(addr - TEXT_START) >> 2] = encode({"lui", strs[1], std::to_string(param)});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            break;
        case BLT:
            text[(addr - TEXT_START - 4) >> 2] = encode({"slt", "$1", strs[1], strs[2]});
            text[(addr - TEXT_START) >> 2] = encode({"bne", "$1", "$0", strs[3]});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            break;
        case BLE:
            text[(addr - TEXT_START - 4) >> 2] = encode({"slt", "$1", strs[2], strs[1]});
            text[(addr - TEXT_START) >> 2] = encode({"beq", "$1", "$0", strs[3]});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            break;
        case BGT:
            text[(addr - TEXT_START - 4) >> 2] = encode({"slt", "$1", strs[2], strs[1]});
            text[(addr - TEXT_START) >> 2] = encode({"bne", "$1", "$0", strs[3]});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            break;
        case BGE:
            text[(addr - TEXT_START - 4) >> 2] = encode({"slt", "$1", strs[1], strs[2]});
            text[(addr - TEXT_START) >> 2] = encode({"beq", "$1", "$0", strs[3]});
            line_numbers[addr] = line;
            line_numbers[addr - 4] = line;
            break;
    }
    
    return;
}

// Format the escape sequences back to normal
// "'\\n'" --> "'\n'"
void Simulator::read_format_ascii_escape_sequences(std::string & s) const
{
    int n = s.size() - 1;
    bool escape_char = false;
    for (int i = 1; i < n; ++i)
    {
        if (!escape_char && s[i] == '\\')
            escape_char = true;

        else if (escape_char)
        {
            escape_char = false;
            switch (s[i])
            {
                case '0':
                    s[i - 1] = '\0';
                    break;
                case '\\':
                    s[i - 1] = '\\';
                    break;
                case 'v':
                    s[i - 1] = '\v';
                    break;
                case 'n':
                    s[i - 1] = '\n';
                    break;
                case 't':
                    s[i - 1] = '\t';
                    break;
                case '\"':
                    s[i - 1] = '\"';
                    break;
            }
            s.erase(i, 1);

            --n;
            --i;
        }
    }
    
    return;
}

////////////////////////////
///// Shared functions /////
////////////////////////////

// Get rid of comments
std::string Simulator::truncate_comments(std::string s) const
{
    unsigned int i = 0;
    unsigned int n = s.size();
    while (i < n && s[i] != '#')
        ++i;
    
    return s.substr(0, i);
}

// Make sure whitespace is uniform for string processing.
std::string Simulator::strip_useless_whitespace(std::string s) const
{
    unsigned int start = 0;
    for (const char & c : s)
    {
        if (c != ' ' && c != '\t')
            break;
        ++start;
    }

    unsigned int n = s.size();
    unsigned int len = n - start;
    for (int i = n - 1; i >= 0; --i)
    {
        if (s[i] != ' ' && s[i] != '\t')
            break;
        --len;
    }

    return s.substr(start, len);
}

std::string Simulator::get_numerical_register_str(std::string s) const
{
    // Only register with more than 2 characters.
    if (s == "zero")
        return "0";
    
    if (s.size() > 2)
        throw SimulatorError("Invalid register.");
    
    bool all_digits = true;
    for (char & c : s)
    {
        if (isalpha(c))
        {
            c = tolower(c);
            all_digits = false;
        }
        
        else if (!isdigit(c))
            throw SimulatorError("Invalid register.");
    }

    if (all_digits)
    {
        if (std::stoi(s) > 31)
            throw SimulatorError("Invalid register.");
        return s;
    }

    const static std::unordered_map<std::string, std::string> numerical_reg_map =
        {
            {"at", "1"},
            {"v0", "2"},
            {"v1", "3"},
            {"a0", "4"},
            {"a1", "5"},
            {"a2", "6"},
            {"a3", "7"},
            {"t0", "8"},
            {"t1", "9"},
            {"t2", "10"},
            {"t3", "11"},
            {"t4", "12"},
            {"t5", "13"},
            {"t6", "14"},
            {"t7", "15"},
            {"s0", "16"},
            {"s1", "17"},
            {"s2", "18"},
            {"s3", "19"},
            {"s4", "20"},
            {"s5", "21"},
            {"s6", "22"},
            {"s7", "23"},
            {"t8", "24"},
            {"t9", "25"},
            {"k0", "26"},
            {"k1", "27"},
            {"gp", "28"},
            {"sp", "29"},
            {"fp", "30"},
            {"ra", "31"},
        };
    
    if (numerical_reg_map.find(s) == numerical_reg_map.end())
        throw SimulatorError("Invalid register.");
    
    return numerical_reg_map.find(s)->second;
}

std::string Simulator::replace_registers(std::string s) const
{
    unsigned int i = 0, n = s.size();
    while (i < n)
    {
        // Find register $ symbol
        if (s[i] == '$')
        {
            ++i;
            unsigned int start = i, len = 0;

            while (i + len < n && isalnum(s[i + len]))
                ++len;

            // Isolate the register name and replace with
            // its numerical representation
            std::string reg = s.substr(i, len);
            reg = get_numerical_register_str(reg);
            s.replace(i, len, reg);
            
            i += reg.size() - 1; // -1 for the increment in the end of the loop.
            n -= len - reg.size();
        }
        ++i;
    }
    
    return s;
}

// Split the immediate and register values on instructions
// like "lw $a0, 12($a0)"
// "4($31)" --> { "$31", "4" }.
std::vector< std::string > Simulator::split_immediate_and_register(const std::string & s) const
{
    std::vector< std::string > ret = { "", "" };

    unsigned int i = 1;
    for (const char & c : s)
    {
        switch (c)
        {
            case '(':
                --i;
                break; // Switch to other string.
            case ')':
                break; // Final character, do nothing.
            default:
                ret[i].push_back(c);
        }
    }
    
    return ret;
}

// Break up the input into space separated strings
std::vector< std::string > Simulator::split_input(const std::string & s) const
{
    std::vector< std::string > ret = { "" }; // There will be at least one.

    unsigned int n = s.size();
    unsigned int i = 0;

    // Should ALWAYS be an argument in front.
    while (s[i] != ' ' && s[i] != '\t' && i < n)
        ret[0].push_back(s[i++]);

    bool looking_for_arg = false;
    bool has_parenthases = false;
    while (i < n)
    {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == ',')
        {
            ret.push_back("");
            while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == ','))
                ++i;
            
            if (i >= n)
                break;

            // If it is a character value or a string value.
            if (s[i] == '\"' || s[i] == '\'')
            {
                char search = s[i++];
                ret.back().push_back(search);
                
                // Find matching symbol.
                char escape_sequence = false;
                while (escape_sequence || s[i] != search)
                {
                    if (!escape_sequence && s[i] == '\\')
                        escape_sequence = true;
                    else
                        escape_sequence = false;
                    
                    ret.back().push_back(s[i++]);
                }
                ret.back().push_back(s[i++]);
            }

            // Not a string or character.
            else
            {
                while (i < n && s[i] != ' ' && s[i] != '\t' && s[i] != ',')
                {
                    if (s[i] == '(')
                        has_parenthases = true;
                    ret.back().push_back(s[i++]);
                }
            
                if (has_parenthases)
                {
                    has_parenthases = false;
                    std::vector< std::string > split = split_immediate_and_register(ret.back());
                    ret.pop_back();
                    ret.insert(ret.end(), split.begin(), split.end());
                }
            }
            
            --i;
        }
        
        ++i;
    }

    if (ret.back() == "")
        ret.pop_back();

    return ret;
}

// Format immediates into base 10 numerical values
void Simulator::format_immediates(std::vector< std::string > & strs) const
{
    for (std::string & s : strs)
    {
        std::string sub = s.substr(0, 2);

        // Hexidecimal
        if (sub == "0x")
        {
            s = std::to_string(std::stoi(s, 0, 16));
        }

        // Binary
        else if (sub == "0b")
        {
            s = std::to_string(std::stoi(s.substr(2), 0, 2));
        }

        // Character value.
        else if (s[0] == '\'')
        {
            // These will be escape sequences.
            if (s.size() == 4 && s[1] == '\\')
            {
                char c = s[2];
                switch (c)
                {
                    case '0':
                        s = "0";
                        break;
                    case 't':
                        s = "9";
                        break;
                    case 'n':
                        s = "10";
                        break;
                    case 'v':
                        s = "11";
                        break;
                    default:
                        throw SimulatorError("Unsupported character escape sequence.");
                }
            }
            
            // These are normal characters.
            else if (s.size() == 3)
                s = std::to_string((int)(s[1]));

            else
                throw SimulatorError("Invalid character formatting.");
        }
    }
    
    return;
}

// Replace labels with their address values
void Simulator::replace_labels(std::vector< std::string > & strs) const
{
    for (std::string & s : strs)
        if (labels.find(s) != labels.end())
            s = std::to_string(labels.find(s)->second);
    
    return;
}

// Add a piece of data into the data segment
void Simulator::add_to_data_segment(const std::vector< std::string > & strs)
{
    /*
      I will only accept the following types:
      .space
      .word
      .half
      .byte
      .ascii
      .asciiz
    */    
    if (strs[0] == ".space")
    {
        if (strs.size() != 2)
            throw SimulatorError("Invalid .space value formatting.");

        // move data segment address forward by the number of
        // bytes allocated.
        program_counter += std::stoi(strs[1]);
    }
    
    else if (strs[0] == ".word")
    {
        if (strs.size() < 2)
            throw SimulatorError("Invalid .word value formatting.");

        // Store values in data segment and move program_counter
        // forward.
        unsigned int n = strs.size();
        for (unsigned int i = 1; i < n; ++i)
        {
            uint32_t word = std::stoi(strs[i]);
            uint8_t b0 = (word >> 24),
                b1 = (word >> 16) & 0b11111111,
                b2 = (word >> 8) & 0b11111111,
                b3 = word & 0b11111111;
            data[(program_counter++) - DATA_START] = b0;
            data[(program_counter++) - DATA_START] = b1;
            data[(program_counter++) - DATA_START] = b2;
            data[(program_counter++) - DATA_START] = b3;
        }
    }
    
    else if (strs[0] == ".half")
    {
        if (strs.size() < 2)
            throw SimulatorError("Invalid .half value formatting.");

        // Store values in data segment and move program_counter
        // forward.
        unsigned int n = strs.size();
        for (unsigned int i = 1; i < n; ++i)
        {
            uint16_t halfword = std::stoi(strs[i]);
            uint8_t b0 = (halfword >> 8),
                b1 = (halfword >> 16) & 0b11111111;
            data[(program_counter++) - DATA_START] = b0;
            data[(program_counter++) - DATA_START] = b1;
        }
    }
    
    else if (strs[0] == ".byte")
    {
        if (strs.size() < 2)
            throw SimulatorError("Invalid .byte value formatting.");

        // Store values in data segment and move program_counter
        // forward.
        unsigned int n = strs.size();
        for (unsigned int i = 1; i < n; ++i)
        {
            uint8_t byte = std::stoi(strs[i]);
            data[(program_counter++) - DATA_START] = byte;
        }
    }
    
    else if (strs[0] == ".ascii")
    {
        if (strs.size() != 2)
            throw SimulatorError("Invalid .ascii value formatting.");

        // Store characters in data segment and move program_counter
        // forward.
        std::string str = strs[1];
        if (sim_mode)
            read_format_ascii_escape_sequences(str);
        unsigned int n = str.size() - 1;
        for (unsigned int i = 1; i < n; ++i)
            data[(program_counter++) - DATA_START] = (uint8_t)(str[i]);
    }
    
    else if (strs[0] == ".asciiz")
    {
        if (strs.size() != 2)
            throw SimulatorError("Invalid .asciiz value formatting.");

        // Store characters in data segment and move program_counter
        // forward.
        std::string str = strs[1];
        if (sim_mode)
            read_format_ascii_escape_sequences(str);
        unsigned int n = str.size() - 1;
        for (unsigned int i = 1; i < n; ++i)
            data[(program_counter++) - DATA_START] = (uint8_t)(str[i]);
        data[(program_counter++) - DATA_START] = (uint8_t)('\0');
    }

    else
        throw SimulatorError("Unsupported data segment type.");

    return;
}

// Return value of pseudoinstruction if it is one, also returns
// Instruction(0) so it can be used a boolean condition.
// Throws errors if you dont have a defined label included
// with a pseudoinstruction that requires one.
Instruction Simulator::is_pseudo(const std::string & s,
                                 const std::string & label) const
{
    if (s == "move")
        return MOVE;
    else if (s == "li")
        return LI;
    else if (s == "lw")
    {
        if (isdigit(label[0]))
            return Instruction(0);
        
        if (sim_mode && labels.find(label) == labels.end())
            throw SimulatorError("Undefined label.");
        return LW;
    }
    else if (s == "la")
    {
        if (sim_mode && labels.find(label) == labels.end())
            throw SimulatorError("Undefined label.");
        return LA;
    }
    else if (s == "blt")
    {
        if (sim_mode && labels.find(label) == labels.end())
            throw SimulatorError("Undefined label.");
        return BLT;
    }
    else if (s == "ble")
    {
        if (sim_mode && labels.find(label) == labels.end())
            throw SimulatorError("Undefined label.");
        return BLE;
    }
    else if (s == "bgt")
    {
        if (sim_mode && labels.find(label) == labels.end())
            throw SimulatorError("Undefined label.");
        return BGT;
    }
    else if (s == "bge")
    {
        if (sim_mode && labels.find(label) == labels.end())
            throw SimulatorError("Undefined label.");
        return BGE;
    }
    
    return Instruction(0);
}

// Simulator mode handling of pseudoinstructions.
void Simulator::handle_pseudo(const Instruction & ins,
                              const std::vector< std::string > & strs)
{
    uint32_t immediate;
    uint16_t param;
    
    switch (ins)
    {
        case MOVE:
            if (strs.size() != 3)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            interpret_input("addu " + strs[1] + ", $0, " + strs[2]);
            break;
        case LI:
            if (strs.size() != 3)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            immediate = std::stoi(strs[2]);
            // Bit pattern exceedes 32 bits.
            if (immediate > 0xffff || immediate < 0)
            {
                param = immediate & 0b1111111111111111;
                interpret_input("ori " + strs[1] + ", $0, " + std::to_string(param));
                param = immediate >> 16;
                interpret_input("lui " + strs[1] + ", " + std::to_string(param));
            }
            else
            {
                interpret_input("ori " + strs[1] + ", $0, " + std::to_string(immediate));
            }
            
            break;
        case LW:
            if (strs.size() != 3)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");

            interpret_input("la $at, " + strs[2]);
            interpret_input("lw " + strs[1] + ", 0($at)");
            break;
        case LA:
            if (strs.size() != 3)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            immediate = labels[strs[2]];
            param = immediate & 0b1111111111111111;
            interpret_input("ori " + strs[1] + ", $0, " + std::to_string(param));
            param = immediate >> 16;
            interpret_input("lui " + strs[1] + ", " + std::to_string(param));
            break;
        case BLT:
            if (strs.size() != 4)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            interpret_input("slt $at, " + strs[1] + ", " + strs[2]);
            interpret_input("bne $at, $0, " + strs[3]);
            break;
        case BLE:
            if (strs.size() != 4)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            interpret_input("slt $at, " + strs[2] + ", " + strs[1]);
            interpret_input("beq $at, $0, " + strs[3]);
            break;
        case BGT:
            if (strs.size() != 4)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            interpret_input("slt $at, " + strs[2] + ", " + strs[1]);
            interpret_input("bne $at, $0, " + strs[3]);
            break;
        case BGE:
            if (strs.size() != 4)
                throw SimulatorError("Invalid parameters for pseudoinstruction " + strs[0] + ".");
            interpret_input("slt $at, " + strs[1] + ", " + strs[2]);
            interpret_input("beq $at, $0, " + strs[3]);
            break;
    }
    
    return;
}

// get the instruction object from the string that denotes it
Instruction Simulator::get_instruction(const std::string & s) const
{
    const static std::unordered_map< std::string, Instruction > string_instruction_map = {
        { "add",    ADD    },
        { "addi",   ADDI   },
        { "addiu",  ADDIU  },
        { "addu",   ADDU   },
        { "and",    AND    },
        { "andi",   ANDI   },
        { "beq",    BEQ    },
        { "bne",    BNE    },
        { "j",      J      },
        { "jal",    JAL    },
        { "jr",     JR     },
        { "lbu",    LBU    },
        { "lhu",    LHU    },
        { "lui",    LUI    },
        { "lw",     LW     },
        { "nor",    NOR    },
        { "or",     OR     },
        { "ori",    ORI    },
        { "slt",    SLT    },
        { "slti",   SLTI   },
        { "sltiu",  SLTIU  },
        { "sltu",   SLTU   },
        { "sll",    SLL    },
        { "srl",    SRL    },
        { "sb",     SB     },
        { "sc",     SC     },
        { "sh",     SH     },
        { "sw",     SW     },
        { "sub",    SUB    },
        { "subu",   SUBU   },
        { "div",    DIV    },
        { "divu",   DIVU   },
        { "mfhi",   MFHI   },
        { "mflo",   MFLO   },
        { "mult",   MULT   },
        { "multu",  MULTU  },
        { "sra",    SRA    },
        { "sllv",   SLLV   },
        { "srav",   SRAV   },
        { "srlv",   SRLV   },
        { "xor",    XOR    },
        { "xori",   XORI   },
        { "bgtz",   BGTZ   },
        { "blez",   BLEZ   },
        { "jalr",   JALR   },
        { "lb",     LB     },
        { "lh",     LH     },
        { "mthi",   MTHI   },
        { "mtlo",   MTLO   },
        { "syscall", SYSCALL },
        { "seq",    SEQ    },
        { "bgez",   BGEZ   },
        { "bltz",   BLTZ   }
    };

    if (string_instruction_map.find(s) == string_instruction_map.end())
        throw SimulatorError("Unsupported instruction " + s + ", no opcode found.");

    return string_instruction_map.find(s)->second;
}

// Get the opcode of a given instruction for encoding
uint8_t Simulator::get_opcode(const Instruction & instruction) const
{
    switch (instruction)
    {
        case ADD:     return 0b100000;
        case ADDI:    return 0b001000;
        case ADDIU:   return 0b001001;
        case ADDU:    return 0b100001;
        case AND:     return 0b100100;
        case ANDI:    return 0b001100;
        case BEQ:     return 0b000100;
        case BNE:     return 0b000101;
        case J:       return 0b000010;
        case JAL:     return 0b000011;
        case JR:      return 0b001000;
        case LBU:     return 0b100100;
        case LHU:     return 0b100101;
        case LUI:     return 0b001111;
        case LW:      return 0b100011; // PSEUDOINSTRUCTION WHEN SECOND ARGUMENT IS A LABEL.
        case NOR:     return 0b100111;
        case OR:      return 0b100101;
        case ORI:     return 0b001101;
        case SLT:     return 0b101010;
        case SLTI:    return 0b001010;
        case SLTIU:   return 0b001011;
        case SLTU:    return 0b101011;
        case SLL:     return 0b000000;
        case SRL:     return 0b000010;
        case SB:      return 0b101000;
        case SC:      return 0b111000;
        case SH:      return 0b101001;
        case SW:      return 0b101011;
        case SUB:     return 0b100010;
        case SUBU:    return 0b100011;
        case DIV:     return 0b011010;
        case DIVU:    return 0b011011;
        case MFHI:    return 0b010000;
        case MFLO:    return 0b010010;
        case MULT:    return 0b011000;
        case MULTU:   return 0b011001;
        case SRA:     return 0b000011;
        case SLLV:    return 0b000100;
        case SRAV:    return 0b000111;
        case SRLV:    return 0b000110;
        case XOR:     return 0b100110;
        case XORI:    return 0b001110;
        case BGTZ:    return 0b000111;
        case BLEZ:    return 0b000110;
        case BGEZ:    return 0b010100;
        case BLTZ:    return 0b010101;
        case JALR:    return 0b001001;
        case LB:      return 0b100000;
        case LH:      return 0b100001;
        case MTHI:    return 0b010001;
        case MTLO:    return 0b010011;
        case SYSCALL: return 0b001100;
        case SEQ:     return 0b101000;
            
        default:      throw SimulatorError("Invalid Instruction value sent to get_opcode()"); 
    }
}

// Encode the given arguments into a 32 bit integer to be stored
// into the text segment.
uint32_t Simulator::encode(const std::vector< std::string > & args) const
{
    Instruction instruction = get_instruction(args[0]);
    uint8_t opcode = get_opcode(instruction);
    
    uint32_t encoding = 0;
    unsigned int argc = args.size();

    // Determine type of encoding using
    char encoding_type;
    switch (instruction)
    {
        // I-Style OPERATION syntax encodings.
        case LUI: // ONLY 3 ARGS
            if (argc != 3)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            // Put opcode in front of integer.
            encoding = opcode << 26;
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 16;
            encoding |= uint16_t(std::stoi(args[2]));
            break;
        case ADDI:
        case ADDIU:
        case ANDI:
        case LBU:
        case LHU:
        case LW:
        case ORI:
        case SLTI:
        case SLTIU:
        case SB:
        case SC:
        case SH:
        case SW:
        case XORI:
        case LB:
        case LH:  
            if (argc != 4)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            // Put opcode in front of integer.
            encoding = opcode << 26;
            encoding |= uint8_t(std::stoi(args[2].substr(1))) << 21; // rs
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 16; // rt
            encoding |= uint16_t(std::stoi(args[3])); // immediate
            break;
        case BEQ:
        case BNE:
            if (argc != 4)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            // Put opcode in front of integer.
            encoding = opcode << 26;
            encoding |= uint8_t(std::stoi(args[2].substr(1))) << 21; // rs
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 16; // rt
            encoding |= ((uint16_t)(std::stoi(args[3]) - program_counter)) >> 2; // immediate (jump distance)
            break;
        case BGTZ:
        case BLEZ:
        case BGEZ:
        case BLTZ:
            if (argc != 3)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            // Put opcode in front of integer.
            encoding = opcode << 26;
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 21; // rs
            encoding |= ((uint16_t)(std::stoi(args[2]) - program_counter)) >> 2; // immediate (jump distance)
            break;
            
        // R-Style encodings
        case SLL:
        case SRL:
            if (argc != 4)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            encoding |= uint8_t(std::stoi(args[2].substr(1))) << 16;
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 11;
            encoding |= uint8_t(std::stoi(args[3])) << 6; // shamt value
            encoding |= opcode;
            break;
        case ADD:
        case ADDU:
        case AND:
        case NOR:
        case OR:
        case XOR:
        case SLT:
        case SEQ:
        case SLTU:
        case SUB:
        case SUBU:
        case SLLV:
        case SRLV:
        case SRAV:
            if (argc != 4)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            encoding |= uint8_t(std::stoi(args[2].substr(1))) << 21;
            encoding |= uint8_t(std::stoi(args[3].substr(1))) << 16;
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 11;
            encoding |= opcode;
            break;
        case JR:
            if (argc != 2)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 21;
            encoding |= opcode;
            break;
        case DIV:
        case MULT:
        case DIVU:
        case MULTU:
            if (argc != 3)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 21;
            encoding |= uint8_t(std::stoi(args[2].substr(1))) << 16;
            encoding |= opcode;
            break;
        case MTHI:
        case MTLO:
        case MFHI:
        case MFLO:
            if (argc != 2)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            encoding |= uint8_t(std::stoi(args[1].substr(1))) << 11;
            encoding |= opcode;
            break;
            
        // J-Style Encodings
        case J:
        case JAL:
            if (argc != 2)
                throw SimulatorError("Invalid argument count for \"" + args[0] + "\".");
            encoding |= opcode << 26;
            encoding |= (uint32_t(std::stoi(args[1])) >> 2) & 0b000011111111111111111111111111;
            break;

        case SYSCALL:
            encoding = opcode;
            break;
            
        default:
            throw SimulatorError("Invalid instruction type in encode(): " + std::to_string(instruction));
    }
    
    return encoding;
}

// uint8_t opcode to instruction type conversion
Instruction Simulator::get_instruction(const uint8_t target, const bool is_funct) const
{
    // R Style encodings.
    if (is_funct)
    {
        switch (target)
        {
            case 0b100000: return ADD;
            case 0b100001: return ADDU;
            case 0b100100: return AND;
            case 0b001000: return JR;
            case 0b100111: return NOR;
            case 0b100101: return OR;
            case 0b101010: return SLT;
            case 0b101011: return SLTU;
            case 0b000000: return SLL;
            case 0b000010: return SRL;
            case 0b100010: return SUB;
            case 0b100011: return SUBU;
            case 0b011010: return DIV;
            case 0b011011: return DIVU;
            case 0b010000: return MFHI;
            case 0b010010: return MFLO;
            case 0b011000: return MULT;
            case 0b011001: return MULTU;
            case 0b000011: return SRA;
            case 0b000100: return SLLV;
            case 0b000111: return SRAV;
            case 0b000110: return SRLV;
            case 0b100110: return XOR;
            case 0b001100: return SYSCALL;
            case 0b101000: return SEQ;
        }
    }
    
    // I and J style encodings.
    else
    {
        switch (target)
        {
            case 0b001000: return ADDI;
            case 0b001001: return ADDIU;
            case 0b001100: return ANDI;
            case 0b000100: return BEQ;
            case 0b000101: return BNE;
            case 0b000010: return J;
            case 0b000011: return JAL;
            case 0b100100: return LBU;
            case 0b100101: return LHU;
            case 0b001111: return LUI;
            case 0b100011: return LW;
            case 0b001101: return ORI;
            case 0b001010: return SLTI;
            case 0b001011: return SLTIU;
            case 0b101000: return SB;
            case 0b111000: return SC;
            case 0b101001: return SH;
            case 0b101011: return SW;
            case 0b001110: return XORI;
            case 0b000111: return BGTZ;
            case 0b000110: return BLEZ;
            case 0b010100: return BGEZ;
            case 0b010101: return BLTZ;
            case 0b100000: return LB;
            case 0b100001: return LH;
        }
    }

    throw SimulatorError("Unsupported target encoding in get_instruction().");
}

// Execute an instruction using the array of method
// pointers.
void Simulator::execute(const uint32_t & encoded)
{
    /*
      THE FOLLOWING WILL RUN THE ins_add EXECUTION
      FUNCTION WITH A PARAMETER OF 0:
      
      (this->*ins_executions[ADD])(0);
    */
    bool is_funct;

    uint8_t opcode = encoded >> 26;

    is_funct = (opcode == 0);

    // get funct value if necessary.
    if (is_funct)
        opcode = ((1 << 6) - 1) & encoded;

    Instruction ins = get_instruction(opcode, is_funct);

    // Execute the correct instruction using the method pointer array.
    (this->*ins_executions[ins])(encoded);
    
    return;
}

// Given an address, determine which segment it is located in,
// and using that information also get the starting index
// of the location to handle data, heap, and stack segments
// correctly.
void Simulator::get_location_and_start(const uint32_t & addr,
                                       uint8_t * & location,
                                       unsigned int & start)
{
    // Data segment
    if (addr >= DATA_START && addr < DATA_START + DATA_SEGMENT_SIZE)
    {
        location = data;
        start = DATA_START;
    }
    
    // NOTE, HERE I ASSUME THAT THE STACK AND HEAP EACH
    // TAKE UP EXACTLY HALF OF THE BLOCK THEY SHARE.

    // Heap
    else if (addr >= HEAP_START && addr < HEAP_START + MAX_HEAP)
    {
        location = heap;
        start = HEAP_START;
    }

    // Stack
    else if (addr >= STACK_START && addr < STACK_START + MAX_STACK)
    {
        location = stack;
        start = STACK_START;
    }
    
    else
        throw SimulatorError("Invalid store/load location.");

    return;
}

/////////////////////////////////////////
/// Instruction execution defenitions ///
/////////////////////////////////////////

void Simulator::ins_add(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] + regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_addi(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    regs[rt] = regs[rs] + immediate;

    program_counter += 4;

    return;
}

void Simulator::ins_addiu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    regs[rt] = (unsigned int)(regs[rs] + immediate);

    program_counter += 4;
}

void Simulator::ins_addu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    
    regs[rd] = (unsigned int)(regs[rs] + regs[rt]);

    program_counter += 4;

    return;
}

void Simulator::ins_and(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] & regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_andi(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    regs[rt] = regs[rs] & immediate;

    program_counter += 4;

    return;
}

void Simulator::ins_beq(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t jump_distance = (encoded & 0b1111111111111111) << 2;
    
    if (regs[rt] == regs[rs])
        program_counter += jump_distance;
    else
        program_counter += 4;

    return;
}

void Simulator::ins_bne(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t jump_distance = (encoded & 0b1111111111111111) << 2;

    if (regs[rt] != regs[rs])
        program_counter += jump_distance;
    else
        program_counter += 4;
        
    return;
}

void Simulator::ins_j(const uint32_t & encoded)
{
    uint32_t addr = (encoded & 0b11111111111111111111111111) << 2;

    program_counter = addr;

    return;
}

void Simulator::ins_jal(const uint32_t & encoded)
{
    uint32_t addr = (encoded & 0b11111111111111111111111111) << 2;

    regs[31] = program_counter + 4; // $ra
    program_counter = addr;

    return;
}

void Simulator::ins_jr(const uint32_t & encoded)
{
    uint8_t rs = (encoded >> 21) & 0b11111;

    program_counter = regs[rs];
    
    return;
}

void Simulator::ins_lbu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    uint32_t val = location[addr - location_start];
    
    regs[rt] = val;

    program_counter += 4;

    return;
}

void Simulator::ins_lhu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    uint32_t val = location[addr - location_start] << 8;
    val |= location[addr - location_start + 1];
    
    regs[rt] = val;

    program_counter += 4;

    return;
}

void Simulator::ins_lui(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint16_t immediate = encoded & 0b1111111111111111;

    regs[rt] &= 0b00000000000000001111111111111111;
    regs[rt] |= immediate << 16;

    program_counter += 4;

    return;
}

void Simulator::ins_lw(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    uint32_t val = location[addr - location_start] << 24;
    val |= location[addr - location_start + 1] << 16;
    val |= location[addr - location_start + 2] << 8;
    val |= location[addr - location_start + 3];
    
    regs[rt] = val;

    program_counter += 4;

    return;
}

void Simulator::ins_nor(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = ~(regs[rs] | regs[rt]);

    program_counter += 4;

    return;
}

void Simulator::ins_or(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] | regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_ori(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    regs[rt] = regs[rs] | immediate;
    
    program_counter += 4;
    
    return;
}

void Simulator::ins_slt(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] < regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_slti(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    regs[rt] = regs[rs] < immediate;

    program_counter += 4;
    
    return;
}

void Simulator::ins_sltiu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    regs[rt] = regs[rs] < immediate;

    program_counter += 4;
    
    return;
}

void Simulator::ins_sltu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] < regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_sll(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t shamt = (encoded >> 6) & 0b11111;

    regs[rd] = regs[rt] << shamt;

    program_counter += 4;

    return;
}

void Simulator::ins_srl(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t shamt = (encoded >> 6) & 0b11111;

    regs[rd] = regs[rt] >> shamt;

    program_counter += 4;

    return;
}

void Simulator::ins_sb(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    location[addr - location_start] = regs[rt] & 0b11111111;
    
    program_counter += 4;
    
    return;
}

void Simulator::ins_sc(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    // For a conditional, should only be 0 or 1, so only look at first bit.
    location[addr - location_start] = regs[rt] & 0b1;
    
    program_counter += 4;
    
    return;
}

void Simulator::ins_sh(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    location[addr - location_start] = (regs[rt] >> 8) & 0b11111111;
    location[addr - location_start + 1] = regs[rt] & 0b11111111;
    
    program_counter += 4;
    
    return;
}

void Simulator::ins_sw(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;
    
    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);
    
    location[addr - location_start] = regs[rt] >> 24;
    location[addr - location_start + 1] = (regs[rt] >> 16) & 0b11111111;
    location[addr - location_start + 2] = (regs[rt] >> 8) & 0b11111111;
    location[addr - location_start + 3] = regs[rt] & 0b11111111;
    
    program_counter += 4;
    
    return;
}

void Simulator::ins_sub(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] - regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_subu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] - regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_div(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs.hi() = regs[rs] % regs[rt];
    regs.lo() = regs[rs] / regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_divu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs.hi() = regs[rs] % regs[rt];
    regs.lo() = regs[rs] / regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_mfhi(const uint32_t & encoded)
{
    uint8_t rd = (encoded >> 11) & 0b11111;

    regs[rd] = regs.hi();

    program_counter += 4;

    return;
}

void Simulator::ins_mflo(const uint32_t & encoded)
{
    uint8_t rd = (encoded >> 11) & 0b11111;

    regs[rd] = regs.lo();

    program_counter += 4;

    return;
}

void Simulator::ins_mult(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    uint64_t prod = regs[rs] * regs[rt];
    regs.hi() = prod >> 32;
    regs.lo() = prod & 0xffffffff;

    program_counter += 4;

    return;
}

void Simulator::ins_multu(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    uint64_t prod = regs[rs] * regs[rt];
    regs.hi() = prod >> 32;
    regs.lo() = prod & 0xffffffff;

    program_counter += 4;

    return;
}

void Simulator::ins_sra(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t shamt = (encoded >> 6) & 0b11111;

    regs[rd] = regs[rt] >> shamt;

    program_counter += 4;

    return;
}

void Simulator::ins_sllv(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] << regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_srav(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] >> regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_srlv(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] >> regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_xor(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] ^ regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_xori(const uint32_t & encoded)
{
   uint8_t rt = (encoded >> 16) & 0b11111;
   uint8_t rs = (encoded >> 21) & 0b11111;
   int16_t immediate = encoded & 0b1111111111111111;

   regs[rt] = regs[rs] ^ immediate;

   program_counter += 4;
    
   return;
}

void Simulator::ins_bgtz(const uint32_t & encoded)
{
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t jump_distance = (encoded & 0b1111111111111111) << 2;
    
    if (regs[rs] > 0)
        program_counter += jump_distance;
    else
        program_counter += 4;

    return;
}

void Simulator::ins_blez(const uint32_t & encoded)
{
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t jump_distance = (encoded & 0b1111111111111111) << 2;
    
    if (regs[rs] <= 0)
        program_counter += jump_distance;
    else
        program_counter += 4;

    return;
}

void Simulator::ins_jalr(const uint32_t & encoded)
{
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[31] = program_counter + 4; // $ra
    program_counter = regs[rs];
    
    return;
}

void Simulator::ins_lb(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    uint32_t val = location[addr - location_start];
    
    regs[rt] = val;

    program_counter += 4;

    return;
}

void Simulator::ins_lh(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t immediate = encoded & 0b1111111111111111;

    uint32_t addr = regs[rs] + immediate;
    uint8_t * location;
    unsigned int location_start;
    get_location_and_start(addr, location, location_start);

    uint32_t val = location[addr - location_start] << 8;
    val |= location[addr - location_start + 1];
    
    regs[rt] = val;

    program_counter += 4;

    return;
}

void Simulator::ins_mthi(const uint32_t & encoded)
{
    uint8_t rd = (encoded >> 11) & 0b11111;

    regs.hi() = regs[rd];

    program_counter += 4;

    return;
}

void Simulator::ins_mtlo(const uint32_t & encoded)
{
    uint8_t rd = (encoded >> 11) & 0b11111;

    regs.lo() = regs[rd];

    program_counter += 4;

    return;
}

void Simulator::ins_syscall(const uint32_t & encoded)
{
    switch (regs[2]) // $v0
    {
        // PRINT INT
        case 1:
            // Print $a0 as integer.
            std::cout << int(regs[4]);
            break;
        // PRINT STRING
        case 4:
        {
            uint32_t addr = regs[4];
            uint8_t * location;
            unsigned int location_start;
            get_location_and_start(addr, location, location_start);

            unsigned int i = 0;
            while (location[addr - location_start + i] != '\0')
                std::cout << location[addr - location_start + (i++)];
            
            break;
        }
        // READ INT
        case 5: // Input integer into $a0
        {
            std::string input;
            std::getline(std::cin, input);
            regs[2] = std::stoi(input);
            break;
        }
        // READ STRING
        case 8:
        {
            std::string input;
            std::getline(std::cin, input);
            input.push_back('\n'); // Mips appends a newline character.
            
            uint32_t addr = regs[4]; // $a0
            uint32_t max_chars = (regs[5] > input.size() ? input.size() : regs[5]);; // $a1
            uint8_t * location;
            unsigned int location_start;
            get_location_and_start(addr, location, location_start);

            unsigned int i = 0;
            while (i < max_chars)
                location[addr - location_start + (i++)] = input[i];
            
            break;
        }
        // ALLOCATE HEAP MEMORY
        case 9:
            // Give first address of bytes allocated to $v0.
            regs[2] = heap_ptr;
            heap_ptr += regs[4]; // Move heap pointer forward by the
                                 // amount of bytes denoted by $a0.

            break;
        case 10:
            running = false;
            std::cout << "Simulator exiting..." << std::endl;
            break;
        case 11: // Print $a0 as a character.
            std::cout << char(regs[4]);
            break;
        default:
            throw SimulatorError("Undefined syscall $v0 value.");
    }

    program_counter += 4;

    return;
}

void Simulator::ins_seq(const uint32_t & encoded)
{
    uint8_t rt = (encoded >> 16) & 0b11111;
    uint8_t rd = (encoded >> 11) & 0b11111;
    uint8_t rs = (encoded >> 21) & 0b11111;

    regs[rd] = regs[rs] == regs[rt];

    program_counter += 4;

    return;
}

void Simulator::ins_bgez(const uint32_t & encoded)
{
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t jump_distance = (encoded & 0b1111111111111111) << 2;
    
    if (regs[rs] >= 0)
        program_counter += jump_distance;
    else
        program_counter += 4;

    return;
}

void Simulator::ins_bltz(const uint32_t & encoded)
{
    uint8_t rs = (encoded >> 21) & 0b11111;
    int16_t jump_distance = (encoded & 0b1111111111111111) << 2;
    
    if (regs[rs] < 0)
        program_counter += jump_distance;
    else
        program_counter += 4;

    return;
}
