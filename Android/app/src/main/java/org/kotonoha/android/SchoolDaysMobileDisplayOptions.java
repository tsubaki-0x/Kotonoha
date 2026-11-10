package org.kotonoha.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.os.Build;
import android.view.Display;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Native Android launcher counterpart to the PC/retail Display Options tab. */
final class SchoolDaysMobileDisplayOptions {
    private static final float[] RENDER_SCALES = {1.0f, .75f, .5f};
    private static final String[] SCALE_NAMES = {
            "Native (100%)", "Balanced (75%)", "Performance (50%)"};
    private static final String[] ASPECT_NAMES = {
            "Auto / fit device", "Standard 4:3", "Widescreen 16:9"};
    private static final int[] FRAME_CAPS = {0, 30, 60};
    private static final String[] FRAME_CAP_NAMES = {
            "No extra cap (VSync/device)", "Limit 30 FPS", "Limit 60 FPS"};
    private static final String[] UPSCALE_NAMES = {
            "Off", "Linear", "ArtCNN (Auto)", "ArtCNN Denoise (Auto)",
            "ArtCNN Large (Auto)", "Anime4K (Auto)"};
    private static final int[] PRESET_IDS = {0, 0, 1, 2, 3, 0};
    // C++ UpscaleBackend: None=0, Linear=1, Auto=4. Off is 1.0x.
    private static final int[] BACKEND_IDS = {0, 1, 4, 4, 4, 4};

    private SchoolDaysMobileDisplayOptions() {}

    private static final class Mode {
        final int width;
        final int height;
        final float hz;
        final String title;

        Mode(int width, int height, float hz, String title) {
            this.width = width;
            this.height = height;
            this.hz = hz;
            this.title = title;
        }

        @Override public String toString() { return title; }
    }

    private static int dp(Activity activity, int amount) {
        return (int)(activity.getResources().getDisplayMetrics().density * amount + .5f);
    }

    private static TextView caption(Activity activity, String message) {
        TextView result = new TextView(activity);
        result.setText(message);
        result.setTextSize(14);
        result.setTextColor(Color.DKGRAY);
        result.setPadding(0, dp(activity, 12), 0, dp(activity, 6));
        return result;
    }

