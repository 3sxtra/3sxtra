#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <cell/gcm.h>

class RmlUi_Renderer_GCM : public Rml::RenderInterface {
  public:
    RmlUi_Renderer_GCM();
    virtual ~RmlUi_Renderer_GCM();

    void RenderGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices,
                        Rml::TextureHandle texture, const Rml::Vector2f& translation) override;

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices,
                                                Rml::TextureHandle texture) override;
    void RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation) override;
    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(int x, int y, int width, int height) override;

    bool LoadTexture(Rml::TextureHandle& texture_handle, Rml::Vector2i& texture_dimensions,
                     const Rml::String& source) override;
    bool GenerateTexture(Rml::TextureHandle& texture_handle, const Rml::byte* source,
                         const Rml::Vector2i& source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

    void SetTransform(const Rml::Matrix4f* transform) override;
};
