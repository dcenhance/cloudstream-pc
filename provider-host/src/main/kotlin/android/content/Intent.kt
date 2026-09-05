@file:Suppress("unused")

package android.content

import android.net.Uri

class ComponentName(val packageName: String = "", val className: String = "")

open class Intent {
    var action: String? = null
        private set
    var data: Uri? = null
        private set
    var component: ComponentName? = null
        private set
    private var flags: Int = 0
    private val extras = mutableMapOf<String, Any?>()

    constructor()
    constructor(action: String) { this.action = action }
    constructor(action: String, uri: Uri?) { this.action = action; this.data = uri }

    fun addFlags(value: Int): Intent = apply { flags = flags or value }
    fun setFlags(value: Int): Intent = apply { flags = value }
    fun setData(value: Uri?): Intent = apply { data = value }
    fun setComponent(value: ComponentName?): Intent = apply { component = value }
    fun putExtra(key: String, value: Boolean): Intent = apply { extras[key] = value }
    fun putExtra(key: String, value: Int): Intent = apply { extras[key] = value }
    fun putExtra(key: String, value: Long): Intent = apply { extras[key] = value }
    fun putExtra(key: String, value: String?): Intent = apply { extras[key] = value }
    fun getBooleanExtra(key: String, defaultValue: Boolean): Boolean = extras[key] as? Boolean ?: defaultValue
    fun getStringExtra(key: String): String? = extras[key] as? String

    companion object {
        const val ACTION_VIEW = "android.intent.action.VIEW"
        const val FLAG_ACTIVITY_NEW_TASK = 0x10000000
        const val FLAG_ACTIVITY_CLEAR_TASK = 0x00008000
        @JvmStatic fun makeRestartActivityTask(componentName: ComponentName?): Intent = Intent().setComponent(componentName)
    }
}
