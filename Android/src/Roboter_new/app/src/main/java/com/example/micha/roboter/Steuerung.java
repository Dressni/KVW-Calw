package com.example.micha.roboter;

import android.content.Context;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.util.Log;

import net.margaritov.preference.colorpicker.ColorPickerDialog;

import java.util.Arrays;

/**
 * Created by Nick on 15.07.2018.
 */

public class Steuerung implements Runnable {

    private MainActivity mainActivity;
    private UDPConnector udpConnector;
    private WifiManager wifiManager;

    private String receiveData;
    private LEDColor ledColor = LEDColor.RED();
    private boolean spkMode = false;
    private boolean demoMode = false;

    private boolean gatherData = false;


    public Steuerung(final MainActivity mainActivity) {
        this.mainActivity = mainActivity;
        this.udpConnector = new UDPConnector(6565, 1234, "192.168.4.1");
        new Thread(this).start();
    }

    public void setWifiManager(WifiManager wifiManager){
        this.wifiManager = wifiManager;
    }

    public void openColorPicker(){
        ColorPickerDialog colorPickerDialog = new ColorPickerDialog(this.mainActivity, ledColor.getColorPrefix());
        colorPickerDialog.setAlphaSliderVisible(true);
        colorPickerDialog.setHexValueEnabled(true);
        colorPickerDialog.setTitle("Farbe für LED-Arme");
        colorPickerDialog.setOnColorChangedListener(new ColorPickerDialog.OnColorChangedListener() {
            @Override
            public void onColorChanged(int color) {
                ledColor = new LEDColor(color);
                udpConnector.sendData(ledColor.getCMDColor());
                Toaster.makeToast("Neue Farbe: " + ledColor.toString());
            }
        });
        colorPickerDialog.show();
    }

    @Override
    public void run() {

        long lastMs = 0;


        while (!Thread.interrupted()) {

            if(System.currentTimeMillis() - lastMs > 1000){
                if(wifiManager == null) continue;
                WifiInfo info = wifiManager.getConnectionInfo();
                String anz = "SSID: " + info.getSSID() + "(" + info.getRssi() + "dBm)\nPhys-Speed: " + info.getLinkSpeed() + "mbps";
                mainActivity.anzText(anz);
                lastMs = System.currentTimeMillis();
            }

            if(gatherData) {
                udpConnector.sendData("REQUESTMODE?");
                udpConnector.sendData("REQUESTLEDCOLOR?");
                this.mainActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toaster.makeToast("Gathering Data...");
                    }
                });
                gatherData = false;
            }
            //Distance(m),Velocity(m/s)
            receiveData =  udpConnector.receiveData(500).trim();

            //Log.i("UDP received", receiveData);

            if(receiveData.equals("null")){
                udpConnector.sendData("DROP");
                //Log.i("Drops", "" + ++drops);
                continue;
            }
            if(receiveData.startsWith("RESPONSEAUTOTRIGGER=L")){
                this.mainActivity.showLeft();
                continue;
            }if(receiveData.startsWith("RESPONSEAUTOTRIGGER=R")){
               this.mainActivity.showRight();
                continue;
            }if(receiveData.startsWith("RESPONSEAUTOTRIGGER=NONE")) {
                this.mainActivity.showNone();
                continue;
            }if(receiveData.startsWith("RESPONSEMODE&")){
                String content[] = receiveData.split("&");
                final boolean mode = parseBoolean(content[1].trim().split("=")[1].trim());
                this.demoMode = parseBoolean(content[2].trim().split("=")[1].trim());
                this.spkMode = parseBoolean(content[3].trim().split("=")[1].trim());
                this.mainActivity.setMode(mode);
                this.mainActivity.setSpkMode(this.spkMode);
                this.mainActivity.setDemoMode(this.demoMode);
                continue;
            }if(receiveData.startsWith("RESPONSECOLOR&")) {
                String[] colorData = receiveData.split("&")[1].split(";");
                int[] colors = new int[3];
                for (byte c = 0; c < 3; c++)
                    colors[c] = Integer.parseInt(colorData[c]);
                ledColor = new LEDColor(colors);
            }
        }
    }

    public void linkerArm() {
        this.udpConnector.sendData("TRIGGER=1");
    }

    public void rechterArm() {
        this.udpConnector.sendData("TRIGGER=2");
    }

    public void keineArme() {
        this.udpConnector.sendData("TRIGGER=3");
    }

    public void beideArme() {
        this.udpConnector.sendData("TRIGGER=4");
    }

    public void setzeModus(boolean b) {
        if(b)
            this.udpConnector.sendData("SETMODE=auto");
        else
            this.udpConnector.sendData("SETMODE=manual");
    }

    public void updaten() {
        this.udpConnector.sendData(this.ledColor.getCMDColor());
        this.udpConnector.sendData("SETSPKMODE=" + this.spkMode);
        this.udpConnector.sendData("SETDEMOMODE=" + this.demoMode);
        Toaster.makeToast("Updated!");
    }

    public void updaten2() {
        udpConnector.sendData("REQUESTMODE?");
        udpConnector.sendData("REQUESTLEDCOLOR?");
        Toaster.makeToast("Updated!");
    }

    public void setzeLEDColor(LEDColor ledColor){
        this.ledColor = ledColor;
        Toaster.makeToast("Neue Farbe: " + this.ledColor.toString());
        this.udpConnector.sendData(this.ledColor.getCMDColor());
    }
    public void setzeSpkMode(boolean b){
        this.mainActivity.setSpkMode(b);
        if(b) {
            this.udpConnector.sendData("SETSPKMODE=true");
            Toaster.makeToast("Neue Farben: Sparkasse");
        }else{
            this.udpConnector.sendData("SETSPKMODE=false");
        }
    }

    public void toggleDemoMode() {
        this.demoMode = !this.demoMode;
        this.udpConnector.sendData("SETDEMOMODE=" + this.demoMode);
        this.mainActivity.setDemoMode(this.demoMode);
        Toaster.makeToast("" + this.mainActivity.gibTitelDemo());
    }
    private boolean parseBoolean(String x){
        String parserStr = x.trim();
        if(parserStr.startsWith("1")){
            return true;
        }else{
            return false;
        }
    }

    public void gatherData() {
        this.gatherData = true;
    }
}
