package org.kotonoha.android;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

public final class BootstrapActivity extends Activity {
    private static final int SCHOOL_DAYS_PACKS_REQUEST = 4536;
    private static final int SCHOOL_DAYS_EN_LXH_REQUEST = 4537;
    private static final int SCHOOL_DAYS_TRANSLATION_REQUEST = 4538;
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

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        status = new TextView(this);
        status.setPadding(32, 32, 32, 32);
        status.setText("Preparing game assets...");
        setContentView(status);
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
                    Files.write(complete.toPath(), fingerprint.getBytes(StandardCharsets.UTF_8));
                }
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
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
        startActivity(new Intent(this, GameActivity.class));
        finish();
    }

    private File privateSchoolDaysPacks() {
        return new File(new File(getDataDir(), "game_assets"), "Packs");
    }

    private File privateSchoolDaysEnglishLxh() {
        return new File(new File(getDataDir(), "game_assets"),
                "translations/jsons/en-lxh.json");
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

    private void showSchoolDaysSetup() {
        final boolean packsReady = hasSchoolDaysPacks();
        final boolean lxhReady = privateSchoolDaysEnglishLxh().isFile() &&
                privateSchoolDaysEnglishLxh().length() > 0;
        // Keep this small Android storage/import screen available on every
        // launch so other language packs can be installed without reinstalling
        // the APK or ever replacing the player's existing settings/saves.
        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setPadding(32, 32, 32, 32);
        status = new TextView(this);
        status.setPadding(0, 0, 0, 22);
        status.setText("School Days HQ — Android game files\n\n" +
                (packsReady ? "Original Packs: ready\n" :
                        "Original Packs: missing (29 HQ GPK + LxH.GPK)\n") +
                (lxhReady ? "English LxH text: ready\n" :
                        "English LxH text: missing (en-lxh.json)\n") +
                "\nChoose the files from your own PC installation. Original data " +
                "is not included in the APK. Imported copies stay inside the app; " +
                "existing saves and source files are not changed.");
        controls.addView(status);
        Button choose = new Button(this);
        choose.setText("Import original Packs...");
        choose.setEnabled(!packsReady);
        choose.setOnClickListener(view -> {
            if (importingPacks) return;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try {
                startActivityForResult(intent, SCHOOL_DAYS_PACKS_REQUEST);
            } catch (Exception error) {
                status.setText("Could not open Android's folder picker: " + error.getMessage());
            }
        });
        controls.addView(choose);
        Button english = new Button(this);
        english.setText("Import English LxH text (en-lxh.json)...");
        english.setEnabled(!lxhReady);
        english.setOnClickListener(view -> {
            if (importingPacks) return;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try {
                startActivityForResult(intent, SCHOOL_DAYS_EN_LXH_REQUEST);
            } catch (Exception error) {
                status.setText("Could not open Android's file picker: " + error.getMessage());
            }
        });
        controls.addView(english);
        Button translations = new Button(this);
        translations.setText("Import language GPK or JSON...");
        translations.setOnClickListener(view -> {
            if (importingPacks) return;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try {
                startActivityForResult(intent, SCHOOL_DAYS_TRANSLATION_REQUEST);
            } catch (Exception error) {
                status.setText("Could not open translation picker: " + error.getMessage());
            }
        });
        controls.addView(translations);
        Button retry = new Button(this);
        retry.setText("Recheck imported files");
        retry.setOnClickListener(view -> {
            if (!importingPacks) showSchoolDaysSetup();
        });
        controls.addView(retry);
        Button play = new Button(this);
        play.setText("Open School Days launcher");
        play.setEnabled(packsReady && lxhReady);
        play.setOnClickListener(view -> { if (!importingPacks) launchGame(); });
        controls.addView(play);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(controls);
        setContentView(scroll);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if ((requestCode != SCHOOL_DAYS_PACKS_REQUEST &&
                requestCode != SCHOOL_DAYS_EN_LXH_REQUEST &&
                requestCode != SCHOOL_DAYS_TRANSLATION_REQUEST) ||
                resultCode != RESULT_OK || data == null || data.getData() == null ||
                importingPacks) return;
        importingPacks = true;
        Uri selected = data.getData();
        status.setText(requestCode == SCHOOL_DAYS_PACKS_REQUEST
                ? "Checking the selected original Packs directory..."
                : "Checking the selected English LxH text file...");
        new Thread(() -> {
            try {
                if (requestCode == SCHOOL_DAYS_PACKS_REQUEST)
                    importSchoolDaysPacks(selected);
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
        File destination = privateSchoolDaysEnglishLxh();
        if (destination.isFile() && destination.length() > 0) return;
        File parent = destination.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot prepare the private translation folder");
        File temporary = new File(parent, ".en-lxh-import-" + UUID.randomUUID());
        try {
            long copied = 0;
            try (InputStream input = getContentResolver().openInputStream(source);
                 FileOutputStream output = new FileOutputStream(temporary)) {
                if (input == null) throw new IOException("Cannot read the selected text file");
                byte[] buffer = new byte[65536];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    copied += count;
                    if (copied > 4 * 1024 * 1024)
                        throw new IOException("The selected LxH JSON exceeds 4 MiB");
                    output.write(buffer, 0, count);
                }
                output.getFD().sync();
            }
            if (copied <= 0) throw new IOException("The selected LxH JSON is empty");
            Files.move(temporary.toPath(), destination.toPath());
        } finally {
            Files.deleteIfExists(temporary.toPath());
        }
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
        File documentRoot = getExternalFilesDir(null);
        if (documentRoot == null) documentRoot = getFilesDir();
        File destination = new File(new File(new File(new File(documentRoot,
                "Kotonoha"), getPackageName()), "translations"), folder + "/" + name);
        // Existing translations (especially private PT-BR files) are owned by
        // the user. Do not replace them through an import or a package update.
        if (destination.exists())
            throw new IOException("Translation already installed; it was left unchanged: " + name);
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
            if (size == 0) throw new IOException("Selected translation is empty");
            Files.move(temporary.toPath(), destination.toPath());
        } finally {
            Files.deleteIfExists(temporary.toPath());
        }
    }

    private static final class DocumentEntry {
        final Uri uri;
        final boolean directory;
        final long size;

        DocumentEntry(Uri uri, boolean directory, long size) {
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
                result.put(name.toLowerCase(Locale.ROOT), new DocumentEntry(
                        DocumentsContract.buildDocumentUriUsingTree(tree, id),
                        DocumentsContract.Document.MIME_TYPE_DIR.equals(mime), size));
            }
        } catch (SecurityException error) {
            throw new IOException("Android denied access to the selected folder", error);
        }
        return result;
    }

    private static boolean hasAllPackNames(Map<String, DocumentEntry> entries) {
        for (String name : SCHOOL_DAYS_PACKS) {
            DocumentEntry found = entries.get(name.toLowerCase(Locale.ROOT));
            if (found == null || found.directory) return false;
        }
        return true;
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
            throw new IOException("Select a folder with all 29 original GPK files AND LxH.GPK.");

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
