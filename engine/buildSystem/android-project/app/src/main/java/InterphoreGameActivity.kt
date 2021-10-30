package com.paraphore.interphore

import org.libsdl.app.SDLActivity

class InterphoreGameActivity : SDLActivity() {
    override fun getLibraries(): Array<String> {
        return arrayOf(
            //"SDL2",
            "interphore"
        )
    }
}