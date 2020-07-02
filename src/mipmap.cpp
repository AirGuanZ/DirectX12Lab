#include <avir.h>

#include <agz/d3d12/texture/mipmap.h>

AGZ_D3D12_BEGIN

namespace
{

    template<typename Texel, typename Comp, int N>
    texture::texture2d_t<Texel> generateNextMipmapLevel(
        const texture::texture2d_t<Texel> &initalLevel,
        const texture::texture2d_t<Texel> &lastLevel)
    {
        static_assert(std::is_standard_layout_v<Texel>);
        static_assert(sizeof(Texel) == sizeof(Comp) * N);

        if(lastLevel.width() == 1 && lastLevel.height() == 1)
            return {};

        const int newWidth  = (std::max)(1, lastLevel.width() / 2);
        const int newHeight = (std::max)(1, lastLevel.height()  / 2);
        texture::texture2d_t<Texel> output(newHeight, newWidth);

        avir::CImageResizer imageResizer(8);
        imageResizer.resizeImage(
            reinterpret_cast<const Comp *>(initalLevel.raw_data()),
            initalLevel.width(), initalLevel.height(), 0,
            reinterpret_cast<Comp *>(output.raw_data()),
            newWidth, newHeight, N, 0);

        return output;
    }

    template<typename Texel, typename Comp, int N>
    std::vector<texture::texture2d_t<Texel>> constructMipmapChainImpl(
        texture::texture2d_t<Texel> lod0Data, int maxChainLength)
    {
        std::vector<texture::texture2d_t<Texel>> ret;
        ret.push_back(std::move(lod0Data));

        while(maxChainLength < 0 ||
              ret.size() < static_cast<size_t>(maxChainLength))
        {
            auto nextLOD = generateNextMipmapLevel<Texel, Comp, N>(
                ret.front(), ret.back());
            if(!nextLOD.is_available())
                break;
            ret.push_back(std::move(nextLOD));
        }

        return ret;
    }

} // namespace anonymous

int computeMaxMipmapChainLength(int width, int height) noexcept
{
    int ret = 1;
    while(width > 1 || height > 1)
    {
        width  >>= 1;
        height >>= 1;
        ++ret;
    }
    return ret;
}

std::vector<texture::texture2d_t<math::color4b>> constructMipmapChain(
    texture::texture2d_t<math::color4b> lod0Data, int maxChainLength)
{
    return constructMipmapChainImpl<math::color4b, uint8_t, 4>(
        lod0Data, maxChainLength);
}

AGZ_D3D12_END
