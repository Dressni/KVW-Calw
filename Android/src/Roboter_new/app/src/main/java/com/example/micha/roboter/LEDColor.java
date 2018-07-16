package com.example.micha.roboter;

import android.graphics.Color;

/**
 * Created by Nick on 26.06.2018.
 */

public class LEDColor {

    private int r,g,b;


    public LEDColor(int color){
        this.r = (color >> 16) & 0xff;
        this.g = (color >>  8) & 0xff;
        this.b = (color      ) & 0xff;
    }

    public LEDColor(int r, int g, int b){
        this.r = r;
        this.g = g;
        this.b = b;
    }
    public LEDColor(int colors[]){
        this.r = colors[0];
        this.g = colors[1];
        this.b = colors[2];
    }

    public String getCMDColor(){
        return "SETLEDCOLOR=" + r + ";" + g + ";" + b;
    }

    public static LEDColor RED(){
        return new LEDColor(255, 0, 0);
    }

    public static LEDColor GREEN(){
        return new LEDColor(0 , 255, 0);
    }

    public static LEDColor BLUE(){
        return new LEDColor(0, 0, 255);
    }

    public int getColor() {
        return  (r & 0xff) << 16 | (g & 0xff) << 8 | (b & 0xff);
    }
    public int getColorPrefix() {
        return  0xFF000000 + getColor();
    }

    @Override
    public String toString() {
        return "[" + r +"," + g + "," + b + "], #" + Integer.toHexString(getColorPrefix());
    }
}
