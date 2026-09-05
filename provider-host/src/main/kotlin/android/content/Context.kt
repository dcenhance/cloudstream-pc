@file:Suppress("unused")

package android.content

import android.content.res.Resources
import android.content.pm.PackageManager
import com.fasterxml.jackson.databind.JsonNode
import com.fasterxml.jackson.databind.json.JsonMapper
import java.io.File
import java.util.concurrent.ConcurrentHashMap

open class Context(preferencesFile: File? = null) {
    private val preferenceStores = ConcurrentHashMap<String, MemoryPreferences>().apply {
        putAll(loadPreferenceStores(preferencesFile))
    }

    open val applicationContext: Context get() = this
    open val resources: Resources = Resources()
    open val packageManager: PackageManager = PackageManager()
    open val packageName: String = "com.lagradost.cloudstream3.linux"
    open val filesDir: File = File(System.getProperty("java.io.tmpdir"), "cloudstream-provider-files")
    open val cacheDir: File = File(System.getProperty("java.io.tmpdir"), "cloudstream-provider-cache")

    open fun getSharedPreferences(name: String, mode: Int): SharedPreferences =
        preferenceStores.computeIfAbsent(name) { MemoryPreferences() }

    open fun getString(id: Int): String = id.toString()
    open fun startActivity(intent: Intent) = Unit

    companion object {
        const val MODE_PRIVATE: Int = 0
    }
}

private fun loadPreferenceStores(file: File?): Map<String, MemoryPreferences> {
    if (file == null || !file.isFile) return emptyMap()
    return runCatching {
        val root = JsonMapper.builder().build().readTree(file)
        if (!root.isObject) return@runCatching emptyMap()
        root.fields().asSequence().mapNotNull { (storeName, storeNode) ->
            if (!storeNode.isObject) return@mapNotNull null
            val values = storeNode.fields().asSequence().mapNotNull { (key, value) ->
                jsonPreferenceValue(value)?.let { key to it }
            }.toMap()
            storeName to MemoryPreferences(values)
        }.toMap()
    }.getOrDefault(emptyMap())
}

private fun jsonPreferenceValue(node: JsonNode): Any? = when {
    node.isTextual -> node.textValue()
    node.isBoolean -> node.booleanValue()
    node.isInt -> node.intValue()
    node.isIntegralNumber -> node.longValue()
    node.isFloatingPointNumber -> node.floatValue()
    node.isArray && node.all(JsonNode::isTextual) -> node.map(JsonNode::textValue).toSet()
    else -> null
}

private class MemoryPreferences(initialValues: Map<String, Any> = emptyMap()) : SharedPreferences {
    private val values = ConcurrentHashMap<String, Any>()
    private val listeners = ConcurrentHashMap.newKeySet<SharedPreferences.OnSharedPreferenceChangeListener>()
    init { values.putAll(initialValues) }
    override val all: Map<String, *> get() = values.toMap()
    override fun getBoolean(key: String, defaultValue: Boolean) = values[key] as? Boolean ?: defaultValue
    override fun getString(key: String, defaultValue: String?) = values[key] as? String ?: defaultValue
    @Suppress("UNCHECKED_CAST")
    override fun getStringSet(key: String, defaultValues: Set<String>?) = values[key] as? Set<String> ?: defaultValues
    override fun getInt(key: String, defaultValue: Int) = values[key] as? Int ?: defaultValue
    override fun getLong(key: String, defaultValue: Long) = values[key] as? Long ?: defaultValue
    override fun getFloat(key: String, defaultValue: Float) = values[key] as? Float ?: defaultValue
    override fun contains(key: String) = values.containsKey(key)
    override fun edit(): SharedPreferences.Editor = MemoryEditor(this)
    override fun registerOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {
        listener?.let(listeners::add)
    }
    override fun unregisterOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {
        listener?.let(listeners::remove)
    }

    fun apply(changes: Map<String, Any?>, clear: Boolean) {
        if (clear) values.clear()
        for ((key, value) in changes) {
            if (value == null) values.remove(key) else values[key] = value
            listeners.forEach { it.onSharedPreferenceChanged(this, key) }
        }
    }
}

private class MemoryEditor(private val preferences: MemoryPreferences) : SharedPreferences.Editor {
    private val changes = linkedMapOf<String, Any?>()
    private var clear = false
    override fun putString(key: String, value: String?) = apply { changes[key] = value }
    override fun putStringSet(key: String, values: Set<String>?) = apply { changes[key] = values?.toSet() }
    override fun putInt(key: String, value: Int) = apply { changes[key] = value }
    override fun putLong(key: String, value: Long) = apply { changes[key] = value }
    override fun putFloat(key: String, value: Float) = apply { changes[key] = value }
    override fun putBoolean(key: String, value: Boolean) = apply { changes[key] = value }
    override fun remove(key: String) = apply { changes[key] = null }
    override fun clear() = apply { clear = true; changes.clear() }
    override fun commit(): Boolean { apply(); return true }
    override fun apply() { preferences.apply(changes, clear) }
}
