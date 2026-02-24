#include "imgui.h"

extern "C" {
    void TestHelper_CreateImGuiContext() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1920.0f, 1080.0f);
        io.DeltaTime = 1.0f / 60.0f; // Default
        
        // Build atlas
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    void TestHelper_SetDeltaTime(float dt) {
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = dt;
    }

    void TestHelper_NewFrame() {
        ImGui::NewFrame();
    }

    void TestHelper_EndFrame() {
        ImGui::EndFrame();
    }

    void TestHelper_DestroyImGuiContext() {
        ImGui::DestroyContext();
    }
}
