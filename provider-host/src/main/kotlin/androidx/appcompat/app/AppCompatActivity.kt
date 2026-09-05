@file:Suppress("unused")

package androidx.appcompat.app

import androidx.fragment.app.FragmentActivity
import java.io.File

open class AppCompatActivity(preferencesFile: File? = null) : FragmentActivity(preferencesFile)
