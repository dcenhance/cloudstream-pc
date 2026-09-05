plugins {
    alias(libs.plugins.kotlin.jvm)
    application
}

group = "com.lagradost.cloudstream3"
version = libs.versions.versionName.get()

kotlin {
    jvmToolchain(17)
}

application {
    mainClass = "com.lagradost.cloudstream3.linux.MainKt"
    applicationName = "cloudstream-linux"
}

dependencies {
    implementation(project(":library"))
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.kotlinx.serialization.json)
    testImplementation(kotlin("test"))
}

tasks.test {
    useJUnitPlatform()
}

// Keep the Linux module independent from Android UI and package it as a runnable distribution.
tasks.register<Sync>("assembleLinuxApp") {
    dependsOn(tasks.named("installDist"))
    from(tasks.named("installDist"))
    into(layout.buildDirectory.dir("linux-app"))
}
