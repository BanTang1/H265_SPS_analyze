package com.zhanghao.h265_sps_analyze;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import com.zhanghao.h265_sps_analyze.databinding.ActivityMainBinding;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("h265_sps_analyze");
    }

    private ActivityMainBinding binding;

    // H265 数据
    private byte[] dataByteArray;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        readDataToByte();
        analyzeSps(dataByteArray);
    }

    /**
     * 读取数据到byte数组中;
     * 未进行分批处理，因此文件不能太大
     */
    private void readDataToByte() {
        InputStream dataInputStream = getResources().openRawResource(R.raw.output);
        try {
            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
            byte[] buffer = new byte[1024];
            int bytesRead;
            while ((bytesRead = dataInputStream.read(buffer)) != -1) {
                byteArrayOutputStream.write(buffer, 0, bytesRead);
            }
            dataInputStream.close();

            dataByteArray = byteArrayOutputStream.toByteArray();
            byteArrayOutputStream.close();

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private native void analyzeSps(byte[] dataByteArray);
}