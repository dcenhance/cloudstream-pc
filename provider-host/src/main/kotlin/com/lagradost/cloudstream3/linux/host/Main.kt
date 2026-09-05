package com.lagradost.cloudstream3.linux.host

import androidx.appcompat.app.AppCompatActivity
import com.googlecode.d2j.dex.BaseDexExceptionHandler
import com.googlecode.d2j.dex.Dex2jar
import com.fasterxml.jackson.databind.json.JsonMapper
import com.lagradost.cloudstream3.APIHolder
import com.lagradost.cloudstream3.AcraApplication
import com.lagradost.cloudstream3.AnimeLoadResponse
import com.lagradost.cloudstream3.CloudStreamApp
import com.lagradost.cloudstream3.LiveStreamLoadResponse
import com.lagradost.cloudstream3.LoadResponse
import com.lagradost.cloudstream3.MainAPI
import com.lagradost.cloudstream3.MainPageRequest
import com.lagradost.cloudstream3.MovieLoadResponse
import com.lagradost.cloudstream3.SearchResponse
import com.lagradost.cloudstream3.SubtitleFile
import com.lagradost.cloudstream3.TvSeriesLoadResponse
import com.lagradost.cloudstream3.fetchText
import com.lagradost.cloudstream3.plugins.BasePlugin
import com.lagradost.cloudstream3.plugins.Plugin
import com.lagradost.cloudstream3.plugins.PluginData
import com.lagradost.cloudstream3.plugins.PluginManager
import com.lagradost.cloudstream3.utils.ExtractorLink
import com.lagradost.cloudstream3.utils.ExtractorLinkPlayList
import com.lagradost.cloudstream3.utils.loadExtractor
import com.lagradost.cloudstream3.utils.newExtractorLink
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import org.objectweb.asm.ClassReader
import org.objectweb.asm.ClassWriter
import java.io.File
import java.io.PrintStream
import java.lang.reflect.Modifier
import java.net.URLClassLoader
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.util.concurrent.CopyOnWriteArrayList
import java.util.jar.JarEntry
import java.util.jar.JarFile
import java.util.jar.JarOutputStream
import java.util.zip.ZipFile

private const val MAX_DEX_BYTES = 64 * 1024 * 1024
private const val MAX_CLASS_SCAN_BYTES = 64 * 1024 * 1024
private val URL_PATTERN = Regex("https?://[A-Za-z0-9._~:/?#\\[\\]@!$&'()*+,;=%-]+")

internal fun repairJarFrames(input: Path, output: Path) {
    require(Files.isRegularFile(input)) { "JAR file does not exist: $input" }
    require(input != output) { "Input and output paths must differ" }
    output.parent?.let(Files::createDirectories)
    JarFile(input.toFile()).use { source ->
        JarOutputStream(Files.newOutputStream(output)).use { target ->
            for (entry in source.entries().asSequence()) {
                val upperName = entry.name.uppercase()
                val isStaleSignature = upperName.startsWith("META-INF/") &&
                    (upperName.endsWith(".SF") || upperName.endsWith(".RSA") ||
                        upperName.endsWith(".DSA") || upperName.endsWith(".EC"))
                if (isStaleSignature) continue
                target.putNextEntry(JarEntry(entry.name).apply { time = entry.time })
                if (!entry.isDirectory) {
                    val original = source.getInputStream(entry).use { it.readBytes() }
                    val content = if (entry.name.endsWith(".class")) {
                        val reader = ClassReader(original)
                        val writer = object : ClassWriter(COMPUTE_FRAMES or COMPUTE_MAXS) {
                            override fun getCommonSuperClass(type1: String, type2: String): String =
                                runCatching { super.getCommonSuperClass(type1, type2) }
                                    .getOrDefault("java/lang/Object")
                        }
                        reader.accept(writer, ClassReader.SKIP_FRAMES)
                        writer.toByteArray()
                    } else original
                    target.write(content)
                }
                target.closeEntry()
            }
        }
    }
}

