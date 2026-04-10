#include "RmlUi_Renderer_GCM.h"
#include <stdio.h>

RmlUi_Renderer_GCM::RmlUi_Renderer_GCM() {}
RmlUi_Renderer_GCM::~RmlUi_Renderer_GCM() {}

void RmlUi_Renderer_GCM::RenderGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture, const Rml::Vector2f& translation) {
    // TODO: implement cellGcmSetDrawElements mapping
}

Rml::CompiledGeometryHandle RmlUi_Renderer_GCM::CompileGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture) {
    return 0; // Returning 0 means RmlUi will fallback to RenderGeometry
}

void RmlUi_Renderer_GCM::RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation) {
}

void RmlUi_Renderer_GCM::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) {
}

void RmlUi_Renderer_GCM::EnableScissorRegion(bool enable) {
    // TODO: map to cellGcmSetScissor(enable ? current_rect : full_screen)
}

void RmlUi_Renderer_GCM::SetScissorRegion(int x, int y, int width, int height) {
    // TODO: map to cellGcmSetScissor
}

bool RmlUi_Renderer_GCM::LoadTexture(Rml::TextureHandle& texture_handle, Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    return false;
}

bool RmlUi_Renderer_GCM::GenerateTexture(Rml::TextureHandle& texture_handle, const Rml::byte* source, const Rml::Vector2i& source_dimensions) {
    return false;
}

void RmlUi_Renderer_GCM::ReleaseTexture(Rml::TextureHandle texture_handle) {
}

void RmlUi_Renderer_GCM::SetTransform(const Rml::Matrix4f* transform) {
}
