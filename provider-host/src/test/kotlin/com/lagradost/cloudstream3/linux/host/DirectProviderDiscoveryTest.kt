package com.lagradost.cloudstream3.linux.host

import android.content.Context
import com.lagradost.cloudstream3.MainAPI
import com.lagradost.cloudstream3.plugins.BasePlugin
import com.lagradost.cloudstream3.plugins.Plugin
import java.nio.file.Files
import java.util.jar.JarEntry
import java.util.jar.JarOutputStream
import kotlin.test.Test
import kotlin.test.assertEquals

private class DirectFixtureProvider : MainAPI() {
    override var name: String = "Direct fixture"
    override val hasMainPage: Boolean = true
}

private class NotAProvider

private class UnexpectedCatalog : MainAPI() {
    override var name: String = "Unexpected catalog"
}

private class ArgumentCatalog(private val value: String) : MainAPI()

private class ThrowingCatalog : MainAPI() {
    init {
        error("fixture constructor failure")
    }
}

private class EmptyFixturePlugin : BasePlugin()

private class ConfiguredCatalog(label: String) : MainAPI() {
    override var name: String = label
}

private class ContextFixturePlugin : Plugin() {
    override fun load(context: Context) {
        require(context is androidx.appcompat.app.AppCompatActivity)
        registerMainAPI(ConfiguredCatalog("Context fixture"))
    }
}

private class RegisterThenThrowPlugin : BasePlugin() {
    override fun load() {
        registerMainAPI(ConfiguredCatalog("Registered before optional failure"))
        error("optional setup failed")
    }
}

private class SidecarConfiguredPlugin : Plugin() {
    override fun load(context: Context) {
        val metadata = com.lagradost.cloudstream3.plugins.PluginManager.getPluginsOnline().singleOrNull()
            ?.takeIf { it.internalName == "SidecarProvider" && it.url == "https://example.org/repo.json" }
            ?: return
        val providerName = com.lagradost.cloudstream3.utils.DataStore.getSharedPrefs(context)
            .getString("provider_name", null) ?: return
        registerMainAPI(ConfiguredCatalog(providerName))
    }
}

class DirectProviderDiscoveryTest {
    @Test
    fun discoversDirectMainApiWithoutPluginWrapper() {
        val names = sequenceOf(
            NotAProvider::class.java.name,
            DirectFixtureProvider::class.java.name,
        )
        val providers = discoverDirectProviderClasses(
            names,
            DirectFixtureProvider::class.java.classLoader,
        )
        assertEquals(listOf(DirectFixtureProvider::class.java), providers)
    }

    @Test
    fun discoversMainApiRegardlessOfClassName() {
        val providers = discoverDirectProviderClasses(
            sequenceOf(UnexpectedCatalog::class.java.name),
            UnexpectedCatalog::class.java.classLoader,
        )
        assertEquals(listOf(UnexpectedCatalog::class.java), providers)
    }

    @Test
    fun skipsUnconstructableClassesWithoutBlockingValidProvider() {
        val providers = instantiateDirectProviders(
            listOf(
                ArgumentCatalog::class.java,
                ThrowingCatalog::class.java,
                DirectFixtureProvider::class.java,
            ),
            "fixture.jar",
        )
        assertEquals(listOf("Direct fixture"), providers.map { it.name })
    }

    @Test
    fun fallsBackToDirectProviderWhenPluginWrapperRegistersNothing() {
        val jar = Files.createTempFile("empty-plugin-with-provider", ".jar")
        try {
            JarOutputStream(Files.newOutputStream(jar)).use { output ->
                for (type in listOf(EmptyFixturePlugin::class.java, DirectFixtureProvider::class.java)) {
                    output.putNextEntry(JarEntry(type.name.replace('.', '/') + ".class"))
                    output.write(byteArrayOf(0))
                    output.closeEntry()
                }
            }
            loadPlugin(jar.toString(), "auto").use { loaded ->
                assertEquals(listOf("Direct fixture"), loaded.providers.map { it.name })
            }
        } finally {
            Files.deleteIfExists(jar)
        }
    }

    @Test
    fun invokesAndroidPluginContextLoadForConfiguredProviders() {
        val jar = Files.createTempFile("context-plugin-provider", ".jar")
        try {
            JarOutputStream(Files.newOutputStream(jar)).use { output ->
                for (type in listOf(ContextFixturePlugin::class.java, ConfiguredCatalog::class.java)) {
                    output.putNextEntry(JarEntry(type.name.replace('.', '/') + ".class"))
                    output.write(byteArrayOf(0))
                    output.closeEntry()
                }
            }
            loadPlugin(jar.toString(), "auto").use { loaded ->
                assertEquals(true, loaded.providers.any { it.name == "Context fixture" })
            }
        } finally {
            Files.deleteIfExists(jar)
        }
    }

    @Test
    fun retainsProviderRegisteredBeforeOptionalPluginFailure() {
        val jar = Files.createTempFile("partial-plugin-provider", ".jar")
        try {
            JarOutputStream(Files.newOutputStream(jar)).use { output ->
                for (type in listOf(RegisterThenThrowPlugin::class.java, ConfiguredCatalog::class.java)) {
                    output.putNextEntry(JarEntry(type.name.replace('.', '/') + ".class"))
                    output.write(byteArrayOf(0))
                    output.closeEntry()
                }
            }
            loadPlugin(jar.toString(), "auto").use { loaded ->
                assertEquals(true, loaded.providers.any { it.name == "Registered before optional failure" })
            }
        } finally {
            Files.deleteIfExists(jar)
        }
    }

    @Test
    fun loadsConfiguredProvidersFromArtifactSidecar() {
        val jar = Files.createTempFile("sidecar-plugin-provider", ".jar")
        val settings = jar.resolveSibling(jar.fileName.toString() + ".settings.json")
        try {
            JarOutputStream(Files.newOutputStream(jar)).use { output ->
                for (type in listOf(SidecarConfiguredPlugin::class.java, ConfiguredCatalog::class.java)) {
                    output.putNextEntry(JarEntry(type.name.replace('.', '/') + ".class"))
                    output.write(byteArrayOf(0))
                    output.closeEntry()
                }
            }
            Files.writeString(settings, """{"_plugin":{"internalName":"SidecarProvider","url":"https://example.org/repo.json","version":4},"rebuild_preference":{"provider_name":"Sidecar fixture"}}""")
            loadPlugin(jar.toString(), "auto").use { loaded ->
                assertEquals(true, loaded.providers.any { it.name == "Sidecar fixture" })
            }
        } finally {
            Files.deleteIfExists(settings)
            Files.deleteIfExists(jar)
        }
    }
}