internal fun convertCs3ToJar(input: Path, output: Path) {
    require(Files.isRegularFile(input)) { "CS3 file does not exist: $input" }
    require(input != output) { "Input and output paths must differ" }
    val dex = ZipFile(input.toFile()).use { archive ->
        val entry = archive.getEntry("classes.dex")
            ?: throw IllegalArgumentException("CS3 archive contains no classes.dex")
        require(!entry.isDirectory) { "classes.dex is not a file" }
        require(entry.size < 0 || entry.size <= MAX_DEX_BYTES) { "classes.dex exceeds 64 MiB" }
        archive.getInputStream(entry).use { stream ->
            stream.readNBytes(MAX_DEX_BYTES + 1).also { bytes ->
                require(bytes.size <= MAX_DEX_BYTES) { "classes.dex exceeds 64 MiB" }
            }
        }
    }
    require(dex.size >= 8 && dex.copyOfRange(0, 4).contentEquals("dex\n".toByteArray())) {
        "classes.dex has an invalid DEX header"
    }
    output.parent?.let(Files::createDirectories)
    val temporary = output.resolveSibling(output.fileName.toString() + ".part")
    val repaired = output.resolveSibling(output.fileName.toString() + ".frames.part")
    Files.deleteIfExists(temporary)
    Files.deleteIfExists(repaired)
    try {
        Dex2jar.from(dex)
            .dontSanitizeNames(true)
            .withExceptionHandler(BaseDexExceptionHandler())
            .skipDebug()
            .to(temporary)
        repairJarFrames(temporary, repaired)
        val hasClasses = JarFile(repaired.toFile()).use { archive ->
            archive.entries().asSequence().any { !it.isDirectory && it.name.endsWith(".class") }
        }
        require(hasClasses) { "DEX conversion produced no JVM classes" }
        Files.deleteIfExists(temporary)
        try {
            Files.move(repaired, output, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE)
        } catch (_: AtomicMoveNotSupportedException) {
            Files.move(repaired, output, StandardCopyOption.REPLACE_EXISTING)
        }
    } catch (error: Throwable) {
        Files.deleteIfExists(temporary)
        Files.deleteIfExists(repaired)
        throw error
    }
}

internal fun findRepositoryUrlCandidates(input: Path): List<String> {
    require(Files.isRegularFile(input)) { "Extension artifact does not exist: $input" }
    var temporary: Path? = null
    val jar = if (input.fileName.toString().endsWith(".cs3", ignoreCase = true)) {
        Files.createTempFile("cloudstream-repository-candidates", ".jar").also {
            temporary = it
            convertCs3ToJar(input, it)
        }
    } else input
    return try {
        val candidates = linkedSetOf<String>()
        var scanned = 0
        JarFile(jar.toFile()).use { archive ->
            for (entry in archive.entries().asSequence()) {
                if (entry.isDirectory || !entry.name.endsWith(".class")) continue
                val remaining = MAX_CLASS_SCAN_BYTES - scanned
                if (remaining <= 0) break
                val bytes = archive.getInputStream(entry).use { it.readNBytes(remaining + 1) }
                require(bytes.size <= remaining) { "Extension classes exceed 64 MiB scan limit" }
                scanned += bytes.size
                val text = bytes.toString(Charsets.ISO_8859_1)
                for (match in URL_PATTERN.findAll(text)) {
                    val url = match.value.trimEnd('.', ',', ';', ')', ']', '}', '\u0000')
                    val lower = url.lowercase()
                    if (lower.endsWith(".json") || "repo" in lower) candidates += url
                }
            }
        }
        candidates.toList()
    } finally {
        temporary?.let(Files::deleteIfExists)
    }
}

internal data class LoadedPlugin(
    val plugin: BasePlugin?,
    val loader: URLClassLoader,
    val providers: List<MainAPI>,
) : AutoCloseable {
    override fun close() {
        runCatching { plugin?.beforeUnload() }
        loader.close()
    }
}

