#include <iostream>
#include <ASGE/Core/Formats/XML/XML.hpp>

int main()
{
    asge::filesystem::Path xmlPath( XML_PATH );
    
    auto result = asge::formats::xml::Parse( xmlPath );
    if ( !result )
    {
        result.LogError();
        return 1;
    }

    return 0;
}