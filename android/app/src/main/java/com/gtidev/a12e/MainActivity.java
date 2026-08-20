package com.gtidev.a12e;

import android.Manifest;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    private static final String PREF_NAME = "a12e_prefs";
    private static final String KEY_IP = "server_ip";
    private static final String DEFAULT_IP = "192.168.4.1";
    private static final long UPDATE_INTERVAL_MS = 1000;
    private static final int HTTP_TIMEOUT_MS = 5000;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService executor = Executors.newSingleThreadExecutor();

    private String serverIp = DEFAULT_IP;
    private boolean isConnected = false;
    private boolean initialized = false;

    private TextView weightValue;
    private TextView connectionStatus;
    private TextView errorBox;
    private TextView serverValue;
    private TextView subtitle;
    private Button serverChange;

    private WifiManager wifiManager;
    private ConnectivityManager connectivityManager;

    private Button mode1Auto;
    private Button mode1Manual;
    private Button set1Btn;
    private Button on1Btn;
    private Button off1Btn;
    private EditText threshold1Input;
    private View status1Indicator;
    private TextView status1Text;

    private Button mode2Auto;
    private Button mode2Manual;
    private Button set2Btn;
    private Button on2Btn;
    private Button off2Btn;
    private EditText threshold2Input;
    private View status2Indicator;
    private TextView status2Text;

    private final Runnable poller = new Runnable() {
        @Override
        public void run() {
            fetchStatus();
            handler.postDelayed(this, UPDATE_INTERVAL_MS);
        }
    };

    private final ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
        @Override
        public void onAvailable(Network network) {
            updateSubtitle();
        }

        @Override
        public void onLost(Network network) {
            updateSubtitle();
        }

        @Override
        public void onCapabilitiesChanged(Network network, NetworkCapabilities caps) {
            if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                updateSubtitle();
            }
        }
    };

    private final BroadcastReceiver wifiReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            updateSubtitle();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SharedPreferences prefs = getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        serverIp = prefs.getString(KEY_IP, DEFAULT_IP);

        wifiManager = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        connectivityManager = (ConnectivityManager) getApplicationContext()
                .getSystemService(Context.CONNECTIVITY_SERVICE);

        bindViews();
        setupListeners();
        updateServerLabel();
        requestWifiPermissionIfNeeded();
        registerWifiCallbacks();
        updateSubtitle();

        handler.post(poller);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
        try {
            unregisterReceiver(wifiReceiver);
        } catch (Exception ignored) {
        }
        if (connectivityManager != null) {
            connectivityManager.unregisterNetworkCallback(networkCallback);
        }
        executor.shutdownNow();
    }

    private void bindViews() {
        weightValue = findViewById(R.id.weightValue);
        connectionStatus = findViewById(R.id.connectionStatus);
        errorBox = findViewById(R.id.errorBox);
        serverValue = findViewById(R.id.serverValue);
        subtitle = findViewById(R.id.subtitle);
        serverChange = findViewById(R.id.serverChange);

        mode1Auto = findViewById(R.id.mode1Auto);
        mode1Manual = findViewById(R.id.mode1Manual);
        set1Btn = findViewById(R.id.set1Btn);
        on1Btn = findViewById(R.id.on1Btn);
        off1Btn = findViewById(R.id.off1Btn);
        threshold1Input = findViewById(R.id.threshold1Input);
        status1Indicator = findViewById(R.id.status1Indicator);
        status1Text = findViewById(R.id.status1Text);

        mode2Auto = findViewById(R.id.mode2Auto);
        mode2Manual = findViewById(R.id.mode2Manual);
        set2Btn = findViewById(R.id.set2Btn);
        on2Btn = findViewById(R.id.on2Btn);
        off2Btn = findViewById(R.id.off2Btn);
        threshold2Input = findViewById(R.id.threshold2Input);
        status2Indicator = findViewById(R.id.status2Indicator);
        status2Text = findViewById(R.id.status2Text);
    }

    private void setupListeners() {
        serverChange.setOnClickListener(v -> showServerDialog());

        mode1Auto.setOnClickListener(v -> setMode(1, "auto"));
        mode1Manual.setOnClickListener(v -> setMode(1, "manual"));
        on1Btn.setOnClickListener(v -> setRelay(1, 1));
        off1Btn.setOnClickListener(v -> setRelay(1, 0));
        set1Btn.setOnClickListener(v -> setThreshold(1, threshold1Input));

        mode2Auto.setOnClickListener(v -> setMode(2, "auto"));
        mode2Manual.setOnClickListener(v -> setMode(2, "manual"));
        on2Btn.setOnClickListener(v -> setRelay(2, 1));
        off2Btn.setOnClickListener(v -> setRelay(2, 0));
        set2Btn.setOnClickListener(v -> setThreshold(2, threshold2Input));
    }

    // ============================================================================
    // WiFi SSID (device identification)
    // ============================================================================

    private static final int REQUEST_LOCATION_PERMISSION = 100;

    private void requestWifiPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
                    != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                        REQUEST_LOCATION_PERMISSION);
            }
        }
    }

    private void registerWifiCallbacks() {
        try {
            IntentFilter filter = new IntentFilter();
            filter.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
            registerReceiver(wifiReceiver, filter);
        } catch (Exception ignored) {
        }
        if (connectivityManager != null) {
            NetworkRequest request = new NetworkRequest.Builder()
                    .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                    .build();
            connectivityManager.registerNetworkCallback(request, networkCallback);
        }
    }

    private void updateSubtitle() {
        runOnUiThread(() -> {
            String ssid = getCurrentSsid();
            if (ssid != null && !ssid.isEmpty()) {
                subtitle.setText(ssid);
            } else {
                subtitle.setText(R.string.subtitle);
            }
        });
    }

    private String getCurrentSsid() {
        try {
            if (wifiManager != null) {
                WifiInfo info = wifiManager.getConnectionInfo();
                if (info != null) {
                    String ssid = info.getSSID();
                    if (ssid != null) {
                        ssid = ssid.trim();
                        if (ssid.startsWith("\"") && ssid.endsWith("\"") && ssid.length() >= 2) {
                            ssid = ssid.substring(1, ssid.length() - 1);
                        }
                        if (!ssid.isEmpty() && !"<unknown ssid>".equalsIgnoreCase(ssid)) {
                            return ssid;
                        }
                    }
                }
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_LOCATION_PERMISSION) {
            updateSubtitle();
        }
    }

    // ============================================================================
    // Networking
    // ============================================================================

    private String httpGet(String path) throws Exception {
        URL url = new URL("http://" + serverIp + path);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setConnectTimeout(HTTP_TIMEOUT_MS);
        conn.setReadTimeout(HTTP_TIMEOUT_MS);
        conn.setRequestMethod("GET");
        int code = conn.getResponseCode();
        if (code != 200) {
            conn.disconnect();
            throw new Exception("HTTP " + code);
        }
        BufferedReader reader = new BufferedReader(
                new InputStreamReader(conn.getInputStream(), "UTF-8"));
        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {
            sb.append(line);
        }
        reader.close();
        conn.disconnect();
        return sb.toString();
    }

    private void fetchStatus() {
        executor.execute(() -> {
            try {
                String json = httpGet("/api/status");
                JSONObject obj = new JSONObject(json);
                runOnUiThread(() -> applyStatus(obj));
            } catch (Exception e) {
                runOnUiThread(this::setDisconnected);
            }
        });
    }

    private void sendCommand(String path) {
        executor.execute(() -> {
            try {
                httpGet(path);
                runOnUiThread(this::fetchStatus);
            } catch (Exception e) {
                runOnUiThread(this::setDisconnected);
            }
        });
    }

    // ============================================================================
    // UI updates
    // ============================================================================

    private void applyStatus(JSONObject obj) {
        try {
            weightValue.setText(String.format("%.1f", obj.getDouble("weight")));
            setRelayStatus(1, obj.getBoolean("relay1"));
            setRelayStatus(2, obj.getBoolean("relay2"));

            if (!initialized) {
                initialized = true;
                threshold1Input.setText(String.format("%.1f", obj.getDouble("relay1_threshold")));
                threshold2Input.setText(String.format("%.1f", obj.getDouble("relay2_threshold")));
                updateModeUI(1, obj.getBoolean("autoMode1"));
                updateModeUI(2, obj.getBoolean("autoMode2"));
            }
            setConnected();
        } catch (Exception ignored) {
        }
    }

    private void setConnected() {
        if (!isConnected) {
            isConnected = true;
            errorBox.setVisibility(View.GONE);
            connectionStatus.setText(getString(R.string.connected));
            connectionStatus.setTextColor(ContextCompat.getColor(this, R.color.green));
        }
    }

    private void setDisconnected() {
        if (isConnected) {
            isConnected = false;
            errorBox.setVisibility(View.VISIBLE);
            connectionStatus.setText(getString(R.string.disconnected));
            connectionStatus.setTextColor(ContextCompat.getColor(this, R.color.red));
        }
    }

    private void setRelayStatus(int relayNum, boolean on) {
        View indicator = relayNum == 1 ? status1Indicator : status2Indicator;
        TextView text = relayNum == 1 ? status1Text : status2Text;
        indicator.setBackgroundResource(on ? R.drawable.bg_status_on : R.drawable.bg_status_off);
        text.setText(on ? "ON" : "OFF");
    }

    private void updateModeUI(int relayNum, boolean isAuto) {
        Button autoBtn = relayNum == 1 ? mode1Auto : mode2Auto;
        Button manualBtn = relayNum == 1 ? mode1Manual : mode2Manual;
        Button onBtn = relayNum == 1 ? on1Btn : on2Btn;
        Button offBtn = relayNum == 1 ? off1Btn : off2Btn;

        Button activeBtn = isAuto ? autoBtn : manualBtn;
        Button inactiveBtn = isAuto ? manualBtn : autoBtn;
        styleModeButton(activeBtn, true);
        styleModeButton(inactiveBtn, false);

        boolean enabled = !isAuto;
        onBtn.setEnabled(enabled);
        offBtn.setEnabled(enabled);
        onBtn.setAlpha(enabled ? 1.0f : 0.4f);
        offBtn.setAlpha(enabled ? 1.0f : 0.4f);
    }

    private void styleModeButton(Button btn, boolean active) {
        if (active) {
            btn.setBackgroundTintList(ColorStateList.valueOf(
                    ContextCompat.getColor(this, R.color.primary)));
            btn.setTextColor(ContextCompat.getColor(this, R.color.white));
        } else {
            btn.setBackgroundTintList(ColorStateList.valueOf(
                    ContextCompat.getColor(this, R.color.white)));
            btn.setTextColor(ContextCompat.getColor(this, R.color.text_secondary));
        }
    }

    // ============================================================================
    // Commands
    // ============================================================================

    private void setRelay(int relayNum, int state) {
        sendCommand("/api/relay?relay=" + relayNum + "&state=" + state);
    }

    private void setMode(int relayNum, String mode) {
        updateModeUI(relayNum, mode.equals("auto"));
        sendCommand("/api/mode?relay=" + relayNum + "&mode=" + mode);
    }

    private void setThreshold(int relayNum, EditText input) {
        String raw = input.getText().toString().trim();
        float value;
        try {
            value = Float.parseFloat(raw);
        } catch (NumberFormatException e) {
            input.setError("Invalid value");
            return;
        }
        if (value < 0) {
            input.setError("Invalid value");
            return;
        }
        sendCommand("/api/threshold?relay=" + relayNum + "&value=" + value);
    }

    // ============================================================================
    // Server configuration
    // ============================================================================

    private void updateServerLabel() {
        serverValue.setText(serverIp);
    }

    private void showServerDialog() {
        LayoutInflater inflater = LayoutInflater.from(this);
        View dialogView = inflater.inflate(R.layout.dialog_server, null);
        EditText input = dialogView.findViewById(R.id.serverIpInput);
        input.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_URI);
        input.setText(serverIp);

        new AlertDialog.Builder(this)
                .setTitle(R.string.server_label)
                .setView(dialogView)
                .setNegativeButton(R.string.cancel, null)
                .setPositiveButton(R.string.ok, (dialog, which) -> {
                    String value = input.getText().toString().trim();
                    if (value.isEmpty()) {
                        return;
                    }
                    if (!value.startsWith("http://") && !value.startsWith("https://")) {
                        value = "http://" + value;
                    }
                    try {
                        URL url = new URL(value);
                        serverIp = url.getHost();
                        if (url.getPort() != -1) {
                            serverIp = serverIp + ":" + url.getPort();
                        }
                    } catch (Exception e) {
                        serverIp = value;
                    }
                    getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
                            .edit().putString(KEY_IP, serverIp).apply();
                    updateServerLabel();
                    initialized = false;
                    isConnected = false;
                    connectionStatus.setText(getString(R.string.connecting));
                    connectionStatus.setTextColor(ContextCompat.getColor(this, R.color.text_secondary));
                    fetchStatus();
                })
                .show();
    }
}
