# SDL/JNI calls the Java Activity methods by exact names. R8 must not rename
# these, including when a commercial (minified) APK is built.
# There are no developer consoles, QA data or assets in this configuration.
-keep class org.kotonoha.android.BootstrapActivity { *; }
-keep class org.kotonoha.android.GameActivity { *; }
