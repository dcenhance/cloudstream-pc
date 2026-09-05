package com.lagradost.cloudstream3.linux.host

import java.nio.file.Files
import java.net.URLClassLoader
import java.util.jar.JarEntry
import java.util.jar.JarOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.io.path.exists
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import org.objectweb.asm.ClassWriter
import org.objectweb.asm.Label
import org.objectweb.asm.Opcodes

class Cs3ConverterTest {
    @Test
    fun rejectsArchiveWithoutClassesDexAndLeavesNoOutput() {
        val directory = Files.createTempDirectory("cloudstream-cs3-test")
        val input = directory.resolve("broken.cs3")
        val output = directory.resolve("converted.jar")
        ZipOutputStream(Files.newOutputStream(input)).use { zip ->
            zip.putNextEntry(ZipEntry("manifest.json"))
            zip.write("{}".toByteArray())
            zip.closeEntry()
        }

        assertFailsWith<IllegalArgumentException> {
            convertCs3ToJar(input, output)
        }
        assertFalse(output.exists())
        directory.toFile().deleteRecursively()
    }

    @Test
    fun repairsMissingStackMapFramesInConvertedJar() {
        val directory = Files.createTempDirectory("cloudstream-frame-repair-test")
        val input = directory.resolve("invalid.jar")
        val output = directory.resolve("repaired.jar")
        val writer = ClassWriter(0)
        writer.visit(Opcodes.V1_8, Opcodes.ACC_PUBLIC, "fixture/MissingFrames", null, "java/lang/Object", null)
        writer.visitMethod(Opcodes.ACC_PUBLIC or Opcodes.ACC_STATIC, "choose", "(Z)I", null, null).apply {
            visitCode()
            val falseBranch = Label()
            visitVarInsn(Opcodes.ILOAD, 0)
            visitJumpInsn(Opcodes.IFEQ, falseBranch)
            visitInsn(Opcodes.ICONST_1)
            visitInsn(Opcodes.IRETURN)
            visitLabel(falseBranch)
            visitInsn(Opcodes.ICONST_0)
            visitInsn(Opcodes.IRETURN)
            visitMaxs(1, 1)
            visitEnd()
        }
        writer.visitEnd()
        JarOutputStream(Files.newOutputStream(input)).use { jar ->
            jar.putNextEntry(JarEntry("fixture/MissingFrames.class"))
            jar.write(writer.toByteArray())
            jar.closeEntry()
        }
        assertFailsWith<VerifyError> {
            URLClassLoader(arrayOf(input.toUri().toURL()), null).use { loader ->
                loader.loadClass("fixture.MissingFrames").getMethod("choose", Boolean::class.javaPrimitiveType)
            }
        }

        repairJarFrames(input, output)

        URLClassLoader(arrayOf(output.toUri().toURL()), null).use { loader ->
            val method = loader.loadClass("fixture.MissingFrames").getMethod("choose", Boolean::class.javaPrimitiveType)
            assertEquals(1, method.invoke(null, true))
            assertEquals(0, method.invoke(null, false))
        }
        directory.toFile().deleteRecursively()
    }
}
