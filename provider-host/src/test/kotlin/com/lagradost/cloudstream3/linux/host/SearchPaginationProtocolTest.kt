package com.lagradost.cloudstream3.linux.host

import com.lagradost.cloudstream3.*
import java.io.File
import java.nio.file.Files
import java.util.concurrent.TimeUnit
import java.util.jar.JarEntry
import java.util.jar.JarOutputStream
import kotlinx.serialization.json.*
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class PagedSearchFixture : MainAPI() {
    override var name = "Paged fixture"
    override suspend fun search(query: String, page: Int): SearchResponseList {
        return newSearchResponseList(listOf(newMovieSearchResponse("$query page $page", "https://fixture.invalid/$page")), page < 2)
    }
}

class SearchPaginationProtocolTest {
    @Test fun explicitPageUsesProviderPaginationAndRetainsLegacyDefault() {
        val dir = Files.createTempDirectory("search-pages").toFile()
        try {
            val jar = File(dir, "fixture.jar")
            JarOutputStream(jar.outputStream()).use {
                it.putNextEntry(JarEntry(PagedSearchFixture::class.java.name.replace('.', '/') + ".class"))
                it.write(byteArrayOf(0)); it.closeEntry()
            }
            fun invoke(vararg args: String): JsonElement {
                val out = File(dir, "stdout.json")
                val err = File(dir, "stderr.txt")
                val java = File(System.getProperty("java.home"), "bin/java" + if (System.getProperty("os.name").startsWith("Windows")) ".exe" else "")
                val process = ProcessBuilder(java.path, "-cp", System.getProperty("host.test.classpath"),
                    "com.lagradost.cloudstream3.linux.host.MainKt", "search", jar.path, "auto", "Paged fixture", *args)
                    .redirectOutput(out).redirectError(err).start()
                try {
                    assertTrue(process.waitFor(30, TimeUnit.SECONDS), "host timed out")
                    assertEquals(0, process.exitValue(), err.readText())
                    return Json.parseToJsonElement(out.readText())
                } finally { process.destroyForcibly() }
            }
            assertEquals("two words page 1", invoke("two words").jsonArray.single().jsonObject["name"]!!.jsonPrimitive.content)
            val first = invoke("--page", "1", "two words").jsonObject
            assertTrue(first["hasNext"]!!.jsonPrimitive.boolean)
            assertEquals("two words page 1", first["items"]!!.jsonArray.single().jsonObject["name"]!!.jsonPrimitive.content)
            val second = invoke("--page", "2", "two words").jsonObject
            assertEquals(false, second["hasNext"]!!.jsonPrimitive.boolean)
            assertEquals("two words page 2", second["items"]!!.jsonArray.single().jsonObject["name"]!!.jsonPrimitive.content)
        } finally { dir.deleteRecursively() }
    }
}
