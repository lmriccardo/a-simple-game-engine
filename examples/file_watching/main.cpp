#include <iostream>
#include <ASGE/Core/Filesystem/FileWatcher.hpp>

using namespace asge::filesystem;

int main()
{
    FileWatcher fw;
    fw.Start();
    
    auto result = fw.AddWatch( 
        "C:\\Users\\ricca\\Desktop\\dev\\asge\\examples\\file_watching\\files1",
        [&fw]( FileEvent const& inEvent )
        {
            std::cout << inEvent << std::endl;
            fw.Cancel();
        }
    );

    if (!result)
    {
        auto const err = result.Error();
        LOG_ERROR("Error adding watcher: ", err);
        fw.Cancel();
        fw.Join();
        return 1;
    }
    
    fw.Join();

    return 0;
}