import paho.mqtt.client as mqtt
import json
from datetime import datetime

# MQTT Configuration for HiveMQ - Smart Selection enhanced
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/ecowatt_data"
CLIENT_ID = f"ecowatt_register_subscriber_{int(datetime.now().timestamp())}"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT broker: {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
        print("Listening for simplified register data messages... (Press Ctrl+C to exit)\n")
    else:
        print(f"Failed to connect. Return code: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        print("="*80)
        print(f"EcoWatt Register Data received at {datetime.now().strftime('%H:%M:%S')}")
        print(f"Topic: {msg.topic}")
        print("="*80)
        
        # Handle simplified register data format
        if 'register_data' in data:
            register_data = data['register_data']
            print("Solar Inverter Register Data:")
            print("-" * 40)
            
            # AC Voltage
            if 'REG_VAC1' in register_data and 'ac_voltage_readable' in register_data:
                print(f"AC Voltage: {register_data['REG_VAC1']} ({register_data['ac_voltage_readable']})")
            
            # AC Current
            if 'REG_IAC1' in register_data and 'ac_current_readable' in register_data:
                print(f"AC Current: {register_data['REG_IAC1']} ({register_data['ac_current_readable']})")
            
            # PV Current 1
            if 'REG_IPV1' in register_data and 'pv_current_1_readable' in register_data:
                print(f"PV Current 1: {register_data['REG_IPV1']} ({register_data['pv_current_1_readable']})")
            
            # AC Power
            if 'REG_PAC' in register_data and 'ac_power_readable' in register_data:
                print(f"AC Power: {register_data['REG_PAC']} ({register_data['ac_power_readable']})")
            
            # PV Current 2
            if 'REG_IPV2' in register_data and 'pv_current_2_readable' in register_data:
                print(f"PV Current 2: {register_data['REG_IPV2']} ({register_data['pv_current_2_readable']})")
            
            # Temperature
            if 'REG_TEMP' in register_data and 'temperature_readable' in register_data:
                print(f"Temperature: {register_data['REG_TEMP']} ({register_data['temperature_readable']})")
            
            print("-" * 40)
            
            # Calculate and display summary info
            ac_power = register_data.get('REG_PAC', 0)
            ac_voltage_raw = register_data.get('REG_VAC1', 0)
            ac_current_raw = register_data.get('REG_IAC1', 0)
            
            if ac_voltage_raw > 0 and ac_current_raw > 0:
                ac_voltage_volts = ac_voltage_raw / 10.0
                ac_current_amps = ac_current_raw / 100.0
                apparent_power = ac_voltage_volts * ac_current_amps
                
                if apparent_power > 0:
                    efficiency = (ac_power / apparent_power) * 100
                    print(f"Calculated Metrics:")
                    print(f"   Apparent Power: {apparent_power:.2f}W")
                    print(f"   Power Factor/Efficiency: {efficiency:.2f}%")
        
        # Fallback: Handle legacy format if still present
        elif 'smart_compression_summary' in data:
            smart_info = data['smart_compression_summary']
            print(f"Device ID: {data.get('device_id', 'unknown')}")
            print(f"Legacy Smart Compression Summary:")
            print(f"   System Type: {smart_info.get('system_type', 'unknown')}")
            print(f"   Entries Processed: {smart_info.get('entry_count', 0)}")
            print(f"   Total Power: {smart_info.get('total_power_watts', 0)}W")
            print(f"   Average Power: {smart_info.get('average_power_watts', 0)}W")
        
        else:
            # Unknown format - display raw data
            print("WARNING: Unknown data format received:")
            print(json.dumps(data, indent=2))
        
        print("="*80 + "\n")
        
    except json.JSONDecodeError:
        print(f"Received non-JSON message: {msg.payload.decode()[:200]}...")
    except Exception as e:
        print(f"Error processing message: {e}")

def on_disconnect(client, userdata, rc):
    print(f"Disconnected from MQTT broker. Return code: {rc}")

if __name__ == "__main__":
    print("Starting EcoWatt Register Data MQTT Subscriber v4.0")
    print("="*60)
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Topic: {MQTT_TOPIC}")
    print(f"Client ID: {CLIENT_ID}")
    print("Monitoring simplified register data from ESP32...")
    print("Expected Format: {register_data: {REG_VAC1, ac_voltage_readable, ...}}")
    print("Displays: AC Voltage, AC Current, PV Currents, AC Power, Temperature")
    print("="*60)
    
    client = mqtt.Client(client_id=CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nShutting down register data subscriber...")
        client.disconnect()
    except Exception as e:
        print(f"Error: {e}")
