package com.paraphore.interphore

import android.content.Intent
import android.net.Uri
import org.libsdl.app.SDLActivity
import java.io.InputStreamReader

class InterphoreGameActivity : SDLActivity() {
    override fun getLibraries(): Array<String> = arrayOf("interphore")

    private val createSave = 1
    private val loadSave = 2

    // String of the actual save data, to use when the intent from saveToDisk finishes
    private var lastSaveData: String = ""
    // A number representing the function pointer callback that is called when the game is loaded
    private var lastCallback: Long = 0

    // Save the game, called from C++
    private fun saveToDisk(data: String) {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"

            putExtra(Intent.EXTRA_TITLE, "Interphore.txt")
            lastSaveData = data
        }
        startActivityForResult(intent, createSave)
    }

    // Native function to call the callback
    private external fun loadFromDiskCallback(callback: Long, data: String)
    // Load the game, called from C++
    private fun loadFromDisk(callback: Long) {
        lastCallback = callback

        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"
        }
        startActivityForResult(intent, loadSave);
    }

    private fun gotoUrl(url: String) {
        val view = Intent(Intent.ACTION_VIEW, Uri.parse(url))
        startActivity(view)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == RESULT_OK && data != null)
        when(requestCode) {
            createSave -> contentResolver.openOutputStream(data.data!!).use { outputStream ->
                outputStream!!.write(lastSaveData.toByteArray())
            }
            loadSave -> contentResolver.openInputStream(data.data!!).use { inputStream ->
                InputStreamReader(inputStream!!).use { reader ->
                    loadFromDiskCallback(lastCallback, reader.readText())
                }
            }
        }
    }
}