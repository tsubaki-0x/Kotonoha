package org.kotonoha.android;

import android.content.Context;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Android launcher ownership of School Days language selection. The native
 * engine still does the authoritative GPK and JSON coverage checks before
 * showing any translated content. Never reinterpret an unavailable locale as
 * English, or combine a native GPK with a base JSON.
 */
final class SchoolDaysMobileProfile {
    static final class Choice {
        final String id;
        final String label;
        final String mode;
        final String locale;
        final File base;
        final File lxh;
        final boolean available;

        Choice(String id, String label, String mode, String locale,
               File base, File lxh, boolean available) {
            this.id = id;
            this.label = label;
            this.mode = mode;
            this.locale = locale;
            this.base = base;
            this.lxh = lxh;
            this.available = available;
        }

        @Override public String toString() { return label; }
    }

    private static final String PREFS = "school-days-android-launcher";
    private static final String SELECTED = "selected-language";
    private static final String GPK_PROFILE = "SchoolDaysTranslationPack.v1";

    private SchoolDaysMobileProfile() {}

    static File root(Context context) {
        File storage = context.getExternalFilesDir(null);
        if (storage == null) storage = context.getFilesDir();
        return new File(new File(storage, "Kotonoha"), context.getPackageName());
    }

    private static boolean present(File file) {
        return file != null && file.isFile() && file.length() > 0;
    }

    static List<Choice> available(Context context) {
        final File translations = new File(root(context), "translations");
        final File scripts = new File(translations, "scripts");
        final File jsons = new File(translations, "jsons");
        final List<Choice> choices = new ArrayList<>();
        addGpk(choices, scripts, "en", "English — original HQ + GPK L×H", null);
        addGpk(choices, scripts, "pt-BR", "Português (Brasil) — GPK",
                "Script - Brazilian.GPK");
        addGpk(choices, scripts, "es", "Español — GPK",
                "Script - Spanish.GPK");
        addGpk(choices, scripts, "zh-CN", "简体中文 — GPK",
                "Script - chinese-compatible.GPK");
        final File englishLxh = new File(jsons, "en-lxh.json");
        if (present(englishLxh))
            choices.add(new Choice("json:en", "English — original HQ + JSON L×H",
                    "json", "en", null, englishLxh, true));
        final File[] installed = jsons.listFiles();
        if (installed != null) {
            Arrays.sort(installed, (a, b) -> a.getName().compareTo(b.getName()));
            for (File base : installed) {
                final String name = base.getName();
                if (!present(base) || !name.endsWith(".json") ||
                        name.endsWith("-lxh.json") ||
                        name.endsWith("-reviewed-identities.json")) continue;
                final String stem = name.substring(0, name.length() - 5);
                final File lxh = new File(jsons, stem + "-lxh.json");
                // Spanish V4b is the sole native-validated exception that can
                // include all DLC lines inside its base JSON.
                if (!present(lxh) && !name.equals("es.json")) continue;
                final String label = name.equals("es.json") ? "Español — JSON"
                        : name.equals("zh-CN.json") ? "简体中文 — JSON"
                        : name.equals("pt-BR.json") ? "Português (Brasil) — JSON"
                        : stem + " — JSON";
                choices.add(new Choice("json:" + name, label, "json", stem,
                        base, present(lxh) ? lxh : null, true));
            }
        }
        return choices;
    }

    private static void addGpk(List<Choice> out, File scripts, String locale,
                               String label, String baseName) {
        final File base = baseName == null ? null : new File(scripts, baseName);
        final File lxh = new File(scripts, locale + "-lxh.GPK");
        if (present(lxh) && (base == null || present(base)))
            out.add(new Choice("gpk:" + locale, label, "gpk", locale,
                    base, lxh, true));
    }

    static String selectedId(Context context) {
        String selected = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                .getString(SELECTED, "");
        if (!selected.isEmpty()) return selected;
        final File profile = root(context);
        final File nativeProfile = new File(profile, "translation-pack.txt");
        if (nativeProfile.isFile()) {
            try {
                List<String> lines = Files.readAllLines(nativeProfile.toPath(),
                        StandardCharsets.UTF_8);
                if (lines.size() == 2 && GPK_PROFILE.equals(lines.get(0)))
                    return "gpk:" + lines.get(1);
                return "invalid-native-profile";
            } catch (IOException error) {
                return "invalid-native-profile";
            }
        }
        final File language = new File(profile, "language.txt");
        if (language.isFile()) {
            try {
                List<String> lines = Files.readAllLines(language.toPath(),
                        StandardCharsets.UTF_8);
                if (lines.isEmpty()) return "invalid-language-profile";
                if ("english".equals(lines.get(0))) return "json:en";
                if (lines.size() > 1 && !lines.get(1).isEmpty())
                    return "json:" + new File(lines.get(1)).getName();
                return "invalid-language-profile";
            } catch (IOException error) {
                return "invalid-language-profile";
            }
        }
        return "";
    }

