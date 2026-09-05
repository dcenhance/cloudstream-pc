@file:Suppress("unused")

package android.content

/**
 * Minimal binary-compatibility surface for converted CloudStream providers.
 * Linux providers receive null preferences unless a desktop implementation is
 * explicitly supplied, so provider defaults remain authoritative.
 */
interface SharedPreferences {
    val all: Map<String, *>
    fun getBoolean(key: String, defaultValue: Boolean): Boolean
    fun getString(key: String, defaultValue: String?): String?
    fun getStringSet(key: String, defaultValues: Set<String>?): Set<String>?
    fun getInt(key: String, defaultValue: Int): Int
    fun getLong(key: String, defaultValue: Long): Long
    fun getFloat(key: String, defaultValue: Float): Float
    fun contains(key: String): Boolean
    fun edit(): Editor
    fun registerOnSharedPreferenceChangeListener(listener: OnSharedPreferenceChangeListener?)
    fun unregisterOnSharedPreferenceChangeListener(listener: OnSharedPreferenceChangeListener?)

    interface Editor {
        fun putString(key: String, value: String?): Editor
        fun putStringSet(key: String, values: Set<String>?): Editor
        fun putInt(key: String, value: Int): Editor
        fun putLong(key: String, value: Long): Editor
        fun putFloat(key: String, value: Float): Editor
        fun putBoolean(key: String, value: Boolean): Editor
        fun remove(key: String): Editor
        fun clear(): Editor
        fun commit(): Boolean
        fun apply()
    }

    fun interface OnSharedPreferenceChangeListener {
        fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?)
    }
}
