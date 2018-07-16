package com.example.micha.roboter;

import android.os.AsyncTask;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.UnknownHostException;

/**
 * Created by Nick on 12.01.2018.
 */

public class UDPConnector {

    private int portListening,portSend;
    private String ip;
    private InetAddress IP;
    private boolean success;

    private DatagramSocket serverSocket;

    public UDPConnector(int portListening, int portSend, String ip) {
        this.portListening = portListening;
        this.portSend = portSend;
        this.ip = ip;
        refresh();
    }

    private boolean refreshSocket() {
        try {
            this.serverSocket = new DatagramSocket(getPortListening());
            this.serverSocket.setSoTimeout(100);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private boolean refreshIp() {
        try {
            this.IP = InetAddress.getByName(this.ip);
            return true;
        } catch (UnknownHostException e) {
            return false;
        }
    }

    public boolean sendData(final String data)  {

        new Thread(new Runnable() {
            @Override
            public void run() {
                byte[] sendData = data.trim().getBytes();
                DatagramPacket datagramPacket = new DatagramPacket(sendData, sendData.length, IP, getPortSend());
                try {
                    serverSocket.send(datagramPacket);
                    success = true;
                } catch (IOException e) {
                    success = false;
                }
            }
        }).start();
        return success;
    }

    public void refresh(){
        refreshIp();
        refreshSocket();
        //this.toaster.makeToast("IP: " + getIp() + " Port: " + getPortListening());
    }

    public String receiveData(int packetSize) {
        byte[] receiveData = new byte[packetSize];
        DatagramPacket receivePacket = new DatagramPacket(receiveData, receiveData.length);
        try {
            serverSocket.receive(receivePacket);
            return new String(receiveData).trim();
        } catch (IOException e) {
            //e.printStackTrace();
            return "null";
        }
    }

    public void setPortListening(int portListening) {
        this.portListening = portListening;
        refreshSocket();
    }

    public void setPortSend(int portSend) {
        this.portSend = portSend;
    }

    public int getPortListening() {
        return portListening;
    }

    public int getPortSend() {
        return portSend;
    }

    public String getIp() {
        return ip;
    }

    public void setIp(String ip) {
        this.ip = ip;
        refreshIp();
    }
}
