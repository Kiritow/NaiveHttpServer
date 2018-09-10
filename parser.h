#include <string>

int parse_range_request(const std::string& range, int content_length, int& _out_beginat, int& _out_length);