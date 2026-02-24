import os
import subprocess
import sys
import shutil

# Paths
SHADER_SRC_DIR = "src/shaders"
SHADER_OUT_DIR = "src/shaders"
GLSLC = "glslc"

# Shaders to compile
SHADERS = [
    ("scene.gpu.vert", "vert.spv", "vertex"),
    ("scene_array.gpu.frag", "scene.spv", "fragment"),
    ("text.gpu.vert", "text.vert.spv", "vertex"),
    ("text.gpu.frag", "text.frag.spv", "fragment"),
    ("rect.gpu.vert", "rect.vert.spv", "vertex"),
    ("rect.gpu.frag", "rect.frag.spv", "fragment"),
    ("blit.gpu.vert", "blit.vert.spv", "vertex"),
    ("blit.gpu.frag", "blit.frag.spv", "fragment"),
    ("palette_convert.gpu.comp", "palette_convert.comp.spv", "compute"),
]


def find_glslc():
    """Finds the glslc executable."""
    path = shutil.which(GLSLC)
    if path:
        return path

    # Try common locations if not in PATH (e.g. Vulkan SDK)
    if sys.platform == "win32":
        vulkan_sdk = os.environ.get("VULKAN_SDK")
        if vulkan_sdk:
            path = os.path.join(vulkan_sdk, "Bin", "glslc.exe")
            if os.path.exists(path):
                return path
    elif sys.platform == "darwin":
        # macOS specific locations could be added here
        pass

    return None


def compile_shader(src_file, out_file, stage):
    """Compiles a shader using glslc."""
    src_path = os.path.join(SHADER_SRC_DIR, src_file)
    out_path = os.path.join(SHADER_OUT_DIR, out_file)

    if not os.path.exists(src_path):
        print(f"Error: Shader source not found: {src_path}")
        return False

    cmd = [GLSLC, "-fshader-stage=" + stage, src_path, "-o", out_path]

    print(f"Compiling {src_file} -> {out_file}...")
    try:
        subprocess.check_call(cmd)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error compiling {src_file}: {e}")
        return False
    except OSError as e:
        print(f"Error executing glslc: {e}")
        return False


def main():
    global GLSLC
    glslc_path = find_glslc()
    if not glslc_path:
        print("Error: glslc not found. Please install the Vulkan SDK.")
        sys.exit(1)

    GLSLC = glslc_path
    print(f"Using glslc: {GLSLC}")

    success = True
    for src, out, stage in SHADERS:
        if not compile_shader(src, out, stage):
            success = False

    if success:
        print("All shaders compiled successfully.")
    else:
        print("Shader compilation failed.")
        sys.exit(1)


if __name__ == "__main__":
    main()
