#include <iostream>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>

using namespace asge::config::_internal;

std::string toml_str = 
    "# Root level keys\n"
    "root_key = 1\n"
    "\n"
    "# Standard table\n"
    "[server]\n"
    "host = \"localhost\"\n"
    "port = 8080\n"
    "\n"
    "# Nested standard table\n"
    "[server.tls]\n"
    "enabled = true\n"
    "cert    = \"/etc/ssl/cert.pem\"\n"
    "\n"
    "# Another top-level table\n"
    "[database]\n"
    "host = \"db.local\"\n"
    "port = 5432\n"
    "name = \"mydb\"\n";

int main()
{
    auto table = toml::Parse(toml_str);
    std::cout << table << std::endl;
    table->Get<std::string>( "root_ke" );
    table->Get<std::string>( "root_key" );
    std::cout << *table->Get<int>( "root_key" ) << std::endl;

    return 0;
}