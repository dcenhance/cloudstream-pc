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
    mainClass = "com.lagradost.cloudstream3.linux.host.MainKt"
    applicationName = "cloudstream-provider-host"
}

dependencies {
    implementation(project(":library"))
    implementation("de.femtopedia.dex2jar:dex-translator:2.4.38")
    implementation("com.google.code.gson:gson:2.11.0")
    implementation("com.fasterxml.jackson.core:jackson-databind:2.13.1")
    implementation("com.squareup.okhttp3:okhttp:5.3.2")
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.kotlinx.serialization.json)
    testImplementation(kotlin("test"))
}

tasks.test {
    useJUnitPlatform()
}
