#include <iostream>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>

using namespace asge::config::toml;

int main()
{
    std::string toml = "x = 1\ny = 2\nz=[1,2,3,4]\nt=[[2,3,4], [5,6]]";

    auto table = _internal::Parse(toml);
    std::cout << table << std::endl;

    return 0;
}