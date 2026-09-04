#include <iostream>
#include <filesystem>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Filesystem/Filesystem.hpp>

using namespace asge::media;

int main()
{
    // ASGE_IMAGE_INFO_ASSET_DIR is injected by CMakeLists.txt, so this
    // example doesn't hardcode a path to any one developer's checkout.
    asge::filesystem::Path const imagePath =
        std::filesystem::path(ASGE_IMAGE_INFO_ASSET_DIR) / "checker.bmp";

    auto result = Image::Load(imagePath);
    if (!result)
    {
        result.LogError();
        return 1;
    }

    Image const& image = result.Value();
    auto const dimensions = image.Dimensions();
    auto const* pixels = image.Data();

    std::cout << "Loaded " << imagePath.filename() << "\n"
              << "  Dimensions: " << dimensions.x() << "x" << dimensions.y() << "\n"
              << "  Format:     RGBA8\n"
              << "  Stride:     " << image.Stride() << " bytes/row\n"
              << "  Pixel(0,0): "
              << "r=" << static_cast<int>(pixels[0]) << " "
              << "g=" << static_cast<int>(pixels[1]) << " "
              << "b=" << static_cast<int>(pixels[2]) << " "
              << "a=" << static_cast<int>(pixels[3]) << "\n";

    return 0;
}
