@file:Suppress("unused")

package android.content.pm

import android.content.Intent

open class ApplicationInfo {
    var sourceDir: String? = null
    var publicSourceDir: String? = null
    var packageName: String? = null
}

open class PackageManager {
    open fun getLaunchIntentForPackage(packageName: String): Intent? = null
}
