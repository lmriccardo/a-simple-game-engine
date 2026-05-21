#include <iostream>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>

using namespace asge::config;

int main()
{
    std::string toml = "x = 1\ny = 2";

    auto table = _internal::TOML_Parse(toml);
    std::cout << table << std::endl;

    return 0;
}