    static void rememberChoice(Context context, Choice choice) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                .putString(SELECTED, choice.id).apply();
    }

    static float renderScale(Context context) {
        final File settings = new File(root(context), "settings.json");
        try {
            if (settings.isFile()) {
                double stored = new JSONObject(readSmall(settings))
                        .optDouble("renderScale", .75);
                if (Double.isFinite(stored) && stored >= .25 && stored <= 1.0)
                    return (float) stored;
            }
        } catch (IOException | JSONException ignored) {
            // No settings yet; a missing/corrupt profile is not rewritten here.
        }
        return .75f;
    }

    static void setRenderScale(Context context, float value) throws IOException {
        if (value != 1.0f && value != .75f && value != .5f)
            throw new IOException("Unsupported Android render scale");
        final File settings = new File(root(context), "settings.json");
        try {
            JSONObject json = settings.isFile()
                    ? new JSONObject(readSmall(settings)) : new JSONObject();
            if (json.optInt("version", 1) != 1)
                throw new IOException("Unsupported settings version");
            json.put("version", 1);
            json.put("renderScale", value);
            atomicWrite(settings, json.toString());
        } catch (JSONException error) {
            throw new IOException("Invalid settings.json; not changed", error);
        }
    }

    static void apply(Context context, Choice choice) throws IOException {
        // Spanish V4b may include all 273 LxH occurrences in its base JSON,
        // but native CheckLayered still must certify completeness at startup.
        final boolean spanishFullBase = "json".equals(choice.mode) &&
                "es".equals(choice.locale) &&
                choice.base != null && "es.json".equals(choice.base.getName());
        if (!choice.available ||
                (!present(choice.lxh) && !spanishFullBase) ||
                (choice.base != null && !present(choice.base)))
            throw new IOException("Selected language is missing its HQ/L×H files. "
                    + "It was not changed to English.");
        final File profile = root(context);
        if (!profile.isDirectory() && !profile.mkdirs())
            throw new IOException("Cannot prepare School Days settings directory");
        final File settingsFile = new File(profile, "settings.json");
        final File nativeProfile = new File(profile, "translation-pack.txt");
        final File languageFile = new File(profile, "language.txt");
        final JSONObject settings;
        try {
            settings = settingsFile.isFile()
                    ? new JSONObject(readSmall(settingsFile))
                    : new JSONObject();
            if (settings.optInt("version", 1) != 1)
                throw new IOException("Unsupported settings version; refusing to overwrite");
            settings.put("version", 1);
            settings.put("mode", 1); // Android's managed fullscreen Surface.
            settings.put("renderer", "opengles2");
            settings.put("translationPath",
                    choice.mode.equals("gpk") || choice.base == null
                            ? "" : choice.base.getCanonicalPath());
        } catch (JSONException error) {
            throw new IOException("Invalid settings.json; refusing to overwrite", error);
        }
        String language;
        if (choice.mode.equals("gpk")) {
            // Publishing the native sidecar first makes a process interruption
            // fail closed if an old JSON is still selected.
            atomicWrite(nativeProfile, GPK_PROFILE + "\n" + choice.locale + "\n");
            atomicWrite(settingsFile, settings.toString());
            language = choice.locale.equals("en") ? "english-gpk"
                    : choice.locale.equals("pt-BR") ? "brazilian-gpk"
                    : choice.locale.equals("es") ? "spanish-gpk" : "chinese-gpk";
            atomicWrite(languageFile, language + "\n");
        } else {
            language = choice.locale.equals("en") ? "english"
                    : choice.locale.equals("es") ? "spanish"
                    : choice.locale.equals("zh-CN") ? "chinese" : "custom";
            // Remember explicit non-native locale *before* disabling any old
            // native profile: a partially written selection must not silently
            // boot native English.
            final String base = choice.base == null
                    ? "" : choice.base.getCanonicalPath();
            atomicWrite(languageFile, language + "\n" + base + "\n" +
                    (choice.lxh == null ? "" :
                            "lxh-json=" + choice.lxh.getCanonicalPath() + "\n"));
            atomicWrite(settingsFile, settings.toString());
            Files.deleteIfExists(nativeProfile.toPath());
        }
        rememberChoice(context, choice);
    }

    private static String readSmall(File source) throws IOException {
        if (source.length() > 1024 * 1024)
            throw new IOException("Settings file exceeds 1 MiB; not modifying it");
        return new String(Files.readAllBytes(source.toPath()),
                StandardCharsets.UTF_8);
    }

    private static void atomicWrite(File target, String text) throws IOException {
        final File parent = target.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create School Days profile directory");
        final File staging = File.createTempFile(".mobile-settings-", ".tmp", parent);
        try {
            try (FileOutputStream stream = new FileOutputStream(staging)) {
                stream.write(text.getBytes(StandardCharsets.UTF_8));
                stream.getFD().sync();
            }
            Files.move(staging.toPath(), target.toPath(),
                    StandardCopyOption.REPLACE_EXISTING);
        } finally {
            Files.deleteIfExists(staging.toPath());
        }
    }
}
