package com.example.micha.roboter;


import android.content.ClipData;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.support.design.widget.NavigationView;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.ColorUtils;
import android.support.v4.view.GravityCompat;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBarDrawerToggle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.ToggleButton;

import net.margaritov.preference.colorpicker.ColorPickerDialog;

import java.util.ArrayList;
import java.util.Arrays;

public class MainActivity extends AppCompatActivity
        implements View.OnClickListener,NavigationView.OnNavigationItemSelectedListener {


    private Button rightS, leftS, noArms;
    private ToggleButton zufButton;
    private ImageView rechts,links,oben,unten,zufImage;
    private Switch perspektive;
    private Menu menü;
    private NavigationView navigationView;
    private TextView anzTextView;

    private Steuerung dieSteuerung;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        new Toaster(this);

        rightS = (Button) findViewById(R.id.rechterSwitch);
        leftS = (Button) findViewById(R.id.linkerSwitch);
        noArms = (Button) findViewById(R.id.noArms);
        rightS.setOnClickListener(this);
        leftS.setOnClickListener(this);
        noArms.setOnClickListener(this);
        leftS.setVisibility(View.VISIBLE);
        rightS.setVisibility(View.VISIBLE);
        rightS.getBackground().setAlpha(0);
        leftS.getBackground().setAlpha(0);
        noArms.getBackground().setAlpha(0);


        rechts = (ImageView) findViewById(R.id.rechts);
        links = (ImageView) findViewById(R.id.links);
        oben = (ImageView) findViewById(R.id.oben);
        unten = (ImageView) findViewById(R.id.unten);
        zufImage = (ImageView) findViewById(R.id.zufÌmage);
        perspektive = (Switch) findViewById(R.id.switch1);
        this.anzTextView = (TextView) findViewById(R.id.textView);

        zufButton = (ToggleButton) findViewById(R.id.zufButton);
        zufButton.setOnClickListener(this);
        ImageView zufImage = (ImageView) findViewById(R.id.zufÌmage);
        zufImage.setVisibility(View.INVISIBLE);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        DrawerLayout drawer = (DrawerLayout) findViewById(R.id.drawer_layout);
        ActionBarDrawerToggle toggle = new ActionBarDrawerToggle(this, drawer, toolbar, R.string.navigation_drawer_open, R.string.navigation_drawer_close);
        drawer.addDrawerListener(toggle);
        toggle.syncState();

        navigationView = (NavigationView) findViewById(R.id.nav_view);
        navigationView.setNavigationItemSelectedListener(this);

        this.dieSteuerung = new Steuerung(this);
        this.dieSteuerung.setWifiManager((WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE));
    }


    public void setSpkMode(final boolean spkMode) {
        if(navigationView == null)
            return;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if(spkMode) {
                    navigationView.getMenu().getItem(4).setChecked(true);
                } else {
                    navigationView.getMenu().getItem(3).setChecked(true);
                    navigationView.getMenu().getItem(4).setChecked(false);
                }
            }
        });
    }

    public void setDemoMode(final boolean demo){
        if(this.menü == null)
            return;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                menü.getItem(1).setTitle(new String("Demo: " + (demo ? "ein" : "aus")));
            }
        });
    }

    public String gibTitelDemo() {
        if(this.menü == null)
            return null;
        return "" + this.menü.getItem(1).getTitle();
    }

    public void setMode(final boolean b) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
            if(b){
                rechts.setVisibility(View.INVISIBLE);
                links.setVisibility(View.INVISIBLE);
                oben.setVisibility(View.INVISIBLE);
                unten.setVisibility(View.INVISIBLE);
                zufImage.setVisibility(View.VISIBLE);

                leftS.setVisibility(View.INVISIBLE);
                rightS.setVisibility(View.INVISIBLE);
                noArms.setVisibility(View.INVISIBLE);
                zufButton.setChecked(true);
            }else{
                rechts.setVisibility(View.INVISIBLE);
                links.setVisibility(View.INVISIBLE);
                oben.setVisibility(View.INVISIBLE);
                unten.setVisibility(View.VISIBLE);
                zufImage.setVisibility(View.INVISIBLE);

                leftS.setVisibility(View.VISIBLE);
                rightS.setVisibility(View.VISIBLE);
                noArms.setVisibility(View.VISIBLE);
                zufButton.setChecked(false);
            }
            }
        });
    }

    public void showLeft(){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                rechts.setVisibility(View.INVISIBLE);
                links.setVisibility(View.VISIBLE);
                oben.setVisibility(View.INVISIBLE);
                unten.setVisibility(View.INVISIBLE);
                zufImage.setVisibility(View.INVISIBLE);
            }
        });
    }
    public void showRight(){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                rechts.setVisibility(View.VISIBLE);
                links.setVisibility(View.INVISIBLE);
                oben.setVisibility(View.INVISIBLE);
                unten.setVisibility(View.INVISIBLE);
                zufImage.setVisibility(View.INVISIBLE);
            }
        });
    }
    public void showNone(){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                rechts.setVisibility(View.INVISIBLE);
                links.setVisibility(View.INVISIBLE);
                oben.setVisibility(View.INVISIBLE);
                unten.setVisibility(View.INVISIBLE);
                zufImage.setVisibility(View.VISIBLE);
            }
        });}

    @Override
    public void onClick(View v) {

        rechts.setVisibility(View.INVISIBLE);
        links.setVisibility(View.INVISIBLE);
        oben.setVisibility(View.INVISIBLE);
        unten.setVisibility(View.VISIBLE);
        zufImage.setVisibility(View.INVISIBLE);

        if(noArms.isPressed()){
            this.dieSteuerung.keineArme();
            rechts.setVisibility(View.INVISIBLE);
            links.setVisibility(View.INVISIBLE);
            oben.setVisibility(View.INVISIBLE);
            unten.setVisibility(View.VISIBLE);
            return;
        }

        //Setmode auto
        if (zufButton.isChecked()){

            rechts.setVisibility(View.INVISIBLE);
            links.setVisibility(View.INVISIBLE);
            oben.setVisibility(View.INVISIBLE);
            unten.setVisibility(View.INVISIBLE);
            zufImage.setVisibility(View.VISIBLE);

            leftS.setVisibility(View.INVISIBLE);
            rightS.setVisibility(View.INVISIBLE);
            noArms.setVisibility(View.INVISIBLE);
            this.dieSteuerung.keineArme();
            this.dieSteuerung.setzeModus(true);
            return;
        }

        if (!zufButton.isChecked() && !rightS.isPressed() && !leftS.isPressed()){
            rechts.setVisibility(View.INVISIBLE);
            links.setVisibility(View.INVISIBLE);
            oben.setVisibility(View.INVISIBLE);
            unten.setVisibility(View.VISIBLE);
            zufImage.setVisibility(View.INVISIBLE);

            leftS.setVisibility(View.VISIBLE);
            rightS.setVisibility(View.VISIBLE);
            noArms.setVisibility(View.VISIBLE);
            this.dieSteuerung.setzeModus(false);
            return;
        }

        //Links
        if (leftS.isPressed() && !rightS.isPressed()) {
            rechts.setVisibility(View.INVISIBLE);
            links.setVisibility(View.VISIBLE);
            oben.setVisibility(View.INVISIBLE);
            unten.setVisibility(View.INVISIBLE);

            //zufButton.setEnabled(false);
            if(perspektive.isChecked())
                this.dieSteuerung.rechterArm();
            else
                this.dieSteuerung.linkerArm();

            return;
        }

        //Rechts
        if (!leftS.isPressed() && rightS.isPressed()) {
            rechts.setVisibility(View.VISIBLE);
            links.setVisibility(View.INVISIBLE);
            oben.setVisibility(View.INVISIBLE);
            unten.setVisibility(View.INVISIBLE);

            //zufButton.setEnabled(false);
            if(perspektive.isChecked())
                this.dieSteuerung.linkerArm();
            else
                this.dieSteuerung.rechterArm();
            return;
        }

        //Beide
        if (leftS.isPressed() && rightS.isPressed()) {
            rechts.setVisibility(View.INVISIBLE);
            links.setVisibility(View.INVISIBLE);
            oben.setVisibility(View.VISIBLE);
            unten.setVisibility(View.INVISIBLE);

            //zufButton.setEnabled(false);
            this.dieSteuerung.beideArme();
            return;
        }

        //Keine
        if (!leftS.isPressed() && !rightS.isPressed()) {
            rechts.setVisibility(View.INVISIBLE);
            links.setVisibility(View.INVISIBLE);
            oben.setVisibility(View.INVISIBLE);
            unten.setVisibility(View.VISIBLE);

            //zufButton.setEnabled(true);
            this.dieSteuerung.keineArme();
            return;
        }

    }

    @Override
    public void onBackPressed() {
        DrawerLayout drawer = (DrawerLayout) findViewById(R.id.drawer_layout);
        if (drawer.isDrawerOpen(GravityCompat.START)) {
            drawer.closeDrawer(GravityCompat.START);
        } else {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        this.menü = menu;
        this.dieSteuerung.gatherData();
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //Update values to ESP
        if(id == R.id.action_settings) {
            this.dieSteuerung.updaten();
        }
        //Demo-Mode toggeln
        else if(id == R.id.toggleDemo)
            this.dieSteuerung.toggleDemoMode();

        return super.onOptionsItemSelected(item);
    }

    @SuppressWarnings("StatementWithEmptyBody")
    @Override
    public boolean onNavigationItemSelected(MenuItem item) {
        // Handle navigation view item clicks here.
        int id = item.getItemId();

        boolean noSpk = true;

         if(id == R.id.nav_red){
             this.dieSteuerung.setzeLEDColor(LEDColor.RED());
        } else if(id == R.id.nav_green){
             this.dieSteuerung.setzeLEDColor(LEDColor.GREEN());
        } else if(id == R.id.nav_blue){
             this.dieSteuerung.setzeLEDColor(LEDColor.BLUE());
        } else if(id == R.id.nav_custom){
            this.dieSteuerung.openColorPicker();
        } else if (id == R.id.nav_sparkasse) {
            noSpk = false;
            this.dieSteuerung.setzeSpkMode(true);
         }

         if(noSpk)
             this.dieSteuerung.setzeSpkMode(false);

        DrawerLayout drawer = (DrawerLayout) findViewById(R.id.drawer_layout);
        drawer.closeDrawer(GravityCompat.START);
        return true;
    }

    public void anzText(final String anz) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                anzTextView.setText("" + anz);
            }
        });
    }
}
