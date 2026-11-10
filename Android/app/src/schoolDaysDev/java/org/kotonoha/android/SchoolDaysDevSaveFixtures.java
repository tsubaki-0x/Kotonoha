package org.kotonoha.android;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.channels.FileChannel;
import java.nio.channels.Channels;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * Compiled only for schoolDaysDev. No source/class/bytecode or save fixture
 * from this source set belongs in schoolDaysDist.
 */
public final class SchoolDaysDevSaveFixtures {
    private static final String VERSION = "SchoolDaysDevSaveFixtures.v1";
    private static final String TAG = "SchoolDaysDevSaves";
    private static final Pattern SLOT = Pattern.compile("SaveFile00[0-6]\\.DAT");
    private static final int MAX_MANIFEST = 4096;
    private static final long MAX_SAVE_BYTES = 1024 * 1024;

    private SchoolDaysDevSaveFixtures() {}

    public static String install(Context context, AssetManager assets) throws IOException {
        if (BuildConfig.DISTRIBUTION_BUILD ||
                !BuildConfig.NATIVE_TARGET.equals("SchoolDays") ||
                !context.getPackageName().endsWith(".dev"))
            throw new IOException("DEV save fixtures cannot be installed in a distribution");
        final Map<String, Entry> entries = loadManifest(assets);
        final File root = new File(SchoolDaysMobileProfile.root(context), "Save");
        if (!root.isDirectory() && !root.mkdirs())
            throw new IOException("Cannot create private DEV Save folder");
        int installed = 0;
        int skipped = 0;
        StringBuilder report = new StringBuilder("DEV LxH saves:");
        for (Map.Entry<String, Entry> entry : entries.entrySet()) {
            final String name = entry.getKey();
            final Entry expected = entry.getValue();
            final File destination = new File(root, name);
            if (!destination.getCanonicalFile().getParentFile().equals(root.getCanonicalFile()))
                throw new IOException("Unsafe DEV save path");
            if (destination.exists()) {
                if (!destination.isFile() || Files.isSymbolicLink(destination.toPath()))
                    throw new IOException("Invalid occupied DEV Save slot: " + name);
                skipped++;
                report.append("\n").append(name).append(": skipped (already exists)");
                Log.i(TAG, name + ": already occupied, not overwritten");
                continue;
            }
            // The manifest proves identity before any destination file is made.
            File staged = File.createTempFile(".dev-lxh-", ".tmp", root);
            try {
                MessageDigest hash = sha();
                long size = 0;
                try (InputStream input = assets.open("dev-save-fixtures/Save/" + name);
                     FileOutputStream output = new FileOutputStream(staged)) {
                    byte[] bytes = new byte[65536];
                    int count;
                    while ((count = input.read(bytes)) != -1) {
                        size += count;
                        if (size > MAX_SAVE_BYTES)
                            throw new IOException("Oversized DEV fixture: " + name);
                        output.write(bytes, 0, count);
                        hash.update(bytes, 0, count);
                    }
                    output.getFD().sync();
                }
                if (size != expected.bytes || !hex(hash.digest()).equals(expected.hash))
                    throw new IOException("SHA-256 / length mismatch in DEV fixture " + name);
                // CREATE_NEW is stronger than a move without REPLACE_EXISTING
                // on Android filesystems: even a racing existing slot is safe.
                try (FileInputStream input = new FileInputStream(staged);
                     FileChannel output = FileChannel.open(destination.toPath(),
                             StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
                    byte[] bytes = new byte[65536];
                    java.nio.ByteBuffer buffer = java.nio.ByteBuffer.allocate(65536);
                    int count;
                    while ((count = input.read(bytes)) != -1) {
                        buffer.clear();
                        buffer.put(bytes, 0, count).flip();
                        while (buffer.hasRemaining()) output.write(buffer);
                    }
                    output.force(true);
                } catch (FileAlreadyExistsException taken) {
                    skipped++;
                    report.append("\n").append(name).append(": skipped (slot occupied)");
                    Log.i(TAG, name + ": occupied during installation; not overwritten");
                    continue;
                }
                installed++;
                report.append("\n").append(name).append(": copied");
                Log.i(TAG, name + ": copied to private DEV profile");
            } finally {
                Files.deleteIfExists(staged.toPath());
            }
        }
        return report.insert(0, "Copied " + installed + ", skipped " + skipped + ".\n")
                .toString();
    }

    private static final class Entry {
        final String hash;
        final long bytes;
        Entry(String hash, long bytes) { this.hash = hash; this.bytes = bytes; }
    }

    private static Map<String, Entry> loadManifest(AssetManager assets) throws IOException {
        final ByteArrayOutputStream content = new ByteArrayOutputStream();
        try (InputStream input = assets.open("dev-save-fixtures-manifest.sha256")) {
            byte[] buffer = new byte[1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                if (content.size() + count > MAX_MANIFEST)
                    throw new IOException("DEV save fixture manifest is oversized");
                content.write(buffer, 0, count);
            }
        }
        String[] lines = content.toString(StandardCharsets.UTF_8.name()).split("\n");
        if (lines.length != 9 || !VERSION.equals(lines[0]))
            throw new IOException("Incomplete or unsupported DEV save manifest");
        Map<String, Entry> parsed = new LinkedHashMap<>();
        for (int i = 1; i < lines.length; i++) {
            final String[] cols = lines[i].split("\t");
            if (cols.length != 3 ||
                    !(SLOT.matcher(cols[0]).matches() || "GlobalFlag.DAT".equals(cols[0])) ||
                    !cols[1].matches("[a-f0-9]{64}") || parsed.containsKey(cols[0]))
                throw new IOException("Invalid DEV fixture manifest entry " + i);
            final long bytes;
            try {
                bytes = Long.parseLong(cols[2]);
            } catch (NumberFormatException error) {
                throw new IOException("Invalid DEV fixture size " + cols[0], error);
            }
            if (bytes <= 0 || bytes > MAX_SAVE_BYTES)
                throw new IOException("Invalid DEV fixture size " + cols[0]);
            parsed.put(cols[0], new Entry(cols[1], bytes));
        }
        for (int slot = 0; slot < 7; slot++) {
            String name = String.format(java.util.Locale.ROOT, "SaveFile%03d.DAT", slot);
            if (!parsed.containsKey(name))
                throw new IOException("Missing required DEV fixture " + name);
        }
        if (!parsed.containsKey("GlobalFlag.DAT"))
            throw new IOException("Missing DEV GlobalFlag.DAT");
        return parsed;
    }

    private static MessageDigest sha() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException error) {
            throw new IllegalStateException("Android SHA-256 unavailable", error);
        }
    }

    private static String hex(byte[] value) {
        char[] digits = "0123456789abcdef".toCharArray();
        StringBuilder out = new StringBuilder(value.length * 2);
        for (byte v : value) {
            out.append(digits[(v & 0xff) >>> 4]).append(digits[v & 15]);
        }
        return out.toString();
    }
}
