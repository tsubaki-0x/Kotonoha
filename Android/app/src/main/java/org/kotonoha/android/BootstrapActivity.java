package org.kotonoha.android;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
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
        if (hasSchoolDaysPacks()) {
            launchGame();
            return;
        }
        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setPadding(32, 32, 32, 32);
        status.setPadding(0, 0, 0, 22);
        status.setText("School Days HQ needs your own original game files.\n\n" +
                "Select the original Packs folder (or the game folder containing Packs). " +
                "The importer copies 29 retail GPKs and LxH.GPK into this app's private " +
                "game storage. It never changes the source files or your saves. " +
                "Make sure your device has enough free space.");
        controls.addView(status);
        Button choose = new Button(this);
        choose.setText("Import original Packs...");
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
        Button retry = new Button(this);
        retry.setText("Check previously installed Packs");
        retry.setOnClickListener(view -> {
            if (!importingPacks) showSchoolDaysSetup();
        });
        controls.addView(retry);
        setContentView(controls);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != SCHOOL_DAYS_PACKS_REQUEST || resultCode != RESULT_OK ||
                data == null || data.getData() == null || importingPacks) return;
        importingPacks = true;
        Uri originalFolder = data.getData();
        status.setText("Checking the selected original Packs directory...");
        new Thread(() -> {
            try {
                importSchoolDaysPacks(originalFolder);
                runOnUiThread(() -> {
                    importingPacks = false;
                    if (!isFinishing() && !isDestroyed()) launchGame();
                });
            } catch (Exception error) {
                Log.e("Kotonoha", "School Days Packs import failed", error);
                runOnUiThread(() -> {
                    importingPacks = false;
                    if (!isFinishing() && !isDestroyed())
                        status.setText("Original Packs were not imported:\n" + error.getMessage() +
                                "\n\nYour original files and saved games were not modified. " +
                                "You may select the correct folder and retry.");
                });
            }
        }, "SchoolDays-Packs-import").start();
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
