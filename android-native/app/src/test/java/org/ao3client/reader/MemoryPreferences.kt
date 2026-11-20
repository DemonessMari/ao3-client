package org.ao3client.reader
import android.content.SharedPreferences

class MemoryPreferences : SharedPreferences {
    private val data = mutableMapOf<String,Any?>()
    override fun getAll(): MutableMap<String, *> = data.toMutableMap()
    override fun getString(key: String?, defValue: String?): String? = data[key] as? String ?: defValue
    override fun getStringSet(key: String?, defValues: MutableSet<String>?): MutableSet<String>? = (data[key] as? Set<*>)?.filterIsInstance<String>()?.toMutableSet() ?: defValues
    override fun getInt(key: String?, defValue: Int) = data[key] as? Int ?: defValue
    override fun getLong(key: String?, defValue: Long) = data[key] as? Long ?: defValue
    override fun getFloat(key: String?, defValue: Float) = data[key] as? Float ?: defValue
    override fun getBoolean(key: String?, defValue: Boolean) = data[key] as? Boolean ?: defValue
    override fun contains(key: String?) = data.containsKey(key)
    override fun registerOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}
    override fun unregisterOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}
    override fun edit() = object : SharedPreferences.Editor {
        override fun putString(key: String?, value: String?) = apply { if(key != null) data[key] = value }
        override fun putStringSet(key: String?, values: MutableSet<String>?) = apply { if(key != null) data[key] = values?.toSet() }
        override fun putInt(key: String?, value: Int) = apply { if(key != null) data[key] = value }
        override fun putLong(key: String?, value: Long) = apply { if(key != null) data[key] = value }
        override fun putFloat(key: String?, value: Float) = apply { if(key != null) data[key] = value }
        override fun putBoolean(key: String?, value: Boolean) = apply { if(key != null) data[key] = value }
        override fun remove(key: String?) = apply { data.remove(key) }
        override fun clear() = apply { data.clear() }
        override fun commit() = true
        override fun apply() {}
    }
}
