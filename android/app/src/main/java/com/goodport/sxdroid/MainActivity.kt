package com.goodport.sxdroid

import android.os.Bundle
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.libsdl.app.SDLActivity

class MainActivity : SDLActivity() {
    override fun getLibraries(): Array<String> {
        return arrayOf(
            "main"
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        // Force focus on the SDL view. This is crucial for handling keyboard/gamepad input.
        mSurface?.isFocusable = true
        mSurface?.isFocusableInTouchMode = true
        mSurface?.requestFocus()
        hideSystemUI()
    }

    private fun hideSystemUI() {
        val windowInsetsController = WindowCompat.getInsetsController(window, window.decorView)

        windowInsetsController?.apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUI()
        }
    }

    companion object {
        @Volatile
        var finished = false

        @JvmStatic
        fun showRendererChooserDialog(): Int {
            var selectedResult = -1
            finished = false
            
            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) return 3 // Fallback to classic

            activity.runOnUiThread {
                val items = arrayOf("GPU (Vulkan)", "OpenGL", "SDL2D (Alternative)", "SDL2D (Classic)")
                var tempSelection = 3 // default to Classic
                
                val builder = android.app.AlertDialog.Builder(activity)
                builder.setTitle("Select Renderer Backend")
                builder.setCancelable(false)
                builder.setSingleChoiceItems(items, tempSelection) { _, which ->
                    tempSelection = which
                }
                builder.setPositiveButton("Confirm") { _, _ ->
                    selectedResult = tempSelection
                    finished = true
                }
                builder.show()
            }
            
            // Block the SDL thread (which is separate from the UI thread) until the user finishes
            while (!finished) {
                Thread.sleep(50)
            }
            return selectedResult
        }
    }
}
