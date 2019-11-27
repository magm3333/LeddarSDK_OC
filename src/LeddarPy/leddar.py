#import sys
#lib_path='/home/mariano/.node-red/LeddarPy/dist/leddar-1.0-py2.7-linux-x86_64.egg'
#sys.path.append(lib_path)
import leddar
import time

leddar.enable_debug_trace(False)

#Callback functions for the data thread
def echoes_callback(echoes):
    data = echoes["data"]
    #To avoid display of too much lines
    increment = 1
    if len(data) > 100 :
        increment = 100

    json="["
    for i in range(0, len(data), increment):
        if json!="[" :
            json+=", "
        json+="{\"channel\":"+str(data[i]["indices"]) + ", \"dist\":" + str(data[i]["distances"]) + ", \"ampl\":" + str(data[i]["amplitudes"]) + ", \"flags\":" + str(data[i]["flags"]) +"}"
    if json.startswith("[{"):
        print(json+"]")
    time.sleep(1);


#Create device
dev = leddar.Device()

#Connect to the device
sensorlist = leddar.get_devices("Usb")
dev.connect(sensorlist[0]['name'], leddar.device_types["Usb"])

#Set callback method
dev.set_callback_echo(echoes_callback)
dev.set_data_mask(leddar.data_masks["DM_STATES"] | leddar.data_masks["DM_ECHOES"])

#Optionnal : set the delay between two request to the sensor
dev.set_data_thread_delay(2000)

dev.start_data_thread()

#Tiempo de funcionamiento 1 anio
time.sleep( 60*60*24*365 )

dev.stop_data_thread()

time.sleep( 1 )
dev.disconnect()
del dev
