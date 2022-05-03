#include "esphome.h"
#include "mbedtls/gcm.h"

static const char* ESPDM_VERSION = "0.9.0";
static const char* TAG = "espdm";

namespace esphome
{
    namespace espdm
    {
        class DlmsMeter : public Component, public uart::UARTDevice
        {
            public:
                DlmsMeter(uart::UARTComponent *parent);

                void setup() override;
                void loop() override;

                void set_voltage_sensors(sensor::Sensor *voltage_l1, sensor::Sensor *voltage_l2, sensor::Sensor *voltage_l3);
                void set_current_sensors(sensor::Sensor *current_l1, sensor::Sensor *current_l2, sensor::Sensor *current_l3);

                void set_active_power_sensors(sensor::Sensor *active_power_plus, sensor::Sensor *active_power_minus, sensor::Sensor *power_factor);
                void set_active_energy_sensors(sensor::Sensor *active_energy_plus, sensor::Sensor *active_energy_minus);
                void set_reactive_energy_sensors(sensor::Sensor *reactive_energy_plus, sensor::Sensor *reactive_energy_minus);
                void set_timestamp_sensor(text_sensor::TextSensor *timestamp);
                void set_meternumber_sensor(text_sensor::TextSensor *meternumber);

                void enable_mqtt(mqtt::MQTTClientComponent *mqtt_client, const char *topic);

                void set_key(byte key[], size_t keyLength);

            private:
                int receiveBufferIndex = 0; // Current position of the receive buffer
                const static int receiveBufferSize = 1024; // Size of the receive buffer
                byte receiveBuffer[receiveBufferSize]; // Stores the packet currently being received
                unsigned long lastRead = 0; // Timestamp when data was last read
                int readTimeout = 1000; // Time to wait after last byte before considering data complete

                byte key[16]; // Stores the decryption key
                size_t keyLength; // Stores the decryption key length (usually 16 bytes)

                const char *topic; // Stores the MQTT topic

                mbedtls_gcm_context aes; // AES context used for decryption

                sensor::Sensor *voltage_l1 = NULL; // Voltage L1
                sensor::Sensor *voltage_l2 = NULL; // Voltage L2
                sensor::Sensor *voltage_l3 = NULL; // Voltage L3

                sensor::Sensor *current_l1 = NULL; // Current L1
                sensor::Sensor *current_l2 = NULL; // Current L2
                sensor::Sensor *current_l3 = NULL; // Current L3

                sensor::Sensor *active_power_plus = NULL; // Active power taken from grid
                sensor::Sensor *active_power_minus = NULL; // Active power put into grid
                sensor::Sensor *power_factor = NULL; // Power Factor

                sensor::Sensor *active_energy_plus = NULL; // Active energy taken from grid
                sensor::Sensor *active_energy_minus = NULL; // Active energy put into grid

                sensor::Sensor *reactive_energy_plus = NULL; // Reactive energy taken from grid
                sensor::Sensor *reactive_energy_minus = NULL; // Reactive energy put into grid

                text_sensor::TextSensor *timestamp = NULL; // Text sensor for the timestamp value

                text_sensor::TextSensor *meternumber = NULL; // Text sensor for the meterNumber value

                mqtt::MQTTClientComponent *mqtt_client = NULL;

                uint16_t swap_uint16(uint16_t val);
                uint32_t swap_uint32(uint32_t val);
                void log_packet(byte array[], size_t length);
                void abort();
        };
    }
}
