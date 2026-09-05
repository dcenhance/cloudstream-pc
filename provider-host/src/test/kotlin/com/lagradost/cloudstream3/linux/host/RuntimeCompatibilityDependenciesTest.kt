package com.lagradost.cloudstream3.linux.host

import android.net.Uri
import com.lagradost.cloudstream3.network.CloudflareKiller
import com.lagradost.cloudstream3.utils.DataStore
import com.lagradost.cloudstream3.utils.DataStoreHelper
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class RuntimeCompatibilityDependenciesTest {
    @Test
    fun includesGsonUsedByExtensions() {
        assertNotNull(Class.forName("com.google.gson.Gson"))
        assertNotNull(Class.forName("com.google.gson.reflect.TypeToken"))
    }

    @Test
    fun exposesSharedPreferencesNestedAbi() {
        assertNotNull(Class.forName("android.content.SharedPreferences\$Editor"))
        assertNotNull(Class.forName("android.content.SharedPreferences\$OnSharedPreferenceChangeListener"))
    }

    @Test
    fun exposesAndroidAndPluginCompatibilityTypes() {
        assertNotNull(Class.forName("android.content.Context"))
        assertNotNull(Class.forName("android.app.Activity"))
        assertNotNull(Class.forName("android.content.res.Resources"))
        assertNotNull(Class.forName("com.lagradost.cloudstream3.plugins.Plugin"))
        assertNotNull(Class.forName("com.lagradost.cloudstream3.plugins.PluginData"))
    }

    @Test
    fun parsesUriPathsAndRepeatedQueryParameters() {
        val uri = Uri.parse("https://example.org/a%20path?lang=en&tag=one&tag=two")
        assertEquals("/a path", uri.path)
        assertEquals(setOf("lang", "tag"), uri.queryParameterNames)
        assertEquals(listOf("one", "two"), uri.getQueryParameters("tag"))
    }

    @Test
    fun exposesSafeCloudflareInterceptorAbi() {
        val interceptor = CloudflareKiller()
        assertEquals(0, interceptor.getCookieHeaders("https://example.org").size)
    }

    @Test
    fun exposesProcessLocalDataStoreAbi() {
        val context = android.content.Context()
        val preferences = DataStore.getSharedPrefs(context)
        preferences.edit().putString("provider-key", "provider-value").apply()
        assertEquals("provider-value", preferences.getString("provider-key", null))
        assertNotNull(DataStore.mapper)
        assertEquals("0", DataStoreHelper.currentAccount)
    }

    @Test
    fun exposesNonRenderingAndroidxActivityAndPreferencesAbi() {
        val activity = androidx.appcompat.app.AppCompatActivity()
        assertNotNull(activity.supportFragmentManager)
        val preferences = androidx.preference.PreferenceManager.getDefaultSharedPreferences(activity)
        preferences.edit().putBoolean("enabled", true).apply()
        assertEquals(true, preferences.getBoolean("enabled", false))
        assertNotNull(Class.forName("androidx.fragment.app.FragmentActivity"))
        assertNotNull(Class.forName("androidx.fragment.app.FragmentManager"))
    }

    @Test
    fun exposesPluginInitializationServicesWithoutAndroidUi() {
        val activity = androidx.appcompat.app.AppCompatActivity()
        com.lagradost.cloudstream3.CloudStreamApp.context = activity
        com.lagradost.cloudstream3.CloudStreamApp.setKey("fixture", "value")
        assertEquals("value", com.lagradost.cloudstream3.CloudStreamApp.getKey<String>("fixture"))
        com.lagradost.cloudstream3.AcraApplication.context = activity
        assertEquals(activity, com.lagradost.cloudstream3.AcraApplication.context)
        assertEquals(0, com.lagradost.cloudstream3.plugins.PluginManager.getPluginsOnline().size)
        com.lagradost.cloudstream3.CommonActivity.activity = activity
        assertEquals(activity, com.lagradost.cloudstream3.CommonActivity.activity)

        var posted = false
        android.os.Handler().post { posted = true }
        assertEquals(true, posted)
        val intent = android.content.Intent("view", Uri.parse("https://example.org/item"))
            .addFlags(4).putExtra("enabled", true)
        assertEquals("https://example.org/item", intent.data.toString())
        assertNotNull(androidx.fragment.app.Fragment().childFragmentManager)
        assertNotNull(Class.forName("com.google.android.material.bottomsheet.BottomSheetDialogFragment"))
    }

    @Test
    fun exposesNonCrashingAndroidLogAbi() {
        assertEquals(0, android.util.Log.d("provider-test", "debug"))
        assertEquals(0, android.util.Log.e("provider-test", "error", IllegalStateException("fixture")))
    }

    @Test
    fun exposesPluginsLoadedEventWithoutAndroidActivity() {
        var observed = false
        val observer: (Boolean) -> Unit = { observed = it }
        com.lagradost.cloudstream3.MainActivity.afterPluginsLoadedEvent += observer
        com.lagradost.cloudstream3.MainActivity.afterPluginsLoadedEvent(true)
        com.lagradost.cloudstream3.MainActivity.afterPluginsLoadedEvent -= observer
        assertEquals(true, observed)
    }
}
