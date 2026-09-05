@file:Suppress("unused")

package android.content.res

open class Resources {
    open fun getString(id: Int): String = id.toString()
    open fun getIdentifier(name: String?, defType: String?, defPackage: String?): Int = 0
}
