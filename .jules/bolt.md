
## 2024-05-18 - [Optimize Libretro Shader Menu Search]
**Learning:** Found an O(N*M) case-insensitive manual substring search inside a render loop for a UI menu that allocated and transformed `std::string` every frame. We can use SDL's native optimized `SDL_strcasestr` to perform this matching with zero allocations and roughly 2x performance.
**Action:** Replaced manual string matching in `src/port/sdl/shader_menu.cpp` with `SDL_strcasestr`. Removed the `std::string` copies and `std::transform` calls. Kept the diff completely atomic without reformatting the entire file.
