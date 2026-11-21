# SDL and Kotonoha's native JNI bridge call these Activity methods by their
# literal Java names. Keep them when Gradle/R8 minifies generic release APKs.
# The hybrid retail DEV/DIST project carries its own independent rules.
-keep class org.kotonoha.android.BootstrapActivity { *; }
-keep class org.kotonoha.android.GameActivity { *; }

# SDL's native storage and display adapters look up these methods by name.
-keepclassmembers class org.kotonoha.android.GameActivity {
    public *** getKotonoha*(...);
    public *** setKotonoha*(...);
}
