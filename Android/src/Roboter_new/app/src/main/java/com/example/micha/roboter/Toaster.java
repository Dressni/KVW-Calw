package com.example.micha.roboter;

import android.widget.Toast;

/**
 * Created by Nick on 15.01.2018.
 */

public class Toaster {

    private static MainActivity mainActivity;

    public Toaster(MainActivity mainActivity) {
        this.mainActivity = mainActivity;
    }

    public static void makeToast(String msg){
        int duration = Toast.LENGTH_SHORT;
        Toast toast = Toast.makeText(mainActivity.getApplicationContext(), msg, duration);
        toast.show();
    }
}
