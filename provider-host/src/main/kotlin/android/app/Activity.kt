@file:Suppress("unused")

package android.app

import android.content.Context
import java.io.File

open class Activity(preferencesFile: File? = null) : Context(preferencesFile)
