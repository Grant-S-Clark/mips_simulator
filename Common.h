//   File: Common.h
// Author: Grant Clark
//   Date: 12/17/2023

#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstddef>
#include <cctype>
#include <unordered_map>
#include <fstream>

class SimulatorError
{
public:
    SimulatorError(const std::string & msg) :
        what_(msg)
    {}

    virtual
    ~SimulatorError(){};
    
    std::string what() const
    { return what_; }
private:
    std::string what_;
};

template< typename T >
std::ostream & operator<<(std::ostream & cout,
                          const std::vector< T > & v)
{
    cout << '[';
    std::string delim = "";
    for (const T & val : v)
    {
        cout << delim << val;
        delim = ", ";
    }
    cout << ']';
    
    return cout;
}

#endif
