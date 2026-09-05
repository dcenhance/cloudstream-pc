@file:Suppress("unused")

package androidx.fragment.app

open class Fragment {
    open val childFragmentManager: FragmentManager = FragmentManager()
    open val parentFragmentManager: FragmentManager = FragmentManager()
    open val activity: FragmentActivity? = null
}

open class FragmentTransaction {
    open fun add(fragment: Fragment, tag: String?): FragmentTransaction = this
    open fun remove(fragment: Fragment): FragmentTransaction = this
    open fun commit(): Int = 0
    open fun commitNow() = Unit
}