internal fun loadPlugin(jarPath: String, pluginClassName: String): LoadedPlugin {
    val jar = File(jarPath).canonicalFile
    require(jar.isFile) { "Provider JAR does not exist: $jar" }
    val loader = URLClassLoader(arrayOf(jar.toURI().toURL()), Thread.currentThread().contextClassLoader)
    val classNames = JarFile(jar).use { archive ->
        archive.entries().asSequence()
            .filter { !it.isDirectory && it.name.endsWith(".class") && '$' !in it.name }
            .map { it.name.removeSuffix(".class").replace('/', '.') }
            .toList()
    }
    val resolvedClassName = if (pluginClassName == "auto") discoverPluginClass(classNames.asSequence(), loader) else pluginClassName
    val settingsFile = File(jar.absolutePath + ".settings.json").takeIf(File::isFile)
    val metadata = settingsFile?.let { file ->
        runCatching {
            val node = JsonMapper.builder().build().readTree(file).path("_plugin")
            if (!node.isObject) null else PluginData(
                internalName = node.path("internalName").asText(jar.nameWithoutExtension),
                url = node.path("url").takeIf { !it.isMissingNode && !it.isNull }?.asText(),
                isOnline = true,
                filePath = jar.absolutePath,
                version = node.path("version").asInt(0),
            )
        }.getOrNull()
    }
    PluginManager.setCurrentPlugin(metadata)
    var plugin: BasePlugin? = null
    val pluginFailures = mutableListOf<String>()
    if (resolvedClassName != null) {
        try {
            plugin = loader.loadClass(resolvedClassName).getDeclaredConstructor().newInstance() as? BasePlugin
                ?: error("Plugin class does not extend BasePlugin: $resolvedClassName")
            plugin.filename = jar.absolutePath
            if (plugin is Plugin) {
                val context = AppCompatActivity(settingsFile)
                CloudStreamApp.context = context
                AcraApplication.context = context
                plugin.load(context)
            } else plugin.load()
            APIHolder.initAll()
            val registered = APIHolder.allProviders.filter { it.sourcePlugin == jar.absolutePath }
            if (registered.isNotEmpty()) return LoadedPlugin(plugin, loader, registered)
            pluginFailures += "$resolvedClassName registered no providers through load()"
        } catch (error: Throwable) {
            val cause = deepestCause(error)
            pluginFailures += "$resolvedClassName: ${cause::class.java.name}: ${cause.message.orEmpty()}"
        }
        val partiallyRegistered = APIHolder.allProviders.filter { it.sourcePlugin == jar.absolutePath }
        if (partiallyRegistered.isNotEmpty()) {
            APIHolder.initAll()
            return LoadedPlugin(plugin, loader, partiallyRegistered)
        }
    }
    val failures = mutableListOf<String>()
    val providers = instantiateDirectProviders(
        discoverDirectProviderClasses(classNames.asSequence(), loader),
        jar.absolutePath,
    ) { type, error ->
        failures += "${type.name}: ${error::class.java.name}: ${error.message.orEmpty()}"
    }
    require(providers.isNotEmpty()) {
        "No compatible direct MainAPI implementation found in ${jar.name}" +
            if (pluginFailures.isEmpty() && failures.isEmpty()) ""
            else ": " + (pluginFailures + failures).take(4).joinToString(" | ")
    }
    return LoadedPlugin(plugin, loader, providers)
}

private fun discoverPluginClass(names: Sequence<String>, loader: ClassLoader): String? {
    for (name in names) {
        val type = runCatching { loader.loadClass(name) }.getOrNull() ?: continue
        if (type != BasePlugin::class.java &&
            BasePlugin::class.java.isAssignableFrom(type) &&
            !Modifier.isAbstract(type.modifiers)
        ) return name
    }
    return null
}

internal fun discoverDirectProviderClasses(
    names: Sequence<String>,
    loader: ClassLoader,
): List<Class<out MainAPI>> {
    return names.mapNotNull { name ->
            val type = runCatching { loader.loadClass(name) }.getOrNull() ?: return@mapNotNull null
            if (type == MainAPI::class.java || !MainAPI::class.java.isAssignableFrom(type) ||
                Modifier.isAbstract(type.modifiers)
            ) return@mapNotNull null
            @Suppress("UNCHECKED_CAST")
            type as Class<out MainAPI>
        }.toList()
}

private fun deepestCause(error: Throwable): Throwable {
    var current = error
    val seen = mutableSetOf<Throwable>()
    while (current.cause != null && seen.add(current)) current = current.cause!!
    return current
}

internal fun instantiateDirectProviders(
    types: List<Class<out MainAPI>>,
    sourcePlugin: String,
    onFailure: (Class<out MainAPI>, Throwable) -> Unit = { type, error ->
        val cause = deepestCause(error)
        System.err.println("Skipped ${type.name}: ${cause::class.java.name}: ${cause.message.orEmpty()}")
    },
): List<MainAPI> = types.mapNotNull { type ->
    try {
        type.getDeclaredConstructor().apply { isAccessible = true }.newInstance().also { provider ->
            provider.sourcePlugin = sourcePlugin
            provider.init()
        }
    } catch (error: Throwable) {
        onFailure(type, deepestCause(error))
        null
    }
}

