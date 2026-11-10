package org.kotonoha.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.util.TypedValue;
import android.view.DisplayCutout;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

public final class BootstrapActivity extends Activity {
    private static final int SCHOOL_DAYS_PACKS_REQUEST = 4536;
    private static final int SCHOOL_DAYS_EN_LXH_REQUEST = 4537;
    private static final int SCHOOL_DAYS_TRANSLATION_REQUEST = 4538;
    private static final int SCHOOL_DAYS_FOLDER_REQUEST = 4539;
    // These names identify the 29 files from the player's legitimate HQ 1.02
    // installation and the separate, required LxH media archive. The APK never
    // contains or creates any of these copyrighted archives.
    private static final String[] SCHOOL_DAYS_PACKS = {
            "BGM.GPK", "Event00.GPK", "Event01.GPK", "Event02.GPK",
            "Event03.GPK", "Event04.GPK", "Event05.GPK", "Ini.GPK",
            "Movie00.GPK", "Movie01.GPK", "Movie02.GPK", "Movie03.GPK",
            "Movie04.GPK", "Movie05.GPK", "Script.GPK", "Se00.GPK",
            "Se01.GPK", "Se02.GPK", "Se03.GPK", "Se04.GPK",
            "Se05.GPK", "SysSe.GPK", "System.GPK", "Voice00.GPK",
            "Voice01.GPK", "Voice02.GPK", "Voice03.GPK", "Voice04.GPK",
            "Voice05.GPK", "LxH.GPK"
    };
    private TextView status;
    private boolean importingPacks;
    private volatile boolean assetsPrepared;
    private volatile String packagedTranslationStatus = "";
    private SchoolDaysMobileProfile.Choice currentLanguage;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        AndroidImmersion.apply(this);
        status = new TextView(this);
        if (BuildConfig.NATIVE_TARGET.equals("SchoolDays")) {
            // This Activity is the Android game launcher itself, not a splash
            // that precedes a second SDL/desktop launcher. Show its controls
            // immediately while required local assets are verified.
            showSchoolDaysSetup();
        } else {
            status.setPadding(32, 32, 32, 32);
            status.setText("Preparing game assets...");
            setContentView(status);
        }
        new Thread(() -> {
            try {
                String fingerprint;
                try (InputStream input = getAssets().open("packaged-assets.sha256")) {
                    fingerprint = new String(readAll(input), StandardCharsets.UTF_8).trim();
                }
                if (!fingerprint.matches("[a-f0-9]{64}")) {
                    throw new IOException("Invalid packaged asset fingerprint");
                }
                File directory = getFilesDir();
                File complete = new File(directory, ".kotonoha-assets.sha256");
                String installed = complete.isFile()
                        ? new String(Files.readAllBytes(complete.toPath()), StandardCharsets.UTF_8).trim()
                        : "";
                if (!fingerprint.equals(installed)) {
                    Files.deleteIfExists(complete.toPath());
                    copyDirectory(getAssets(), "assets", directory, "assets");
                    String[] gameFiles = getAssets().list("game-assets");
                    if (gameFiles == null) throw new IOException("Cannot list game-assets");
                    if (gameFiles.length > 0)
                        copyDirectory(getAssets(), "game-assets", getDataDir(), "game_assets");
                    File gameAssets = new File(getDataDir(), "game_assets");
                    if (!gameAssets.isDirectory() && !gameAssets.mkdirs()) {
                        throw new IOException("Cannot create game-assets directory");
                    }
                    if (BuildConfig.NATIVE_TARGET.equals("DokiDoki")) {
                        copyDirectory(getAssets(), "python", directory, "python");
                    }
                }
                if (BuildConfig.NATIVE_TARGET.equals("SchoolDays")) {
                    // Packaged locale GPK/JSON files live under the same profile
                    // as native translation-pack.txt and settings.json. Existing
                    // PT-BR/imported translations are never silently replaced.
                    final SchoolDaysEmbeddedTranslations.Result result =
                            SchoolDaysEmbeddedTranslations.install(this, getAssets());
                    packagedTranslationStatus = result.toString() +
                            (result.protectedFiles.isEmpty() ? "" :
                                    "\nUnchanged user-owned translations: " +
                                    String.join(", ", result.protectedFiles));
                }
                if (!fingerprint.equals(installed))
                    Files.write(complete.toPath(), fingerprint.getBytes(StandardCharsets.UTF_8));
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    assetsPrepared = true;
                    if (BuildConfig.NATIVE_TARGET.equals("SchoolDays"))
                        showSchoolDaysSetup();
                    else
                        launchGame();
                });
            } catch (IOException error) {
                Log.e("Kotonoha", "Cannot prepare packaged assets", error);
                runOnUiThread(() -> status.setText("Cannot prepare game assets:\n" + error.getMessage()));
            }
        }, "Kotonoha-assets").start();
    }

    private void launchGame() {
        if (BuildConfig.NATIVE_TARGET.equals("SchoolDays") && !assetsPrepared)
            return;
        startActivity(new Intent(this, GameActivity.class));
        // Keep the Android launcher on the back stack after gameplay exits.
        if (!BuildConfig.NATIVE_TARGET.equals("SchoolDays")) finish();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) AndroidImmersion.apply(this);
    }

    private File privateSchoolDaysPacks() {
        return new File(new File(getDataDir(), "game_assets"), "Packs");
    }

    private File privateSchoolDaysEnglishLxh() {
        return new File(privateSchoolDaysProfile(), "translations/jsons/en-lxh.json");
    }

    private File privateSchoolDaysProfile() {
        // Match FileSystem.cpp: directoryLayout(Android).saves and the native
        // launcher settings.json parent exactly, including external fallback.
        File documents = getExternalFilesDir(null);
        if (documents == null) documents = getFilesDir();
        return new File(new File(documents, "Kotonoha"), getPackageName());
    }

    private boolean hasEnglishLxh() {
        for (File file : new File[]{privateSchoolDaysEnglishLxh(),
                new File(privateSchoolDaysProfile(), "translations/scripts/en-lxh.GPK"),
                // Older Android imports used this path. Keep them readable;
                // never delete/replace data when updating the application.
                new File(new File(getDataDir(), "game_assets"),
                        "translations/jsons/en-lxh.json")}) {
            if (file.isFile() && file.length() > 0) return true;
        }
        return false;
    }

    private boolean hasUsableLanguageFiles() {
        if (hasEnglishLxh()) return true;
        File root = new File(privateSchoolDaysProfile(), "translations");
        File scripts = new File(root, "scripts");
        for (String[] pair : new String[][]{
                {"Script - Brazilian.GPK", "pt-BR-lxh.GPK"},
                {"Script - Spanish.GPK", "es-lxh.GPK"},
                {"Script - chinese-compatible.GPK", "zh-CN-lxh.GPK"}}) {
            if (nonemptyFile(new File(scripts, pair[0])) &&
                    nonemptyFile(new File(scripts, pair[1]))) return true;
        }
        File jsons = new File(root, "jsons");
        File[] installed = jsons.listFiles();
        if (installed != null) for (File base : installed) {
            String name = base.getName();
            if (!nonemptyFile(base) || !name.endsWith(".json") ||
                    name.endsWith("-lxh.json") ||
                    name.endsWith("-reviewed-identities.json")) continue;
            String stem = name.substring(0, name.length() - 5);
            if (name.equals("es.json") ||
                    nonemptyFile(new File(jsons, stem + "-lxh.json"))) return true;
        }
        return false;
    }

    private static boolean nonemptyFile(File file) {
        return file.isFile() && file.length() > 0;
    }

    private static boolean completePacks(File directory) {
        if (!directory.isDirectory()) return false;
        File[] files = directory.listFiles();
        if (files == null) return false;
        Map<String, File> named = new HashMap<>();
        for (File file : files)
            if (file.isFile()) named.put(file.getName().toLowerCase(Locale.ROOT), file);
        for (String name : SCHOOL_DAYS_PACKS) {
            File file = named.get(name.toLowerCase(Locale.ROOT));
            if (file == null || file.length() <= 0) return false;
        }
        return true;
    }

    private boolean hasSchoolDaysPacks() {
        if (completePacks(privateSchoolDaysPacks())) return true;
        // Preserve compatibility with the existing OBB / app-specific external
        // installation. Never move, rewrite or delete those original files.
        File obb = getObbDir();
        if (obb == null) return false;
        return completePacks(obb) || completePacks(new File(obb, "Packs")) ||
                completePacks(new File(obb, "game_assets")) ||
                completePacks(new File(new File(obb, "game_assets"), "Packs")) ||
                completePacks(new File(obb, "game-assets")) ||
                completePacks(new File(new File(obb, "game-assets"), "Packs"));
    }

    private String installedTranslationSummary() {
        File root = new File(privateSchoolDaysProfile(), "translations");
        File scripts = new File(root, "scripts");
        File jsons = new File(root, "jsons");
        StringBuilder summary = new StringBuilder("Installed language files:\n");
        for (String[] pair : new String[][]{
                {"English GPK", "", "en-lxh.GPK"},
                {"Brazilian GPK", "Script - Brazilian.GPK", "pt-BR-lxh.GPK"},
                {"Spanish GPK", "Script - Spanish.GPK", "es-lxh.GPK"},
                {"Chinese GPK", "Script - chinese-compatible.GPK", "zh-CN-lxh.GPK"}
        }) {
            boolean base = pair[1].isEmpty() || new File(scripts, pair[1]).isFile();
            boolean sidecar = new File(scripts, pair[2]).isFile();
            summary.append(pair[0]).append(": ")
                    .append(base && sidecar ? "pair installed" : "incomplete")
                    .append('\n');
        }
        for (String[] pair : new String[][]{
                {"Spanish JSON", "es.json", "es-lxh.json"},
                {"Chinese JSON", "zh-CN.json", "zh-CN-lxh.json"}
        }) {
            boolean base = new File(jsons, pair[1]).isFile();
            boolean sidecar = new File(jsons, pair[2]).isFile();
            summary.append(pair[0]).append(": ")
                    .append(base && sidecar ? "pair installed" :
                            base ? "base only" : "not installed")
                    .append('\n');
        }
        summary.append("Full GPK/JSON compatibility is checked again by the PC " +
                "translation validator when you press PLAY.");
        return summary.toString();
    }

    private int dp(float value) {
        return (int) (value * getResources().getDisplayMetrics().density + .5f);
    }

    private TextView launcherText(String value, int sp, int color) {
        TextView text = new TextView(this);
        text.setText(value);
        text.setTextColor(color);
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        return text;
    }

    private Button launcherButton(String label, boolean primary) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 17);
        button.setTextColor(Color.WHITE);
        button.setMinHeight(dp(primary ? 62 : 54));
        GradientDrawable shape = new GradientDrawable(
                GradientDrawable.Orientation.LEFT_RIGHT,
                primary ? new int[]{0xff2473b6, 0xff0b467c}
                        : new int[]{0xff263e61, 0xff192d4b});
        shape.setCornerRadius(dp(12));
        shape.setStroke(dp(1), primary ? 0xffa5d8ff : 0xff7487a6);
        button.setBackground(shape);
        return button;
    }

    private void addLauncherControl(LinearLayout parent, View child) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(9);
        parent.addView(child, params);
    }

    private void openFolderPicker(int requestCode) {
        if (importingPacks) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        try {
            startActivityForResult(intent, requestCode);
        } catch (Exception error) {
            status.setText("Android could not open the folder picker: " +
                    error.getMessage());
        }
    }

    private void showSchoolDaysSetup() {
        final boolean packsReady = hasSchoolDaysPacks();
        final List<SchoolDaysMobileProfile.Choice> choices =
                new ArrayList<>(SchoolDaysMobileProfile.available(this));
        final String savedId = SchoolDaysMobileProfile.selectedId(this);
        int selected = -1;
        for (int i = 0; i < choices.size(); i++) {
            if (choices.get(i).id.equals(savedId)) selected = i;
        }
        if (selected < 0 && !savedId.isEmpty()) {
            choices.add(0, new SchoolDaysMobileProfile.Choice(savedId,
                    "Missing previously chosen language: " + savedId,
                    "", "", null, null, false));
            selected = 0;
        }
        if (choices.isEmpty()) {
            choices.add(new SchoolDaysMobileProfile.Choice("none",
                    "No complete GPK/JSON translation installed", "",
                    "", null, null, false));
        }
        if (selected < 0) selected = 0;
        currentLanguage = choices.get(selected);

        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(0xff0c1a30);
        ImageView art = new ImageView(this);
        art.setScaleType(ImageView.ScaleType.CENTER_CROP);
        try (InputStream input = getAssets().open("launcher-art/SchoolDaysHQ-Launcher.png")) {
            art.setImageBitmap(BitmapFactory.decodeStream(input));
        } catch (IOException error) {
            Log.w("Kotonoha", "Launcher art unavailable", error);
        }
        root.addView(art, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        View shade = new View(this);
        shade.setBackground(new GradientDrawable(
                GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{0x33071326, 0x66071326, 0xd90b1a31}));
        root.addView(shade, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setVerticalScrollBarEnabled(false);
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        int panelWidth = Math.min(screenWidth, Math.max(dp(370), (int)(screenWidth * .53f)));
        FrameLayout.LayoutParams panel = new FrameLayout.LayoutParams(
                panelWidth, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.END);
        root.addView(scroll, panel);
        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setGravity(Gravity.CENTER_VERTICAL);
        controls.setPadding(dp(22), dp(16), dp(22), dp(18));
        GradientDrawable backdrop = new GradientDrawable(
                GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{0xe6152d4c, 0xf2091528});
        backdrop.setCornerRadius(dp(16));
        controls.setBackground(backdrop);
        scroll.addView(controls, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        addLauncherControl(controls, launcherText("SCHOOL DAYS HQ", 27, Color.WHITE));
        addLauncherControl(controls, launcherText(
                BuildConfig.DISTRIBUTION_BUILD ? "ANDROID EDITION" : "ANDROID · DEV",
                12, 0xffb9d7f4));
        status = launcherText(
                (assetsPrepared ? "App resources ready" : "Preparing app resources…") +
                "\n" + (packsReady ? "Original Packs: 30/30 found" : missingPacksSummary()) +
                (packagedTranslationStatus.isEmpty() ? "" :
                        "\n" + packagedTranslationStatus) +
                "\n" + (choices.get(selected).available ?
                        "Language ready for native validation" :
                        "A complete language profile is required"),
                14, 0xffd5e1f1);
        status.setPadding(dp(5), dp(8), dp(5), dp(8));
        addLauncherControl(controls, status);

        Button selectPacks = launcherButton(
                packsReady ? "Packs installed · select another folder" :
                        "Select SchoolDaysHQ / Packs folder", false);
        selectPacks.setEnabled(assetsPrepared);
        selectPacks.setOnClickListener(view ->
                openFolderPicker(SCHOOL_DAYS_FOLDER_REQUEST));
        addLauncherControl(controls, selectPacks);

        addLauncherControl(controls, launcherText("LANGUAGE · GPK / JSON", 13, 0xffb9d7f4));
        Spinner language = new Spinner(this);
        ArrayAdapter<SchoolDaysMobileProfile.Choice> options = new ArrayAdapter<>(
                this, android.R.layout.simple_spinner_item, choices);
        options.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        language.setAdapter(options);
        language.setMinimumHeight(dp(54));
        language.setBackgroundColor(0xffe3eaf4);
        language.setSelection(selected);
        addLauncherControl(controls, language);

        Button display = launcherButton("Display Options", false);
        display.setOnClickListener(view -> openPreliminaryDisplayOptions());
        addLauncherControl(controls, display);

        if (!BuildConfig.DISTRIBUTION_BUILD) {
            Button devTranslations = launcherButton("DEV · import GPK or JSON file", false);
            devTranslations.setEnabled(assetsPrepared);
            devTranslations.setOnClickListener(view -> {
                if (importingPacks) return;
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("*/*");
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                startActivityForResult(intent, SCHOOL_DAYS_TRANSLATION_REQUEST);
            });
            addLauncherControl(controls, devTranslations);
        }

        Button refresh = launcherButton("Recheck installed Packs and languages", false);
        refresh.setEnabled(assetsPrepared);
        refresh.setOnClickListener(view -> {
            if (!importingPacks) showSchoolDaysSetup();
        });
        addLauncherControl(controls, refresh);
        Button play = launcherButton("PLAY", true);
        play.setEnabled(assetsPrepared && packsReady && currentLanguage.available);
        play.setOnClickListener(view -> {
            if (importingPacks || !assetsPrepared || !hasSchoolDaysPacks() ||
                    currentLanguage == null || !currentLanguage.available) return;
            try {
                SchoolDaysMobileProfile.apply(this, currentLanguage);
                launchGame();
            } catch (IOException error) {
                status.setText("Cannot start with this language:\n" +
                        error.getMessage() +
                        "\nNo English fallback was selected.");
            }
        });
        addLauncherControl(controls, play);

        language.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view,
                                                  int index, long id) {
                currentLanguage = choices.get(index);
                play.setEnabled(assetsPrepared && packsReady && currentLanguage.available);
                // Remember even an unavailable selection, so a temporarily
                // missing locale never turns into English behind the user's back.
                if (!currentLanguage.id.equals(savedId))
                    SchoolDaysMobileProfile.rememberChoice(
                            BootstrapActivity.this, currentLanguage);
            }
            @Override public void onNothingSelected(AdapterView<?> parent) {
                play.setEnabled(false);
            }
        });
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            int left = dp(8), top = dp(8), right = dp(8), bottom = dp(8);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null) {
                    left += cutout.getSafeInsetLeft();
                    top += cutout.getSafeInsetTop();
                    right += cutout.getSafeInsetRight();
                    bottom += cutout.getSafeInsetBottom();
                }
            }
            root.setPadding(left, top, right, bottom);
            return insets;
        });
        setContentView(root);
        root.requestApplyInsets();
    }

    private String missingPacksSummary() {
        File packs = privateSchoolDaysPacks();
        File[] existing = packs.listFiles();
        Map<String, Boolean> names = new HashMap<>();
        if (existing != null) {
            for (File file : existing) {
                if (file.isFile() && file.length() > 0)
                    names.put(file.getName().toLowerCase(Locale.ROOT), true);
            }
        }
        StringBuilder missing = new StringBuilder();
        int found = 0;
        for (String name : SCHOOL_DAYS_PACKS) {
            if (names.containsKey(name.toLowerCase(Locale.ROOT))) found++;
            else if (missing.length() < 100) {
                if (missing.length() > 0) missing.append(", ");
                missing.append(name);
            }
        }
        return "Original Packs: " + found + "/" + SCHOOL_DAYS_PACKS.length +
                "\nMissing: " + missing;
    }

    private void openPreliminaryDisplayOptions() {
        final float[] scales = {1.0f, .75f, .5f};
        final String[] labels = {
                "Native rendering · 100%",
                "Balanced rendering · 75%",
                "Battery saving · 50%"};
        final float current = SchoolDaysMobileProfile.renderScale(this);
        int selected = current > .875f ? 0 : current > .625f ? 1 : 2;
        new AlertDialog.Builder(this)
                .setTitle("Display Options · Android")
                .setSingleChoiceItems(labels, selected, (dialog, which) -> {
                    try {
                        SchoolDaysMobileProfile.setRenderScale(this, scales[which]);
                        dialog.dismiss();
                        status.setText("Render scale saved: " + labels[which] +
                                "\nApplied when the game starts.");
                    } catch (IOException error) {
                        status.setText("Display settings not changed: " + error.getMessage());
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if ((requestCode != SCHOOL_DAYS_PACKS_REQUEST &&
                requestCode != SCHOOL_DAYS_EN_LXH_REQUEST &&
                requestCode != SCHOOL_DAYS_TRANSLATION_REQUEST &&
                requestCode != SCHOOL_DAYS_FOLDER_REQUEST) ||
                resultCode != RESULT_OK || data == null || data.getData() == null ||
                importingPacks) return;
        importingPacks = true;
        Uri selected = data.getData();
        if (requestCode == SCHOOL_DAYS_PACKS_REQUEST ||
                requestCode == SCHOOL_DAYS_FOLDER_REQUEST) {
            try {
                getContentResolver().takePersistableUriPermission(
                        selected, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                getSharedPreferences("school-days-android-launcher", MODE_PRIVATE)
                        .edit().putString("packs-tree", selected.toString()).apply();
            } catch (SecurityException error) {
                importingPacks = false;
                status.setText("Android did not grant persistent folder access. " +
                        "Please choose a SchoolDaysHQ or Packs folder via SAF.\n" +
                        error.getMessage());
                return;
            }
        }
        status.setText(requestCode == SCHOOL_DAYS_FOLDER_REQUEST
                ? "Scanning SchoolDaysHQ/Packs and translations..."
                : requestCode == SCHOOL_DAYS_PACKS_REQUEST
                    ? "Checking the selected original Packs directory..."
                    : "Checking the selected translation file...");
        new Thread(() -> {
            try {
                if (requestCode == SCHOOL_DAYS_PACKS_REQUEST)
                    importSchoolDaysPacks(selected);
                else if (requestCode == SCHOOL_DAYS_FOLDER_REQUEST)
                    importOrganizedSchoolDaysFolder(selected);
                else if (requestCode == SCHOOL_DAYS_EN_LXH_REQUEST)
                    importEnglishLxh(selected);
                else
                    importSchoolDaysTranslation(selected);
                runOnUiThread(() -> {
                    importingPacks = false;
                    if (!isFinishing() && !isDestroyed()) showSchoolDaysSetup();
                });
            } catch (Exception error) {
                Log.e("Kotonoha", "School Days file import failed", error);
                runOnUiThread(() -> {
                    importingPacks = false;
                    if (!isFinishing() && !isDestroyed())
                        status.setText("The selected files were not imported:\n" + error.getMessage() +
                                "\n\nYour original files and saved games were not modified. " +
                                "You may select the correct file or folder and retry.");
                });
            }
        }, "SchoolDays-asset-import").start();
    }

    private void importEnglishLxh(Uri source) throws IOException {
        String displayName = null;
        try (Cursor cursor = getContentResolver().query(source,
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) displayName = cursor.getString(0);
        } catch (SecurityException error) {
            throw new IOException("Android denied access to the selected text file", error);
        }
        if (!"en-lxh.json".equals(displayName))
            throw new IOException("Select the English epilogue translation named en-lxh.json");
        importSchoolDaysTranslationFile(source, displayName);
    }

    private void importSchoolDaysTranslation(Uri source) throws IOException {
        String name = null;
        try (Cursor cursor = getContentResolver().query(source,
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
        } catch (SecurityException error) {
            throw new IOException("Android denied access to the selected translation", error);
        }
        if (name == null) throw new IOException("The selected file has no filename");
        importSchoolDaysTranslationFile(source, name);
    }

    // Every single-file and directory-based import uses the same allowlist,
    // size limit, atomic staging and no-replacement rule. The native GPK/JSON
    // validation still runs inside SchoolDays::Launcher before PLAY.
    private boolean importSchoolDaysTranslationFile(Uri source, String name)
            throws IOException {
        return importSchoolDaysTranslationFile(source, name, -1);
    }

    private boolean importSchoolDaysTranslationFile(Uri source, String name,
                                                    long expectedSize)
            throws IOException {
        String folder;
        long maximum;
        if (name.equals("Script - Brazilian.GPK") || name.equals("pt-BR-lxh.GPK") ||
                name.equals("Script - Spanish.GPK") || name.equals("es-lxh.GPK") ||
                name.equals("en-lxh.GPK") || name.equals("Script - chinese-compatible.GPK") ||
                name.equals("zh-CN-lxh.GPK")) {
            folder = "scripts";
            maximum = 128L * 1024 * 1024;
        } else if (name.matches("[A-Za-z0-9][A-Za-z0-9_-]{0,70}\\.json")) {
            folder = "jsons";
            maximum = 16L * 1024 * 1024;
        } else {
            throw new IOException("Unrecognized translation filename. Select a " +
                    "compatible Script/LxH GPK or an HQ/LxH JSON without path characters.");
        }
        // This is precisely the directory the native launcher uses for
        // settings.json, Config.DAT and language sidecars on Android. Never
        // create a second, hidden translation profile under a different root.
        File destination = new File(new File(privateSchoolDaysProfile(),
                "translations"), folder + "/" + name);
        // Existing translations (especially private PT-BR files) are owned by
        // the user. Do not replace them through an import or a package update.
        if (destination.exists()) {
            if (!destination.isFile() || destination.length() == 0)
                throw new IOException("Incomplete translation already exists; no overwrite: " + name);
            return false;
        }
        File parent = destination.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create the translation directory");
        File temporary = new File(parent, ".translation-import-" + UUID.randomUUID());
        try {
            long size = 0;
            try (InputStream input = getContentResolver().openInputStream(source);
                 FileOutputStream output = new FileOutputStream(temporary)) {
                if (input == null) throw new IOException("Cannot open translation");
                byte[] buffer = new byte[65536];
                int read;
                while ((read = input.read(buffer)) != -1) {
                    size += read;
                    if (size > maximum) throw new IOException("Selected translation is too large");
                    output.write(buffer, 0, read);
                }
                output.getFD().sync();
            }
            if (size == 0 || (expectedSize >= 0 && size != expectedSize))
                throw new IOException("Selected translation is empty or truncated: " + name);
            Files.move(temporary.toPath(), destination.toPath());
        } finally {
            Files.deleteIfExists(temporary.toPath());
        }
        return true;
    }

    private static final class DocumentEntry {
        final String name;
        final Uri uri;
        final boolean directory;
        final long size;

        DocumentEntry(String name, Uri uri, boolean directory, long size) {
            this.name = name;
            this.uri = uri;
            this.directory = directory;
            this.size = size;
        }
    }

    private Map<String, DocumentEntry> listDocumentChildren(Uri tree, Uri folder)
            throws IOException {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                tree, DocumentsContract.getDocumentId(folder));
        Map<String, DocumentEntry> result = new HashMap<>();
        String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE};
        try (Cursor cursor = getContentResolver().query(children, columns, null, null, null)) {
            if (cursor == null) throw new IOException("Cannot read selected folder");
            while (cursor.moveToNext()) {
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                long size = cursor.isNull(3) ? -1 : cursor.getLong(3);
                if (id == null || name == null) continue;
                String folded = name.toLowerCase(Locale.ROOT);
                if (result.containsKey(folded))
                    throw new IOException("Ambiguous case-colliding files: " + name);
                result.put(folded, new DocumentEntry(name,
                        DocumentsContract.buildDocumentUriUsingTree(tree, id),
                        DocumentsContract.Document.MIME_TYPE_DIR.equals(mime), size));
            }
        } catch (SecurityException error) {
            throw new IOException("Android denied access to the selected folder", error);
        }
        return result;
    }

    private int importTranslationDocuments(Uri tree, Uri folder, String category)
            throws IOException {
        int imported = 0;
        for (DocumentEntry file : listDocumentChildren(tree, folder).values()) {
            if (file.directory) continue;
            String name = file.name;
            // Match the desktop's organized layout exactly. Unknown/incompatible
            // files (including the original Chinese Script.GPK dialect) are not
            // silently substituted for a certified translated script.
            boolean isGpk = name.equals("Script - Brazilian.GPK") ||
                    name.equals("pt-BR-lxh.GPK") ||
                    name.equals("Script - Spanish.GPK") ||
                    name.equals("es-lxh.GPK") ||
                    name.equals("en-lxh.GPK") ||
                    name.equals("Script - chinese-compatible.GPK") ||
                    name.equals("zh-CN-lxh.GPK");
            boolean isJson = name.matches("[A-Za-z0-9][A-Za-z0-9_-]{0,70}\\.json");
            if (category.equals("scripts") && !isGpk) continue;
            if (category.equals("jsons") && !isJson) continue;
            if (category.equals("flat") && !isJson && !isGpk) continue;
            final String progress = name;
            runOnUiThread(() -> status.setText("Importing translations: " + progress));
            if (importSchoolDaysTranslationFile(file.uri, name, file.size)) ++imported;
        }
        return imported;
    }

    private void importOrganizedSchoolDaysFolder(Uri tree) throws IOException {
        Uri root = DocumentsContract.buildDocumentUriUsingTree(
                tree, DocumentsContract.getTreeDocumentId(tree));
        Map<String, DocumentEntry> files = listDocumentChildren(tree, root);
        // Accept SchoolDaysHQ containing Packs + translations, or just its
        // translations subfolder on a phone whose retail Packs are installed.
        DocumentEntry translations = files.get("translations");
        Map<String, DocumentEntry> translationFolders;
        Uri translationRoot;
        if (translations != null && translations.directory) {
            translationRoot = translations.uri;
            translationFolders = listDocumentChildren(tree, translationRoot);
        } else if (files.containsKey("scripts") || files.containsKey("jsons")) {
            translationRoot = root;
            translationFolders = files;
        } else {
            translationRoot = null;
            translationFolders = null;
        }
        if (translationRoot == null && !files.containsKey("packs") &&
                !hasAllPackNames(files))
            throw new IOException("Choose SchoolDaysHQ (Packs + translations) " +
                    "or its translations folder, not the Downloads root.");

        int imported = 0;
        if (translationFolders != null) {
            imported += importTranslationDocuments(tree, translationRoot, "flat");
            for (String subfolder : new String[]{"jsons", "scripts"}) {
                DocumentEntry folder = translationFolders.get(subfolder);
                if (folder == null) continue;
                if (!folder.directory)
                    throw new IOException("Invalid translations/" + subfolder + " folder");
                imported += importTranslationDocuments(tree, folder.uri, subfolder);
            }
        }
        final int count = imported;
        runOnUiThread(() -> status.setText(
                "Languages prepared (" + count + " new files). Checking original Packs..."));
        if (!hasSchoolDaysPacks()) {
            if (!hasAllPackNames(files) &&
                    (files.get("packs") == null || !files.get("packs").directory))
                throw new IOException("Languages imported. Original Packs are still missing. " +
                        "Select SchoolDaysHQ or SchoolDaysHQ/Packs to import 30 GPKs.");
            importSchoolDaysPacks(tree);
        }
    }

    private static boolean hasAllPackNames(Map<String, DocumentEntry> entries) {
        for (String name : SCHOOL_DAYS_PACKS) {
            DocumentEntry found = entries.get(name.toLowerCase(Locale.ROOT));
            if (found == null || found.directory || found.size == 0) return false;
        }
        return true;
    }

    private static String missingPackNames(Map<String, DocumentEntry> entries) {
        StringBuilder missing = new StringBuilder();
        int found = 0;
        for (String name : SCHOOL_DAYS_PACKS) {
            DocumentEntry entry = entries.get(name.toLowerCase(Locale.ROOT));
            if (entry != null && !entry.directory && entry.size != 0) {
                found++;
            } else {
                if (missing.length() > 0) missing.append(", ");
                missing.append(name);
            }
        }
        return "Found " + found + "/" + SCHOOL_DAYS_PACKS.length +
                " original GPKs; missing/empty: " + missing;
    }

    private void importSchoolDaysPacks(Uri tree) throws IOException {
        Uri folder = DocumentsContract.buildDocumentUriUsingTree(
                tree, DocumentsContract.getTreeDocumentId(tree));
        Map<String, DocumentEntry> entries = listDocumentChildren(tree, folder);
        if (!hasAllPackNames(entries)) {
            DocumentEntry packs = entries.get("packs");
            if (packs != null && packs.directory)
                entries = listDocumentChildren(tree, packs.uri);
        }
        if (!hasAllPackNames(entries))
            throw new IOException("Select a folder with all 29 original HQ GPKs " +
                    "and LxH.GPK. " + missingPackNames(entries));

        File target = privateSchoolDaysPacks();
        if (target.exists()) {
            if (completePacks(target)) return;
            // Do not overwrite a player's existing, possibly incomplete Packs.
            throw new IOException("An incomplete Packs folder already exists in app storage. " +
                    "It must be backed up or fixed manually before importing.");
        }
        File parent = target.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create game asset directory");
        long expected = 0;
        for (String name : SCHOOL_DAYS_PACKS) {
            long size = entries.get(name.toLowerCase(Locale.ROOT)).size;
            if (size > 0) {
                if (Long.MAX_VALUE - expected < size) throw new IOException("Pack sizes overflow");
                expected += size;
            }
        }
        if (expected > 0 && parent.getUsableSpace() < expected + 128L * 1024L * 1024L)
            throw new IOException("Not enough free phone storage for the original Packs.");

        // A private staging folder allows interruption/failure without exposing
        // partial archives as a working installation. No save/profile paths are
        // used or touched here. Existing Packs is never replaced.
        File staging = new File(parent, "Packs-import-" + UUID.randomUUID());
        if (!staging.mkdir()) throw new IOException("Cannot prepare temporary import folder");
        try {
            byte[] buffer = new byte[65536];
            for (int index = 0; index < SCHOOL_DAYS_PACKS.length; ++index) {
                String name = SCHOOL_DAYS_PACKS[index];
                DocumentEntry entry = entries.get(name.toLowerCase(Locale.ROOT));
                final int progress = index + 1;
                runOnUiThread(() -> status.setText("Importing original Packs: " +
                        progress + "/" + SCHOOL_DAYS_PACKS.length + " — " + name));
                File copy = new File(staging, name);
                long written = 0;
                try (InputStream input = getContentResolver().openInputStream(entry.uri);
                     FileOutputStream output = new FileOutputStream(copy)) {
                    if (input == null) throw new IOException("Cannot open " + name);
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        output.write(buffer, 0, count);
                        written += count;
                    }
                    output.getFD().sync();
                }
                if (written <= 0 || (entry.size >= 0 && written != entry.size))
                    throw new IOException("Incomplete copy of " + name);
            }
            if (!completePacks(staging)) throw new IOException("Imported Packs are incomplete");
            Files.move(staging.toPath(), target.toPath());
        } finally {
            // Only delete the temporary folder created in this exact call.
            // The source documents, live Packs and every save remain untouched.
            if (staging.exists()) {
                File[] partial = staging.listFiles();
                if (partial != null) for (File file : partial) file.delete();
                staging.delete();
            }
        }
    }

    private static byte[] readAll(InputStream input) throws IOException {
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[65536];
        int count;
        while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
        return output.toByteArray();
    }

    private static void copyDirectory(AssetManager assets, String path, File root,
                                      String relative) throws IOException {
        File destination = new File(root, relative);
        if (!destination.getCanonicalPath().startsWith(root.getCanonicalPath() + File.separator)) {
            throw new IOException("Asset path escapes resource directory");
        }
        String[] children = assets.list(path);
        if (children == null) throw new IOException("Cannot list assets: " + path);
        if (children.length > 0) {
            if (!destination.isDirectory() && !destination.mkdirs()) {
                throw new IOException("Cannot create asset directory: " + destination);
            }
            for (String child : children)
                copyDirectory(assets, path + "/" + child, root, relative + "/" + child);
        } else {
            File parent = destination.getParentFile();
            if (!parent.isDirectory() && !parent.mkdirs()) {
                throw new IOException("Cannot create asset parent: " + parent);
            }
            try (InputStream input = assets.open(path);
                 FileOutputStream output = new FileOutputStream(destination)) {
                byte[] buffer = new byte[65536];
                int count;
                while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
            }
        }
    }
}
