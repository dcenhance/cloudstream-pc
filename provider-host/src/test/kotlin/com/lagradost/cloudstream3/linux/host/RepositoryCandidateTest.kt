package com.lagradost.cloudstream3.linux.host

import java.nio.file.Files
import java.util.jar.JarEntry
import java.util.jar.JarOutputStream
import kotlin.test.Test
import kotlin.test.assertEquals

class RepositoryCandidateTest {
    @Test
    fun findsRepositoryDocumentsWithoutExecutingPluginCode() {
        val jar = Files.createTempFile("repository-candidates", ".jar")
        try {
            JarOutputStream(Files.newOutputStream(jar)).use { output ->
                output.putNextEntry(JarEntry("example/Utility.class"))
                output.write(
                    ("https://example.org/provider-home " +
                     "https://example.org/repo.json " +
                     "https://raw.githubusercontent.com/example/repos/master/repos-db.json " +
                     "https://example.org/repo.json").toByteArray()
                )
                output.closeEntry()
            }
            assertEquals(
                listOf(
                    "https://example.org/repo.json",
                    "https://raw.githubusercontent.com/example/repos/master/repos-db.json",
                ),
                findRepositoryUrlCandidates(jar),
            )
        } finally {
            Files.deleteIfExists(jar)
        }
    }
}