private fun providerJson(provider: MainAPI): JsonObject = buildJsonObject {
    put("name", JsonPrimitive(provider.name))
    put("mainUrl", JsonPrimitive(provider.mainUrl))
    put("language", JsonPrimitive(provider.lang))
    put("hasMainPage", JsonPrimitive(provider.hasMainPage))
    put("supportedTypes", buildJsonArray {
        provider.supportedTypes.sortedBy { it.name }.forEach { add(JsonPrimitive(it.name)) }
    })
}

private fun searchResultJson(result: SearchResponse): JsonObject = buildJsonObject {
    put("name", JsonPrimitive(result.name))
    put("url", JsonPrimitive(result.url))
    put("apiName", JsonPrimitive(result.apiName))
    result.type?.let { put("type", JsonPrimitive(it.name)) }
    result.posterUrl?.let { put("posterUrl", JsonPrimitive(it)) }
    result.quality?.let { put("quality", JsonPrimitive(it.name)) }
    result.id?.let { put("id", JsonPrimitive(it)) }
}

private fun homeSectionJson(name: String, horizontalImages: Boolean, items: List<SearchResponse>): JsonObject = buildJsonObject {
    put("name", JsonPrimitive(name.ifBlank { "Featured" }))
    put("horizontalImages", JsonPrimitive(horizontalImages))
    put("items", JsonArray(items.map(::searchResultJson)))
}

private fun loadResponseJson(response: LoadResponse): JsonObject = buildJsonObject {
    put("name", JsonPrimitive(response.name))
    put("url", JsonPrimitive(response.url))
    put("apiName", JsonPrimitive(response.apiName))
    put("type", JsonPrimitive(response.type.name))
    response.posterUrl?.let { put("posterUrl", JsonPrimitive(it)) }
    response.backgroundPosterUrl?.let { put("backgroundPosterUrl", JsonPrimitive(it)) }
    response.year?.let { put("year", JsonPrimitive(it)) }
    response.plot?.let { put("plot", JsonPrimitive(it)) }
    response.duration?.let { put("duration", JsonPrimitive(it)) }
    response.contentRating?.let { put("contentRating", JsonPrimitive(it)) }
    put("comingSoon", JsonPrimitive(response.comingSoon))
    put("tags", JsonArray(response.tags.orEmpty().map(::JsonPrimitive)))
    when (response) {
        is MovieLoadResponse -> put("data", JsonPrimitive(response.dataUrl))
        is LiveStreamLoadResponse -> put("data", JsonPrimitive(response.dataUrl))
        is TvSeriesLoadResponse -> put("episodes", JsonArray(response.episodes.map { episode ->
            buildJsonObject {
                put("data", JsonPrimitive(episode.data))
                episode.name?.let { put("name", JsonPrimitive(it)) }
                episode.season?.let { put("season", JsonPrimitive(it)) }
                episode.episode?.let { put("episode", JsonPrimitive(it)) }
                episode.posterUrl?.let { put("posterUrl", JsonPrimitive(it)) }
                episode.description?.let { put("description", JsonPrimitive(it)) }
            }
        }))
        is AnimeLoadResponse -> put("episodes", buildJsonArray {
            response.episodes.forEach { (dubStatus, episodes) ->
                episodes.forEach { episode ->
                    add(buildJsonObject {
                        put("data", JsonPrimitive(episode.data))
                        put("dubStatus", JsonPrimitive(dubStatus.name))
                        episode.name?.let { put("name", JsonPrimitive(it)) }
                        episode.season?.let { put("season", JsonPrimitive(it)) }
                        episode.episode?.let { put("episode", JsonPrimitive(it)) }
                        episode.posterUrl?.let { put("posterUrl", JsonPrimitive(it)) }
                    })
                }
            }
        })
        else -> Unit
    }
}

