#pragma once

#include <ASGE/Core/Configuration/TOML_Builder.hpp>

namespace asge::game::components
{

template<typename T>
class Serializer
{
    static void ToToml( T, asge::config::TOMLTableView ) noexcept 
    {
        static_assert( false && "Not Implemented" );
    }

    static T FromToml( asge::config::TOMLTableView ) noexcept 
    {
        static_assert( false && "Not Implemented" );
    }
};

}