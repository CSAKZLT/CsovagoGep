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


    private lateinit var jogD: EditText
    private lateinit var jogX: EditText
    private lateinit var jogFi: EditText
    private lateinit var jogAdat: Button
    private lateinit var startX: Button
    private lateinit var startA: Button


    private lateinit var funkcioSwitch: Switch
    private lateinit var d: EditText
    private lateinit var D: EditText
    private lateinit var alfa: EditText
    private lateinit var sebesseg: EditText
    private lateinit var vagasAdat: Button
    private lateinit var palastMegm: Button
    private lateinit var homlokMegm: Button





    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_parameter_megadas)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        jogD = findViewById(R.id.edtexJogD)
        jogX = findViewById(R.id.edtexJogX)
        jogFi = findViewById(R.id.edtexJogFi)
        jogAdat = findViewById(R.id.btnJogAdatKüld)
        startX = findViewById(R.id.btnXpoz)
        startA =findViewById(R.id.btnApoz)
        d = findViewById(R.id.edtexKisd)
        D = findViewById(R.id.edtexNagyD)
        alfa = findViewById(R.id.edtexBezartSzog)
        sebesseg = findViewById(R.id.edtexVagsSeb)
        vagasAdat = findViewById(R.id.btnVagasAdat)
        palastMegm = findViewById(R.id.btnPalástV)
        homlokMegm = findViewById(R.id.btnHomlokV)

        var message = ""

        jogAdat.setOnClickListener {
            val d =jogD.text.toString()
            val x = jogX.text.toString()
            val fi = jogFi.text.toString()
            if(d.isNotEmpty() && x.isNotEmpty() && fi.isNotEmpty()){
                message = "J $d $x $fi\n"
                sendData(message)
            }else if(d.isNotEmpty() && x.isNotEmpty()){
                message = "J $d $x 0\n"
                sendData(message)
            }else if(d.isNotEmpty() && fi.isNotEmpty()){
                message = "J $d 0 $fi\n"
                sendData(message)
            }
            else{
                Toast.makeText(this, "Adj meg elmozdulás értéket!", Toast.LENGTH_SHORT).show()
            }
        }

        startX.setOnClickListener {
            message = "X"
            sendData(message)
        }

        startA.setOnClickListener {
            message = "A"
            sendData(message)
        }


        vagasAdat.setOnClickListener {
            val nagyD = D.text.toString()
            val kisd = d.text.toString()
            val alfaszog = alfa.text.toString()
            val v = sebesseg.text.toString()
            if(v.isNotEmpty() && kisd.isNotEmpty() && nagyD.isNotEmpty() && alfaszog.isNotEmpty()){
                message = "D $nagyD $kisd $alfaszog $v 0\n"
                Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
                sendData(message)
            }else{
                Toast.makeText(this, "Adj meg értéket a vágáshoz!", Toast.LENGTH_SHORT).show()
            }
        }

        palastMegm.setOnClickListener {
            message = "P"
            sendData(message)
        }

        homlokMegm.setOnClickListener {
            message = "H"
            sendData(message)
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

