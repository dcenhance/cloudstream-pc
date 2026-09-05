package com.lagradost.cloudstream3.linux.host

import com.lagradost.cloudstream3.*
import java.io.File
import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction
import java.nio.file.Files
import java.util.concurrent.TimeUnit
import java.util.jar.JarEntry
import java.util.jar.JarOutputStream
import kotlinx.serialization.json.*
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class UnicodeHomeFixture : MainAPI() {
    override var name = "Unicode fixture"
    override val mainPage = listOf(MainPageData("", "Home", false))
    @Suppress("DEPRECATION_ERROR")
    override suspend fun getMainPage(page: Int, request: MainPageRequest): HomePageResponse {
        println("provider debug output must not contaminate JSON")
        return HomePageResponse(listOf(HomePageList("Zufälliger Anime — 日本語", emptyList())))
    }
}

class FailingHomeFixture : MainAPI() {
    override var name = "Failing fixture"
    override val mainPage = listOf(MainPageData("", "Home", false))
    override suspend fun getMainPage(page: Int, request: MainPageRequest): HomePageResponse {
        error("fixture TLS handshake failed")
    }
}

class HomeProtocolEncodingTest {
    @Test
    fun failedHomeRequestDoesNotMasqueradeAsEmptySuccess() {
        val dir = Files.createTempDirectory("home-failure").toFile()
        try {
            val jar = File(dir, "fixture.jar")
            JarOutputStream(jar.outputStream()).use {
                it.putNextEntry(JarEntry(FailingHomeFixture::class.java.name.replace('.', '/') + ".class"))
                it.write(byteArrayOf(0)); it.closeEntry()
            }
            val out = File(dir, "stdout.json")
            val err = File(dir, "stderr.txt")
            val java = File(System.getProperty("java.home"), "bin/java" + if (System.getProperty("os.name").startsWith("Windows")) ".exe" else "")
            val process = ProcessBuilder(java.path, "-cp", System.getProperty("host.test.classpath"),
                "com.lagradost.cloudstream3.linux.host.MainKt", "home", jar.path, "auto", "Failing fixture")
                .redirectOutput(out).redirectError(err).start()
            try {
                assertTrue(process.waitFor(30, TimeUnit.SECONDS), "host did not finish")
                assertTrue(process.exitValue() != 0, "failed provider returned success: ${out.readText()}")
                assertTrue(err.readText().contains("fixture TLS handshake failed"))
            } finally { process.destroyForcibly() }
        } finally { dir.deleteRecursively() }
    }
    @Test
    fun homeIsUtf8EvenWhenWindowsJvmUsesLegacyCodePage() {
        val dir = Files.createTempDirectory("home-protocol").toFile()
        try {
            val jar = File(dir, "fixture.jar")
            JarOutputStream(jar.outputStream()).use {
                it.putNextEntry(JarEntry(UnicodeHomeFixture::class.java.name.replace('.', '/') + ".class"))
                it.write(byteArrayOf(0)) // class is supplied by the test runtime classpath
                it.closeEntry()
            }
            val out = File(dir, "stdout.json")
            val err = File(dir, "stderr.txt")
            val java = File(System.getProperty("java.home"), "bin/java" + if (System.getProperty("os.name").startsWith("Windows")) ".exe" else "")
            val process = ProcessBuilder(java.path, "-Dfile.encoding=windows-1252",
                "-Dsun.stdout.encoding=windows-1252", "-cp", System.getProperty("host.test.classpath"),
                "com.lagradost.cloudstream3.linux.host.MainKt", "home", jar.path, "auto", "Unicode fixture")
                .redirectOutput(out).redirectError(err).start()
            try {
                assertTrue(process.waitFor(30, TimeUnit.SECONDS), "host did not finish")
                assertEquals(0, process.exitValue(), err.readText())
                val json = Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(out.readBytes())).toString()
                assertEquals("Zufälliger Anime — 日本語", Json.parseToJsonElement(json).jsonArray.single().jsonObject["name"]!!.jsonPrimitive.content)
                assertTrue(err.readText().contains("provider debug output"))
            } finally { process.destroyForcibly() }
        } finally { dir.deleteRecursively() }
    }
}
