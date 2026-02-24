#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
extern "C" {
#include <cmocka.h>
}
#include <stdbool.h>
#include <stdio.h>
#include "port/imgui_font.h"
#include "imgui.h"

// CMocka tests
extern "C" {

static void test_imgui_font_init(void **state) {
    (void) state;
    
    // Setup: Create ImGui Context
    ImGui::CreateContext();
    
    // Execute: Init
    // Should return true now that we have a context
    bool result = ImGuiFont_Init();
    assert_true(result);
    
    // Teardown
    ImGui::DestroyContext();
}

static void test_imgui_font_init_no_context(void **state) {
    (void) state;
    
    // Ensure no context exists
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }
    
    // Execute: Init without context
    // Should return false
    bool result = ImGuiFont_Init();
    assert_false(result);
}

static void test_imgui_font_load_failure(void **state) {
    (void) state;
    
    ImGui::CreateContext();
    
    // Execute: Load non-existent font
    bool result = ImGuiFont_LoadJapaneseFont("non_existent_file.ttf", 18.0f);
    assert_false(result);
    
    ImGui::DestroyContext();
}

static void test_imgui_font_load_success(void **state) {
    (void) state;
    
    ImGui::CreateContext();
    
    // Execute: Load existing font (using BoldPixels as a proxy for a valid TTF)
    // Note: This relies on the file existing at this path.
    // In a real CI env, we should use a test asset.
    const char* font_path = "assets/BoldPixels.ttf";
    
    // We expect this to might fail if the file isn't found by the test runner,
    // but the code execution path is what we are testing.
    // However, without a guarantee of the file's presence relative to the binary,
    // we will just check that the function doesn't crash.
    
    // Check if file exists first to avoid false negative
    FILE* f = fopen(font_path, "rb");
    if (f) {
        fclose(f);
        bool result = ImGuiFont_LoadJapaneseFont(font_path, 18.0f);
        // If file exists, it should likely succeed unless ImGui freaks out about Japanese glyph ranges on a non-JP font
        // But AddFontFromFileTTF usually just loads what it can.
        assert_true(result);
    } else {
        printf("Skipping load success test: Test font not found at %s\n", font_path);
    }
    
    ImGui::DestroyContext();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_imgui_font_init),
        cmocka_unit_test(test_imgui_font_init_no_context),
        cmocka_unit_test(test_imgui_font_load_failure),
        cmocka_unit_test(test_imgui_font_load_success),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

}