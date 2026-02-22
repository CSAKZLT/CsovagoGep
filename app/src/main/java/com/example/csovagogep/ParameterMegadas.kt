package com.example.csovagogep

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.Switch
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.IOException
import java.util.UUID

class ParameterMegadas : AppCompatActivity() {


    private var devAddress: String? = null
    companion object { private val myUUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB") }
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private var btSocket: BluetoothSocket? = null
    private var isBtConnected = false


    private lateinit var jogX: EditText
    private lateinit var jogFi: EditText
    private lateinit var jogStart: Button
    private lateinit var funkcioSwitch: Switch
    private lateinit var d: EditText
    private lateinit var D: EditText
    private lateinit var alfa: EditText
    private lateinit var vagasStart: Button


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_parameter_megadas)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        jogX = findViewById(R.id.edtexJogX)
        jogFi = findViewById(R.id.edtexJogFi)
        jogStart = findViewById(R.id.btnJogStart)
        funkcioSwitch = findViewById(R.id.swVagasTipus)
        d = findViewById(R.id.edtexKisd)
        D = findViewById(R.id.edtexNagyD)
        alfa = findViewById(R.id.edtexBezartSzog)
        vagasStart = findViewById(R.id.btnVagasStart)

        var message = ""

        jogStart.setOnClickListener {
            val x = jogX.text.toString()
            val fi = jogFi.text.toString()
            if(x.isNotEmpty() && fi.isNotEmpty()){
                message = "$x $fi"
                sendData(message)
            }else if(x.isNotEmpty()){
                message = "$x 0"
                sendData(message)
            }else if(fi.isNotEmpty()){
                message = "0 $fi"
                sendData(message)
            }
            else{
                Toast.makeText(this, "Adj meg elmozdulás értéket!", Toast.LENGTH_SHORT).show()
            }
        }

        funkcioSwitch.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) {
                funkcioSwitch.text = "Homlok megmunkálás"
                vagasStart.setOnClickListener {
                    val kisd = d.text.toString()
                    val nagyD = D.text.toString()
                    val alfaszog = alfa.text.toString()
                    if(kisd.isNotEmpty() && nagyD.isNotEmpty() && alfaszog.isNotEmpty()){
                        message = "H $kisd $nagyD $alfaszog"
                        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
                        sendData(message)
                    }else{
                        Toast.makeText(this, "Adj meg értéket a vágáshoz!", Toast.LENGTH_SHORT).show()
                    }
                }
            } else {
                funkcioSwitch.text = "Palást megmunkálás"
                vagasStart.setOnClickListener {
                    val kisd = d.text.toString()
                    val nagyD = D.text.toString()
                    val alfaszog = alfa.text.toString()
                    if(kisd.isNotEmpty() && nagyD.isNotEmpty() && alfaszog.isNotEmpty()){
                        message = "P $kisd $nagyD $alfaszog"
                        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
                        sendData(message)
                    }else{
                        Toast.makeText(this, "Adj meg értéket a vágáshoz!", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }

        devAddress = intent.getStringExtra("MAC_ADDRESS")
        Toast.makeText(this, devAddress, Toast.LENGTH_SHORT).show()

        val bluetoothManager = getSystemService(BluetoothManager::class.java)
        bluetoothAdapter = bluetoothManager.adapter



    }


    private fun sendData(message: String){
        val address = devAddress ?: return
        CoroutineScope(Dispatchers.IO).launch {
            if (ActivityCompat.checkSelfPermission( this@ParameterMegadas,
                    Manifest.permission.BLUETOOTH_CONNECT ) != PackageManager.PERMISSION_GRANTED )
                return@launch

            try {
                if (bluetoothAdapter.isDiscovering){
                    bluetoothAdapter.cancelDiscovery()
                }

                if (btSocket == null || !isBtConnected) {
                    val device = bluetoothAdapter.getRemoteDevice(address)
                    btSocket = device.createInsecureRfcommSocketToServiceRecord(myUUID)
                    btSocket!!.connect()
                    isBtConnected = true
                }

                btSocket!!.outputStream.write(message.toByteArray())
                withContext(Dispatchers.Main) {
                    Toast.makeText( this@ParameterMegadas, "Üzenet elküldve: $message", Toast.LENGTH_LONG ).show()
                }
            }catch (e: IOException){
                withContext(Dispatchers.Main) {
                    Toast.makeText( this@ParameterMegadas, "Hiba történt a küldés során", Toast.LENGTH_LONG ).show()
                }
            }
        }
    }

}

