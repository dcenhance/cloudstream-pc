@file:Suppress("unused")

package com.lagradost.cloudstream3

import android.app.Activity
import android.content.Context
import androidx.appcompat.app.AppCompatActivity
import java.util.concurrent.ConcurrentHashMap

class CloudStreamApp {
    companion object {
        var context: Context? = AppCompatActivity()
        private val values = ConcurrentHashMap<String, Any>()

        fun <T> setKey(path: String, value: T?) {
            if (value == null) values.remove(path) else values[path] = value
        }

        @Suppress("UNCHECKED_CAST")
        fun <T> getKey(path: String): T? = values[path] as? T

        fun removeKey(path: String) {
            values.remove(path)
        }
    }
}

open class AcraApplication {
    companion object {
        var context: Context? = CloudStreamApp.context
        fun <T> setKey(path: String, value: T?) = CloudStreamApp.setKey(path, value)
        fun <T> getKey(path: String): T? = CloudStreamApp.getKey(path)
    }
}

object CommonActivity {
    var activity: Activity? = null

    fun showToast(activity: Activity?, message: String, duration: Int?) = Unit
    fun showToast(message: String, duration: Int? = null) = Unit
}