private fun extractorLinkJson(link: ExtractorLink): JsonObject = buildJsonObject {
    put("source", JsonPrimitive(link.source))
    put("name", JsonPrimitive(link.name))
    put("url", JsonPrimitive(link.url))
    put("referer", JsonPrimitive(link.referer))
    put("quality", JsonPrimitive(link.quality))
    put("type", JsonPrimitive(link.type.name))
    put("headers", buildJsonObject {
        link.headers.forEach { (key, value) -> put(key, JsonPrimitive(value)) }
    })
    link.extractorData?.let { put("extractorData", JsonPrimitive(it)) }
    put("audioTracks", buildJsonArray {
        link.audioTracks.forEach { audio ->
            add(buildJsonObject {
                put("url", JsonPrimitive(audio.url))
                put("headers", buildJsonObject {
                    audio.headers.orEmpty().forEach { (key, value) -> put(key, JsonPrimitive(value)) }
                })
            })
        }
    })
    if (link is ExtractorLinkPlayList) {
        put("playlist", buildJsonArray {
            link.playlist.forEach { item ->
                add(buildJsonObject {
                    put("url", JsonPrimitive(item.url))
                    put("durationUs", JsonPrimitive(item.durationUs))
                })
            }
        })
    }
}

private fun subtitleJson(subtitle: SubtitleFile): JsonObject = buildJsonObject {
    put("language", JsonPrimitive(subtitle.lang))
    put("url", JsonPrimitive(subtitle.url))
    put("headers", buildJsonObject {
        subtitle.headers.orEmpty().forEach { (key, value) -> put(key, JsonPrimitive(value)) }
    })
}

internal fun sourceResultJson(
    links: List<ExtractorLink>,
    subtitles: List<SubtitleFile>,
    success: Boolean,
): JsonObject = buildJsonObject {
    put("success", JsonPrimitive(success))
    put("links", JsonArray(links.map(::extractorLinkJson)))
    put("subtitles", JsonArray(subtitles.map(::subtitleJson)))
}

private data class DiscoveredSources(
    val links: List<ExtractorLink>,
    val subtitles: List<SubtitleFile>,
    val success: Boolean,
)

private suspend fun discoverSources(provider: MainAPI, data: String): DiscoveredSources {
    val links = CopyOnWriteArrayList<ExtractorLink>()
    val subtitles = CopyOnWriteArrayList<SubtitleFile>()
    var failure: Throwable? = null
    val providerSuccess = try {
        provider.loadLinks(
            data,
            false,
            { subtitles += it },
            { links += it },
        )
    } catch (error: Throwable) {
        if (error is CancellationException) throw error
        failure = error
        false
    }
    if (links.isEmpty() && (data.startsWith("http://") || data.startsWith("https://"))) {
        runCatching {
            val html = fetchText(data).take(4 * 1024 * 1024)
            directMediaCandidates(html, data).forEach { candidate ->
                links += newExtractorLink(
                    source = provider.name + " direct",
                    name = provider.name + if (candidate.quality > 0) " ${candidate.quality}p" else " direct",
                    url = candidate.url,
                    type = candidate.type,
                ) {
                    referer = data
                    quality = candidate.quality
                }
            }
            embeddedPageCandidates(html, data).forEach { embeddedUrl ->
                runCatching {
                    loadExtractor(
                        embeddedUrl,
                        referer = data,
                        subtitleCallback = { subtitles += it },
                        callback = { links += it },
                    )
                }
            }
        }.onFailure { fallbackError ->
            if (failure == null) failure = fallbackError
        }
    }
    val distinctLinks = links.distinctBy { listOf(it.url, it.type.name, it.quality.toString()) }
    val providerFailure = failure
    if (distinctLinks.isEmpty() && providerFailure != null) throw providerFailure
    if (providerFailure != null && distinctLinks.isNotEmpty()) {
        System.err.println("WARN SourceFallback: ${provider.name}: ${providerFailure.message}")
    }
    return DiscoveredSources(
        distinctLinks,
        subtitles.distinctBy { it.lang to it.url },
        providerSuccess || distinctLinks.isNotEmpty(),
    )
}

private fun usage(): Nothing {
    error(
        "Usage: cloudstream-provider-host list <jar> <plugin-class> | " +
            "convert <cs3> <output-jar> | " +
            "repository-candidates <jar-or-cs3> | " +
            "home <jar> <plugin-class> <provider-name> | " +
            "load <jar> <plugin-class> <provider-name> <url> | " +
            "sources <jar> <plugin-class> <provider-name> <data> | " +
            "links <jar> <plugin-class> <provider-name> <data> | " +
            "search <jar> <plugin-class> <provider-name> <query>"
    )
}

