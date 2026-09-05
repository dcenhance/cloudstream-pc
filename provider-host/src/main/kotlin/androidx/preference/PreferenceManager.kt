@file:Suppress("unused")

package androidx.preference

import android.content.Context
import android.content.SharedPreferences

class PreferenceManager private constructor() {
    companion object {
        @JvmStatic
        fun getDefaultSharedPreferences(context: Context): SharedPreferences =
            context.getSharedPreferences("default_preferences", Context.MODE_PRIVATE)
    }
}
