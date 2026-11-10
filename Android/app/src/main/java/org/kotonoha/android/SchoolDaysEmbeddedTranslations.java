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
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Installs only the translated language packs supplied by an external,
 * build-time source. Retail game data never enters the APK or this directory.
 * Existing user-owned translations are never overwritten by an update.
 */
final class SchoolDaysEmbeddedTranslations {
    private static final String VERSION = "SchoolDaysEmbeddedTranslations.v1";
    private static final String ASSET_MANIFEST = "translations-manifest.sha256";
    private static final String MANAGED_MANIFEST = ".embedded-translations.v1";
    private static final int MAX_MANIFEST = 32 * 1024;
    private static final long MAX_GPK_BYTES = 128L * 1024 * 1024;
    private static final long MAX_JSON_BYTES = 16L * 1024 * 1024;

    private SchoolDaysEmbeddedTranslations() {}

    static final class Result {
        int installed;
        int updated;
        int preserved;
        int verified;
        final List<String> protectedFiles = new ArrayList<>();

        @Override public String toString() {
            return "Translation assets: " + installed + " installed, " +
                    updated + " updated, " + verified + " verified, " +
                    preserved + " preserved (not overwritten)";
        }
    }

    static Result install(Context context, AssetManager assets) throws IOException {
        final File profile = SchoolDaysMobileProfile.root(context);
        final File translations = new File(profile, "translations");
        final File installedManifest = new File(profile, MANAGED_MANIFEST);
        final Map<String, String> packaged;
        try (InputStream manifest = assets.open(ASSET_MANIFEST)) {
            packaged = parseManifest(readLimited(manifest), true);
        }
        if (packaged.isEmpty())
            throw new IOException("APK has no School Days GPK/JSON translations");
        final Map<String, String> prior = installedManifest.isFile()
                ? parseManifest(Files.readAllBytes(installedManifest.toPath()), false)
                : new HashMap<>();
        if (!translations.isDirectory() && !translations.mkdirs())
            throw new IOException("Cannot create private translations directory");

        final Map<String, String> managedNow = new HashMap<>();
        final Result result = new Result();
        for (Map.Entry<String, String> item : packaged.entrySet()) {
            final String name = item.getKey();
            final String expected = item.getValue();
            final File target = new File(translations, name.replace('/', File.separatorChar));
            final File parent = target.getParentFile();
            if (!parent.isDirectory() && !parent.mkdirs())
                throw new IOException("Cannot create language directory: " + name);
            if (!target.getCanonicalFile().toPath().startsWith(translations.getCanonicalFile().toPath()))
                throw new IOException("Unsafe translation target: " + name);
            if (Files.isSymbolicLink(target.toPath()) ||
                    (target.exists() && !target.isFile()))
                throw new IOException("Invalid existing translation destination: " + name);

            boolean canUpdate = false;
            if (target.isFile()) {
                final String actual = sha256(target);
                if (actual.equals(expected)) {
                    result.verified++;
                    // Imported files with matching bytes are still user-owned
                    // unless our previous managed manifest attests ownership.
                    if (prior.containsKey(name)) managedNow.put(name, expected);
                    continue;
                }
                final boolean ptBr = name.contains("pt-BR") ||
                        name.endsWith("Script - Brazilian.GPK");
                // Never overwrite PT-BR content, even if a prior APK originally
                // installed it. This also protects an already-imported locale.
                canUpdate = !ptBr && actual.equals(prior.get(name));
                if (!canUpdate) {
                    result.preserved++;
                    result.protectedFiles.add(name);
                    Log.w("SchoolDaysTranslations", "User-owned locale preserved: " + name);
                    continue;
                }
            }

            final File staging = File.createTempFile(".translation-bundle-", ".tmp", parent);
            try {
                MessageDigest digest = digest();
                final long limit = name.endsWith(".GPK") ? MAX_GPK_BYTES : MAX_JSON_BYTES;
                long bytes = 0;
                try (InputStream input = assets.open("translations/" + name);
                     FileOutputStream output = new FileOutputStream(staging)) {
                    byte[] buffer = new byte[65536];
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        bytes += count;
                        if (bytes > limit)
                            throw new IOException("Packaged translation exceeds safe limit: " + name);
                        output.write(buffer, 0, count);
                        digest.update(buffer, 0, count);
                    }
                    output.getFD().sync();
                }
                if (bytes == 0 || !hex(digest.digest()).equals(expected))
                    throw new IOException("Packaged translation SHA256 mismatch: " + name);
                try {
                    if (canUpdate) {
                        Files.move(staging.toPath(), target.toPath(),
                                StandardCopyOption.ATOMIC_MOVE,
                                StandardCopyOption.REPLACE_EXISTING);
                    } else {
                        Files.move(staging.toPath(), target.toPath(),
                                StandardCopyOption.ATOMIC_MOVE);
                    }
                } catch (AtomicMoveNotSupportedException error) {
                    if (canUpdate)
                        Files.move(staging.toPath(), target.toPath(),
                                StandardCopyOption.REPLACE_EXISTING);
                    else
                        Files.move(staging.toPath(), target.toPath());
                }
                managedNow.put(name, expected);
                if (canUpdate) result.updated++;
                else result.installed++;
            } finally {
                Files.deleteIfExists(staging.toPath());
            }
        }
        List<String> lines = new ArrayList<>();
        lines.add(VERSION);
        managedNow.entrySet().stream()
                .sorted(Map.Entry.comparingByKey())
                .forEach(entry -> lines.add(entry.getKey() + "\t" + entry.getValue()));
        final File marker = File.createTempFile(".embedded-translation-", ".tmp", profile);
        try {
            try (FileOutputStream output = new FileOutputStream(marker)) {
                output.write((String.join("\n", lines) + "\n")
                        .getBytes(StandardCharsets.UTF_8));
                output.getFD().sync();
            }
            Files.move(marker.toPath(), installedManifest.toPath(),
                    StandardCopyOption.REPLACE_EXISTING);
        } finally {
            Files.deleteIfExists(marker.toPath());
        }
        return result;
    }

    private static boolean validName(String name) {
        if (name.matches("jsons/[A-Za-z0-9][A-Za-z0-9_-]{0,70}\\.json"))
            return true;
        switch (name) {
            case "scripts/en-lxh.GPK":
            case "scripts/pt-BR-lxh.GPK":
            case "scripts/es-lxh.GPK":
            case "scripts/zh-CN-lxh.GPK":
            case "scripts/Script - Brazilian.GPK":
            case "scripts/Script - Spanish.GPK":
            case "scripts/Script - chinese-compatible.GPK":
                return true;
            default:
                return false;
        }
    }

    private static Map<String, String> parseManifest(byte[] contents, boolean required)
            throws IOException {
        if (contents.length > MAX_MANIFEST)
            throw new IOException("Translation asset manifest exceeds safe size");
        final String[] lines = new String(contents, StandardCharsets.UTF_8)
                .split("\n", -1);
        if (lines.length < 2 || !VERSION.equals(lines[0]))
            throw new IOException("Unsupported translation asset manifest");
        Map<String, String> result = new java.util.LinkedHashMap<>();
        for (int i = 1; i < lines.length; i++) {
            if (lines[i].isEmpty()) continue;
            String[] fields = lines[i].split("\t", -1);
            if (fields.length != 2 || !validName(fields[0]) ||
                    !fields[1].matches("[a-f0-9]{64}") ||
                    result.containsKey(fields[0]))
                throw new IOException("Invalid translation manifest entry at line " + (i + 1));
            result.put(fields[0], fields[1]);
        }
        if (required && result.isEmpty())
            throw new IOException("Packaged translation manifest is empty");
        return result;
    }

    private static byte[] readLimited(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (output.size() + count > MAX_MANIFEST)
                throw new IOException("Translation manifest exceeds safe size");
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    private static MessageDigest digest() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException error) {
            throw new IllegalStateException("Android SHA-256 unavailable", error);
        }
    }

    private static String sha256(File input) throws IOException {
        MessageDigest digest = digest();
        try (FileInputStream stream = new FileInputStream(input)) {
            byte[] buffer = new byte[65536];
            int count;
            while ((count = stream.read(buffer)) != -1)
                digest.update(buffer, 0, count);
        }
        return hex(digest.digest());
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte b : bytes)
            result.append(String.format(Locale.ROOT, "%02x", b & 0xff));
        return result.toString();
    }
}
