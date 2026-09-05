@file:Suppress("unused")

package androidx.fragment.app

import android.app.Activity
import java.io.File

open class FragmentManager {
    open val fragments: List<Fragment> = emptyList()
    open fun beginTransaction(): FragmentTransaction = FragmentTransaction()
}

open class FragmentActivity(preferencesFile: File? = null) : Activity(preferencesFile) {
    open val supportFragmentManager: FragmentManager = FragmentManager()
}
