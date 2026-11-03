package org.libsdl.app;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Color;
import android.hardware.input.InputManager;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.nio.charset.Charset;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Read-only Android/SDL2 controller, GPU and firmware diagnostic activity.
 * API19+, no root, no input injection, no permissions for contacts/network.
 * No Android IDs, addresses, MACs, serial numbers, accounts or locations logged.
 */
public class FITDDiagnosticActivity extends SDLActivity implements InputManager.InputDeviceListener {
    private static final String TAG = "FITD-DIAG";
    private static final int REQUEST_SAVE = 815;
    private static final long MAX_LOG_BYTES = 1500000L;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final Object fileLock = new Object();
    private InputManager manager;
    private TextView status;
    private File javaReport;
    private File nativeReport;
    private long lastMotion = 0;
    private long lastUi = 0;
    private int keyEvents = 0;
    private int motionEvents = 0;
    private String lastEvent = "Aguardando comandos...";

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        File parent = getExternalFilesDir(null);
        if (parent == null) parent = getFilesDir();
        javaReport = new File(parent, "fitd-android-input.txt");
        nativeReport = new File(parent, "fitd-sdl2-input.txt");
        if (state == null) {
            // Reset previous run, preserving data only until the next launch.
            try { new FileOutputStream(javaReport, false).close(); }
            catch (IOException e) { Log.w(TAG, "Cannot reset log", e); }
        }
        append("FITD-ANDROID-DIAGNOSTIC v0.2; ARMv7 API19 GLES2");
        append("Start: " + new Date().toString());
        append("Files are private to this app. No root, camera, microphone or network requested.");
        writeSystemInfo();
        manager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        if (manager != null) manager.registerInputDeviceListener(this, ui);
        enumerateDevices();
        buildOverlay();
    }

    private static String hex(int n) {
        return "0x" + Integer.toHexString(n);
    }

    private void append(String message) {
        Log.i(TAG, message);
        synchronized (fileLock) {
            if (javaReport == null) return;
            if (javaReport.length() > MAX_LOG_BYTES) return;
            try {
                FileWriter writer = new FileWriter(javaReport, true);
                try { writer.write(System.currentTimeMillis() + " " + message + "\n"); }
                finally { writer.close(); }
            } catch (IOException e) { Log.w(TAG, "Cannot write diagnostic", e); }
        }
    }

    private void writeSystemInfo() {
        append("SYSTEM manufacturer=" + Build.MANUFACTURER + " model=" + Build.MODEL +
                " device=" + Build.DEVICE + " product=" + Build.PRODUCT +
                " board=" + Build.BOARD + " hardware=" + Build.HARDWARE);
        append("ANDROID release=" + Build.VERSION.RELEASE +
                " sdk=" + Build.VERSION.SDK_INT +
                " incremental=" + Build.VERSION.INCREMENTAL);
        append("CPU_ABI " + Build.CPU_ABI + " ABI2 " + Build.CPU_ABI2);
        if (Build.VERSION.SDK_INT >= 21) {
            StringBuilder abis = new StringBuilder();
            for (String abi : Build.SUPPORTED_ABIS) abis.append(abi).append(" ");
            append("SUPPORTED_ABIS " + abis.toString());
        }
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        append("DISPLAY " + metrics.widthPixels + "x" + metrics.heightPixels +
                " densityDpi=" + metrics.densityDpi);
        try {
            WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
            append("DISPLAY refreshHz=" + wm.getDefaultDisplay().getRefreshRate());
            ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
            android.content.pm.ConfigurationInfo info = am.getDeviceConfigurationInfo();
            append("GRAPHICS requiredOpenGLES=" + hex(info.reqGlEsVersion));
        } catch (Exception e) { append("GRAPHICS query error=" + e.getClass().getSimpleName()); }
        PackageManager pm = getPackageManager();
        append("FEATURE bluetooth=" + pm.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH) +
                " touchscreen=" + pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN));
        try {
            AudioManager audio = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            append("AUDIO sampleRate=" + audio.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE) +
                    " framesPerBuffer=" + audio.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));
        } catch (Exception e) { append("AUDIO query error=" + e.getClass().getSimpleName()); }
    }

    private void enumerateDevices() {
        append("INPUTDEVICE_SCAN_BEGIN");
        int[] ids = InputDevice.getDeviceIds();
        append("INPUTDEVICE_COUNT " + ids.length);
        for (int id : ids) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null) continue;
            append("INPUTDEVICE id=" + id + " name=" + d.getName() +
                " sources=" + hex(d.getSources()) + " keyboardType=" + d.getKeyboardType() +
                " vendor=" + (Build.VERSION.SDK_INT >= 19 ? d.getVendorId() : -1) +
                " product=" + (Build.VERSION.SDK_INT >= 19 ? d.getProductId() : -1) +
                " virtual=" + (Build.VERSION.SDK_INT >= 16 && d.isVirtual()));
            List<InputDevice.MotionRange> ranges = d.getMotionRanges();
            for (InputDevice.MotionRange range : ranges) {
                append("MOTION_RANGE device=" + id + " axis=" + range.getAxis() +
                  " source=" + hex(range.getSource()) +
                  " min=" + range.getMin() + " max=" + range.getMax() +
                  " flat=" + range.getFlat() + " fuzz=" + range.getFuzz());
            }
        }
        append("INPUTDEVICE_SCAN_END");
        updateStatus();
    }

    private void buildOverlay() {
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(dp(8), dp(5), dp(8), dp(5));
        panel.setBackgroundColor(Color.argb(205, 12, 15, 28));
        status = new TextView(this);
        status.setTextColor(Color.WHITE);
        status.setTextSize(12);
        status.setText("FITD Diagnóstico: pressione botões do iPega e do controle remoto");
        panel.addView(status);
        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        Button rescan = new Button(this);
        rescan.setText("Reexaminar controles");
        rescan.setTextSize(11);
        rescan.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { enumerateDevices(); }
        });
        actions.addView(rescan, new LinearLayout.LayoutParams(0, dp(48), 1));
        Button save = new Button(this);
        save.setText("SALVAR TXT");
        save.setTextSize(12);
        save.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { saveReport(); }
        });
        actions.addView(save, new LinearLayout.LayoutParams(0, dp(48), 1));
        panel.addView(actions);
        android.widget.FrameLayout.LayoutParams lp =
            new android.widget.FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM);
        addContentView(panel, lp);
        updateStatus();
    }

    private int dp(int pixels) {
        return Math.round(pixels * getResources().getDisplayMetrics().density);
    }

    private void updateStatus() {
        if (status == null) return;
        long now = SystemClock.uptimeMillis();
        if (now - lastUi < 250) return;
        lastUi = now;
        status.setText("FITD DIAGNÓSTICO | teclas=" + keyEvents +
            " movimento=" + motionEvents + "\n" + lastEvent +
            "\nTeste iPega + remoto; depois toque em SALVAR TXT");
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        if (e.getAction() == KeyEvent.ACTION_DOWN || e.getAction() == KeyEvent.ACTION_UP) {
            keyEvents++;
            lastEvent = "Android KEY device=" + e.getDeviceId() +
                " source=" + hex(e.getSource()) + " key=" + KeyEvent.keyCodeToString(e.getKeyCode()) +
                " scan=" + e.getScanCode() + " action=" + e.getAction();
            if (e.getAction() == KeyEvent.ACTION_DOWN || e.getRepeatCount() == 0) append(lastEvent);
            updateStatus();
        }
        return super.dispatchKeyEvent(e);
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        int source = e.getSource();
        if ((source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK ||
            (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (source & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD) {
            motionEvents++;
            long now = SystemClock.uptimeMillis();
            if (now - lastMotion >= 200) {
                lastMotion = now;
                StringBuilder values = new StringBuilder();
                InputDevice device = e.getDevice();
                if (device != null) {
                    for (InputDevice.MotionRange range : device.getMotionRanges()) {
                        if ((range.getSource() & source) != 0) {
                            float v = e.getAxisValue(range.getAxis());
                            if (Math.abs(v) >= 0.10f) values.append(" axis")
                                .append(range.getAxis()).append("=").append(v);
                        }
                    }
                }
                lastEvent = "Android MOTION device=" + e.getDeviceId() +
                    " source=" + hex(source) + " action=" + e.getActionMasked() +
                    values.toString();
                append(lastEvent);
                updateStatus();
            }
        }
        return super.dispatchGenericMotionEvent(e);
    }

    @Override public void onInputDeviceAdded(int id) {
        append("ANDROID_INPUT_ADDED id=" + id);
        enumerateDevices();
    }
    @Override public void onInputDeviceRemoved(int id) {
        append("ANDROID_INPUT_REMOVED id=" + id);
        enumerateDevices();
    }
    @Override public void onInputDeviceChanged(int id) {
        append("ANDROID_INPUT_CHANGED id=" + id);
        enumerateDevices();
    }

    private void saveReport() {
        append("EXPORT_REQUEST");
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TITLE, "FITD-Android-ARMv7-Diagnostico.txt");
        try {
            startActivityForResult(intent, REQUEST_SAVE);
        } catch (Exception e) {
            append("EXPORT_PICKER_UNAVAILABLE " + e);
            Toast.makeText(this,
                "Seletor indisponível. Logs em Android/data/" + getPackageName() + "/files/",
                Toast.LENGTH_LONG).show();
        }
    }

    private void writeOneFile(OutputStreamWriter writer, File file, String section) throws IOException {
        writer.write("\n========== " + section + " ==========\n");
        if (!file.isFile()) {
            writer.write("Arquivo ainda não disponível\n");
            return;
        }
        BufferedReader br = new BufferedReader(new InputStreamReader(
            new FileInputStream(file), Charset.forName("UTF-8")));
        try {
            String line;
            while ((line = br.readLine()) != null) {
                writer.write(line);
                writer.write('\n');
            }
        } finally { br.close(); }
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != REQUEST_SAVE || result != RESULT_OK || data == null || data.getData() == null)
            return;
        Uri uri = data.getData();
        try {
            OutputStream output = getContentResolver().openOutputStream(uri);
            if (output == null) throw new IOException("Empty document stream");
            try {
                OutputStreamWriter w = new OutputStreamWriter(output, Charset.forName("UTF-8"));
                w.write("FITD Android ARMv7 diagnostic / Android 4.4+ / SDL2 GLES2\n");
                w.write("No personal accounts, MAC, serial or location intentionally collected.\n");
                writeOneFile(w, javaReport, "ANDROID INPUT / FIRMWARE");
                writeOneFile(w, nativeReport, "SDL2 / OPENGL ES / GAMEPAD");
                w.flush();
            } finally { output.close(); }
            append("EXPORT_SAVED file chooser completed");
            Toast.makeText(this, "Diagnóstico TXT salvo! Envie o arquivo no chat.",
                Toast.LENGTH_LONG).show();
        } catch (Exception e) {
            append("EXPORT_FAILED " + e);
            Toast.makeText(this, "Não foi possível salvar: " + e.getMessage(),
                Toast.LENGTH_LONG).show();
        }
    }

    @Override protected void onDestroy() {
        if (manager != null) manager.unregisterInputDeviceListener(this);
        append("ANDROID_ACTIVITY_DESTROYED");
        super.onDestroy();
    }
}