fun main(args: Array<String>) {
    // QJsonDocument consumes UTF-8, but Java 17 on Windows defaults stdout to
    // the system code page. Wrap the original stream with an explicit encoding
    // before redirecting provider println/logging to stderr.
    val protocolOut = PrintStream(System.out, true, Charsets.UTF_8)
    System.setOut(System.err)
    try {
        if (args.size < 2) usage()
        val command = args[0]
        if (command == "convert") {
            if (args.size < 3) usage()
            convertCs3ToJar(File(args[1]).toPath(), File(args[2]).toPath())
            protocolOut.println(buildJsonObject {
                put("jar", JsonPrimitive(File(args[2]).canonicalPath))
            })
            return
        }
        if (command == "repository-candidates") {
            protocolOut.println(JsonArray(findRepositoryUrlCandidates(File(args[1]).toPath()).map(::JsonPrimitive)))
            return
        }
        if (args.size < 3) usage()
        loadPlugin(args[1], args[2]).use { loaded ->
            when (command) {
                "list" -> protocolOut.println(JsonArray(loaded.providers.map(::providerJson)))
                "home" -> {
                    if (args.size < 4) usage()
                    val provider = loaded.providers.firstOrNull { it.name == args[3] }
                        ?: error("Provider not found: ${args[3]}")
                    var homeFailure: Throwable? = null
                    val sections = runBlocking {
                        provider.mainPage.flatMap { page ->
                            val request = MainPageRequest(page.name, page.data, page.horizontalImages)
                            try {
                                provider.getMainPage(1, request)?.items.orEmpty()
                            } catch (error: Throwable) {
                                if (error is CancellationException) throw error
                                homeFailure = error
                                System.err.println("WARN Home: ${provider.name} / ${page.name}: ${error.message}")
                                emptyList()
                            }
                        }
                    }
                    // Keep partial Home results, but do not turn total provider
                    // failure into the successful empty-array protocol response.
                    if (sections.isEmpty()) homeFailure?.let { throw it }
                    protocolOut.println(JsonArray(sections.map { homeSectionJson(it.name, it.isHorizontalImages, it.list) }))
                }
                "load" -> {
                    if (args.size < 5) usage()
                    val provider = loaded.providers.firstOrNull { it.name == args[3] }
                        ?: error("Provider not found: ${args[3]}")
                    val response = runBlocking { provider.load(args.drop(4).joinToString(" ")) }
                        ?: error("Provider returned no details")
                    protocolOut.println(loadResponseJson(response))
                }
                "links" -> {
                    if (args.size < 5) usage()
                    val provider = loaded.providers.firstOrNull { it.name == args[3] }
                        ?: error("Provider not found: ${args[3]}")
                    val sources = runBlocking { discoverSources(provider, args.drop(4).joinToString(" ")) }
                    protocolOut.println(JsonArray(sources.links.map(::extractorLinkJson)))
                }
                "sources" -> {
                    if (args.size < 5) usage()
                    val provider = loaded.providers.firstOrNull { it.name == args[3] }
                        ?: error("Provider not found: ${args[3]}")
                    val sources = runBlocking { discoverSources(provider, args.drop(4).joinToString(" ")) }
                    protocolOut.println(sourceResultJson(sources.links, sources.subtitles, sources.success))
                }
                "search" -> {
                    if (args.size < 5) usage()
                    val provider = loaded.providers.firstOrNull { it.name == args[3] }
                        ?: error("Provider not found: ${args[3]}")
                    val query = args.drop(4).joinToString(" ")
                    val results = runBlocking { provider.search(query, 1)?.items.orEmpty() }
                    protocolOut.println(JsonArray(results.map(::searchResultJson)))
                }
                else -> usage()
            }
        }
    } catch (error: Throwable) {
        System.err.println(buildJsonObject {
            put("error", JsonPrimitive(error.message ?: error::class.qualifiedName ?: "Unknown error"))
        })
        kotlin.system.exitProcess(1)
    }
    kotlin.system.exitProcess(0)
}
