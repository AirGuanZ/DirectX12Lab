#include <agz/utility/image.h>
#include <agz/utility/mesh.h>

#include "./mesh.h"

std::vector<ComPtr<ID3D12Resource>> Mesh::loadFromFile(
    const Window              &window,
    DescriptorSubHeap         &descHeap,
    ID3D12GraphicsCommandList *copyCmdList,
    const std::string         &objFilename,
    const std::string         &albedoFilename)
{
    albedoDescTable_ = descHeap.allocRange(1);

    std::vector<ComPtr<ID3D12Resource>> ret;

    // albedo texture

    const auto imgData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file(albedoFilename));
    if(!imgData.is_available())
    {
        throw std::runtime_error(
            "failed to load image data from " + albedoFilename);
    }

    ret.push_back(albedo_.initializeShaderResource(
        window.getDevice(), DXGI_FORMAT_R8G8B8A8_UNORM,
        imgData.width(), imgData.height(),
        copyCmdList, { imgData.raw_data() }));

    albedo_.createShaderResourceView(albedoDescTable_[0]);

    // constant buffer
    
    vsTransform_.initializeDynamic(window.getDevice(), window.getImageCount());

    // vertex data

    const auto mesh =
        triangle_to_vertex(agz::mesh::load_from_file(objFilename));

    std::vector<Vertex> vertexData;
    std::transform(
        mesh.begin(), mesh.end(), std::back_inserter(vertexData),
        [](const agz::mesh::vertex_t &v)
    {
        return Vertex{ v.position, v.normal, v.tex_coord };
    });

    ret.push_back(vertexBuffer_.initializeStatic(
        window.getDevice(), copyCmdList, vertexData.size(), vertexData.data()));

    return ret;
}

void Mesh::setWorldTransform(const Mat4 &world) noexcept
{
    world_ = world;
}

void Mesh::draw(
    ID3D12GraphicsCommandList *cmdList,
    int                        imageIndex,
    const WalkingCamera       &camera) const
{
    const Mat4 wvp = world_ * camera.getViewProj();
    vsTransform_.updateContentData(imageIndex, { wvp, world_ });

    cmdList->SetGraphicsRootConstantBufferView(
        0, vsTransform_.getGpuVirtualAddress(imageIndex));

    cmdList->SetGraphicsRootDescriptorTable(
        1, albedoDescTable_[0].getGPUHandle());

    const auto vertexBufferView = vertexBuffer_.getView();
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

    cmdList->DrawInstanced(UINT(vertexBuffer_.getVertexCount()), 1, 0, 0);
}