    private static Spinner selector(Activity activity, LinearLayout panel, String title,
                                    ArrayAdapter<?> adapter, int selection) {
        panel.addView(caption(activity, title));
        Spinner spinner = new Spinner(activity);
        spinner.setAdapter(adapter);
        spinner.setMinimumHeight(dp(activity, 48));
        spinner.setSelection(selection);
        panel.addView(spinner, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(activity, 50)));
        return spinner;
    }

    private static ArrayAdapter<String> labels(Activity activity, String[] choices) {
        ArrayAdapter<String> adapter = new ArrayAdapter<>(activity,
                android.R.layout.simple_spinner_item, choices);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        return adapter;
    }

    private static JSONObject readSettings(File settings) throws IOException {
        try {
            if (!settings.isFile()) return new JSONObject();
            if (settings.length() > 1024 * 1024)
                throw new IOException("Existing settings.json exceeds 1 MiB; no changes made");
            JSONObject json = new JSONObject(new String(
                    Files.readAllBytes(settings.toPath()), StandardCharsets.UTF_8));
            if (json.optInt("version", 1) != 1)
                throw new IOException("Unsupported settings.json version; no changes made");
            return json;
        } catch (JSONException e) {
            throw new IOException("Invalid settings.json; no changes made", e);
        }
    }

    private static void atomicWrite(File target, String contents) throws IOException {
        File directory = target.getParentFile();
        if (!directory.isDirectory() && !directory.mkdirs())
            throw new IOException("Cannot create Android display profile directory");
        File staging = File.createTempFile(".display-options-", ".tmp", directory);
        try {
            try (FileOutputStream output = new FileOutputStream(staging)) {
                output.write(contents.getBytes(StandardCharsets.UTF_8));
                output.getFD().sync();
            }
            Files.move(staging.toPath(), target.toPath(),
                    StandardCopyOption.REPLACE_EXISTING);
        } finally {
            Files.deleteIfExists(staging.toPath());
        }
    }

    // The PC's Display Options and in-game Android Display tab both use
    // ContentAspectPreference= in Kotonoha.ini, not an unrecognized JSON key.
    private static int readAspect(File profile) throws IOException {
        File file = new File(profile, "Kotonoha.ini");
        if (!file.isFile()) return 0;
        if (file.length() > 1024 * 1024)
            throw new IOException("Invalid oversized Kotonoha.ini");
        for (String line : Files.readAllLines(file.toPath(), StandardCharsets.UTF_8)) {
            if (!line.startsWith("ContentAspectPreference=")) continue;
            try {
                int value = Integer.parseInt(line.substring("ContentAspectPreference=".length()).trim());
                if (value >= 0 && value <= 2) return value;
            } catch (NumberFormatException ignored) { }
            throw new IOException("Invalid existing content aspect preference");
        }
        return 0;
    }

    private static int readFrameCap(File profile) throws IOException {
        File file = new File(profile, "Kotonoha.ini");
        if (!file.isFile()) return 0;
        if (file.length() > 1024 * 1024) throw new IOException("Invalid oversized Kotonoha.ini");
        for (String line : Files.readAllLines(file.toPath(), StandardCharsets.UTF_8)) {
            if (!line.startsWith("AndroidFrameLimit=")) continue;
            try {
                int value = Integer.parseInt(line.substring("AndroidFrameLimit=".length()).trim());
                if (value == 0 || value == 30 || value == 60) return value;
            } catch (NumberFormatException ignored) { }
            throw new IOException("Invalid Android frame limit preference");
        }
        return 0;
    }

    private static void saveDisplaySidecar(File profile, int aspect, int cap) throws IOException {
        if (aspect < 0 || aspect > 2) throw new IOException("Unsupported content aspect");
        if (cap != 0 && cap != 30 && cap != 60) throw new IOException("Unsupported frame cap");
        File file = new File(profile, "Kotonoha.ini");
        List<String> lines = file.isFile() ?
                Files.readAllLines(file.toPath(), StandardCharsets.UTF_8) :
                new ArrayList<>();
        String value = "ContentAspectPreference=" + aspect;
        boolean found = false;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.get(i).startsWith("ContentAspectPreference=")) continue;
            lines.set(i, value);
            found = true;
            break;
        }
        if (!found) {
            if (lines.isEmpty()) lines.add("[KotonohaConfig][Settings]");
            lines.add(value);
        }
        boolean foundCap = false;
        String capLine = "AndroidFrameLimit=" + cap;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.get(i).startsWith("AndroidFrameLimit=")) continue;
            lines.set(i, capLine);
            foundCap = true;
            break;
        }
        if (!foundCap) lines.add(capLine);
        atomicWrite(file, String.join("\n", lines) + "\n");
    }

    private static List<Mode> deviceModes(Activity activity) {
        List<Mode> result = new ArrayList<>();
        result.add(new Mode(0, 0, 0, "Device default"));
        Display display = activity.getWindowManager().getDefaultDisplay();
        for (Display.Mode mode : display.getSupportedModes()) {
            int w = Math.max(mode.getPhysicalWidth(), mode.getPhysicalHeight());
            int h = Math.min(mode.getPhysicalWidth(), mode.getPhysicalHeight());
            float hz = mode.getRefreshRate();
            if (w <= 0 || h <= 0 || hz <= 0) continue;
            boolean duplicate = false;
            for (Mode seen : result) {
                if (seen.width == w && seen.height == h &&
                        Math.abs(seen.hz - hz) < .01f) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                String hzLabel = Math.abs(hz - Math.round(hz)) < .01f
                        ? String.format(Locale.ROOT, "%d Hz", Math.round(hz))
                        : String.format(Locale.ROOT, "%.2f Hz", hz);
                result.add(new Mode(w, h, hz, w + " × " + h + " @ " + hzLabel));
            }
        }
        return result;
    }

    static void show(Activity activity, java.util.function.Consumer<String> onResult) {
        final File profile = SchoolDaysMobileProfile.root(activity);
        final File settings = new File(profile, "settings.json");
        final JSONObject saved;
        final int aspect;
        final int frameCap;
        try {
            saved = readSettings(settings);
            aspect = readAspect(profile);
            frameCap = readFrameCap(profile);
        } catch (IOException error) {
            onResult.accept("Display Options cannot load: " + error.getMessage());
            return;
        }
        final LinearLayout controls = new LinearLayout(activity);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setPadding(dp(activity, 18), dp(activity, 4), dp(activity, 18), dp(activity, 18));
        final Spinner aspectChoice = selector(activity, controls, "CONTENT ASPECT",
                labels(activity, ASPECT_NAMES), aspect);
        final double scale = saved.optDouble("renderScale", .75);
        final int scaleIndex = scale > .875 ? 0 : scale > .625 ? 1 : 2;
        final Spinner scaleChoice = selector(activity, controls, "RENDER RESOLUTION",
                labels(activity, SCALE_NAMES), scaleIndex);

        final List<Mode> modes = deviceModes(activity);
        int modeIndex = 0;
        for (int i = 1; i < modes.size(); i++) {
            Mode mode = modes.get(i);
            if (mode.width == saved.optInt("fullscreenWidth", 0) &&
                    mode.height == saved.optInt("fullscreenHeight", 0) &&
                    Math.abs(mode.hz - saved.optDouble("refreshRate", 0)) < .05)
                modeIndex = i;
        }
        ArrayAdapter<Mode> modeAdapter = new ArrayAdapter<>(activity,
                android.R.layout.simple_spinner_item, modes);
        modeAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        final Spinner frameRate = selector(activity, controls,
                "SUPPORTED DISPLAY REFRESH RATE", modeAdapter, modeIndex);
        controls.addView(caption(activity,
                "Select only modes reported by Android. This requests screen refresh; " +
                        "it does not change the 24 FPS timing of scripted videos."));
        final Spinner capChoice = selector(activity, controls,
                "MAXIMUM RENDERED FRAMES / SECOND", labels(activity, FRAME_CAP_NAMES),
                frameCap == 30 ? 1 : frameCap == 60 ? 2 : 0);

        int upscaleIndex = 0;
        int backend = saved.optInt("upscaler", 4);
        int preset = saved.optInt("preset", 1);
        double factor = saved.optDouble("factor", 2.0);
        if (backend == 1 && factor > 1) upscaleIndex = 1;
        else if (backend != 0 && factor > 1) {
            upscaleIndex = preset == 2 ? 3 : preset == 3 ? 4 : preset == 0 ? 5 : 2;
        }
        final Spinner upscale = selector(activity, controls,
                "UPSCALER / PRESET", labels(activity, UPSCALE_NAMES), upscaleIndex);
        controls.addView(caption(activity,
                "GLES2 uses compatible built-in pipelines where available. Auto may " +
                        "use linear scaling if GLSL is unavailable; custom desktop shaders " +
                        "and MetalFX are not offered."));
        final CheckBox vsync = new CheckBox(activity);
        vsync.setText("VSync");
        vsync.setChecked(saved.optBoolean("vsync", true));
        vsync.setMinHeight(dp(activity, 48));
        controls.addView(vsync);

        ScrollView scroll = new ScrollView(activity);
        scroll.addView(controls);
        final AlertDialog dialog = new AlertDialog.Builder(activity)
                .setTitle("Display Options — Android")
                .setView(scroll)
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Save", null)
                .create();
        dialog.setOnShowListener(ignored -> dialog.getButton(AlertDialog.BUTTON_POSITIVE)
                .setOnClickListener(view -> {
                    try {
                        JSONObject updated = readSettings(settings);
                        updated.put("version", 1);
                        updated.put("mode", 1); // WindowMode::Borderless
                        updated.put("display", 0);
                        updated.put("renderer", "opengles2");
                        updated.put("renderScale", RENDER_SCALES[scaleChoice.getSelectedItemPosition()]);
                        updated.put("vsync", vsync.isChecked());
                        Mode chosen = modes.get(frameRate.getSelectedItemPosition());
                        updated.put("fullscreenWidth", chosen.width);
                        updated.put("fullscreenHeight", chosen.height);
                        updated.put("refreshRate", chosen.hz);
                        int presetChoice = upscale.getSelectedItemPosition();
                        updated.put("upscaler", BACKEND_IDS[presetChoice]);
                        updated.put("preset", PRESET_IDS[presetChoice]);
                        updated.put("factor", presetChoice == 0 ? 1.0 : 2.0);
                        updated.put("shaderFiles", "");
                        final int newAspect = aspectChoice.getSelectedItemPosition();
                        // Same profile/format as the native in-game Display tab.
                        atomicWrite(settings, updated.toString());
                        saveDisplaySidecar(profile, newAspect,
                                FRAME_CAPS[capChoice.getSelectedItemPosition()]);
                        onResult.accept("Display Options saved — applied on next Play.");
                        dialog.dismiss();
                    } catch (IOException | JSONException error) {
                        onResult.accept("Display Options NOT saved: " + error.getMessage());
                    }
                }));
        dialog.show();
    }
}
