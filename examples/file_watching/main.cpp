#include <iostream>
#include <ASGE/Core/Filesystem/FileWatcher.hpp>

using namespace asge::filesystem;

int main()
{
    _win32::FileWatcher fw;
    fw.Start();
    
    fw.AddWatch( 
        "C:\\Users\\ricca\\Desktop\\dev\\asge\\examples\\file_watching\\files",
        [&fw]( FileEvent const& inEvent )
        {
            std::cout << inEvent << std::endl;
            fw.Cancel();
        }
    );

    fw.Join();

    return 0;
}