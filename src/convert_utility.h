#ifndef _CONVERT_UTILITY_H
#define _CONVERT_UTILITY_H
#include <string>
std::string to_hex(const std::string& data, int len);
std::string from_hex(const std::string& data);
int hex_to_val(char c);
#endif
