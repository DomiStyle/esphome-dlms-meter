#include "espdm.h"
#include "espdm_dlms.h"
#include "espdm_obis.h"

namespace esphome
{
    namespace espdm
    {
        DlmsMeter::DlmsMeter(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

        void DlmsMeter::setup()
        {
            ESP_LOGI(TAG, "DLMS smart meter component v%s started", ESPDM_VERSION);
        }

        void DlmsMeter::loop()
        {
            unsigned long currentTime = millis();

            while(available()) // Read while data is available
            {
                if(receiveBufferIndex >= receiveBufferSize)
                {
                    ESP_LOGE(TAG, "Buffer overflow");
                    receiveBufferIndex = 0;
                }

                receiveBuffer[receiveBufferIndex] = read();
                receiveBufferIndex++;

                lastRead = currentTime;
            }

            if(receiveBufferIndex > 0 && currentTime - lastRead > readTimeout)
            {
                if(receiveBufferIndex < 256)
                {
                    ESP_LOGE(TAG, "Received packet with invalid size");
                    return abort();
                }

                ESP_LOGD(TAG, "Handling packet");
                log_packet(receiveBuffer, receiveBufferIndex);

                // Decrypting

                uint16_t payloadLength;
                memcpy(&payloadLength, &receiveBuffer[20], 2); // Copy payload length
                payloadLength = swap_uint16(payloadLength) - 5;

                if(receiveBufferIndex <= payloadLength)
                {
                    ESP_LOGE(TAG, "Payload length is too big for received data");
                    return abort();
                }

        /*
                uint16_t payloadLengthPacket1;
                memcpy(&payloadLengthPacket1, &receiveBuffer[9], 2); // Copy payload length of first telegram

                payloadLengthPacket1 = swap_uint16(payloadLengthPacket1);

                if(payloadLengthPacket1 >= payloadLength)
                {
                    ESP_LOGE(TAG, "Payload length 1 is too big");
                    return abort();
                }
        */
                uint16_t payloadLength1 = 227; // TODO: Read payload length 1 from data

                uint16_t payloadLength2 = payloadLength - payloadLength1;

                if(payloadLength2 >= receiveBufferIndex - DLMS_HEADER2_OFFSET - DLMS_HEADER2_LENGTH)
                {
                    ESP_LOGE(TAG, "Payload length 2 is too big");
                    return abort();
                }

                byte iv[12]; // Reserve space for the IV, always 12 bytes

                memcpy(&iv[0], &receiveBuffer[DLMS_SYST_OFFSET], DLMS_SYST_LENGTH); // Copy system title to IV
                memcpy(&iv[8], &receiveBuffer[DLMS_IC_OFFSET], DLMS_IC_LENGTH); // Copy invocation counter to IV

                byte ciphertext[payloadLength];
                memcpy(&ciphertext[0], &receiveBuffer[DLMS_HEADER1_OFFSET + DLMS_HEADER1_LENGTH], payloadLength1);
                memcpy(&ciphertext[payloadLength1], &receiveBuffer[DLMS_HEADER2_OFFSET + DLMS_HEADER2_LENGTH], payloadLength2);

                byte plaintext[payloadLength];

                mbedtls_gcm_init(&aes);
                mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES , key, keyLength * 8);

                mbedtls_gcm_auth_decrypt(&aes, payloadLength, iv, sizeof(iv), NULL, 0, NULL, 0, ciphertext, plaintext);

                mbedtls_gcm_free(&aes);

                if(plaintext[0] != 0x0F || plaintext[5] != 0x0C)
                {
                    ESP_LOGE(TAG, "Packet was decrypted but data is invalid");
                    return abort();
                }

                // Decoding

                int currentPosition = DECODER_START_OFFSET;

                do
                {
                    if(plaintext[currentPosition + OBIS_TYPE_OFFSET] != DataType::OctetString)
                    {
                        ESP_LOGE(TAG, "Unsupported OBIS header type");
                        return abort();
                    }

                    byte obisCodeLength = plaintext[currentPosition + OBIS_LENGTH_OFFSET];

                    if(obisCodeLength != 0x06)
                    {
                        ESP_LOGE(TAG, "Unsupported OBIS header length");
                        return abort();
                    }

                    byte obisCode[obisCodeLength];
                    memcpy(&obisCode[0], &plaintext[currentPosition + OBIS_CODE_OFFSET], obisCodeLength); // Copy OBIS code to array

                    currentPosition += obisCodeLength + 2; // Advance past code, position and type

                    byte dataType = plaintext[currentPosition];
                    currentPosition++; // Advance past data type

                    byte dataLength = 0x00;

                    CodeType codeType = CodeType::Unknown;

                    if(obisCode[OBIS_A] == Medium::Electricity)
                    {
                        // Compare C and D against code
                        if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L1, 2) == 0)
                        {
                            codeType = CodeType::VoltageL1;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L2, 2) == 0)
                        {
                            codeType = CodeType::VoltageL2;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_VOLTAGE_L3, 2) == 0)
                        {
                            codeType = CodeType::VoltageL3;
                        }

                        else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L1, 2) == 0)
                        {
                            codeType = CodeType::CurrentL1;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L2, 2) == 0)
                        {
                            codeType = CodeType::CurrentL2;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_CURRENT_L3, 2) == 0)
                        {
                            codeType = CodeType::CurrentL3;
                        }

                        else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_POWER_PLUS, 2) == 0)
                        {
                            codeType = CodeType::ActivePowerPlus;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_POWER_MINUS, 2) == 0)
                        {
                            codeType = CodeType::ActivePowerMinus;
                        }

                        else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_ENERGY_PLUS, 2) == 0)
                        {
                            codeType = CodeType::ActiveEnergyPlus;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_ACTIVE_ENERGY_MINUS, 2) == 0)
                        {
                            codeType = CodeType::ActiveEnergyMinus;
                        }

                        else if(memcmp(&obisCode[OBIS_C], ESPDM_REACTIVE_ENERGY_PLUS, 2) == 0)
                        {
                            codeType = CodeType::ReactiveEnergyPlus;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_REACTIVE_ENERGY_MINUS, 2) == 0)
                        {
                            codeType = CodeType::ReactiveEnergyMinus;
                        }
                        else
                        {
                            ESP_LOGW(TAG, "Unsupported OBIS code");
                        }
                    }
                    else if(obisCode[OBIS_A] == Medium::Abstract)
                    {
                        if(memcmp(&obisCode[OBIS_C], ESPDM_TIMESTAMP, 2) == 0)
                        {
                            codeType = CodeType::Timestamp;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_SERIAL_NUMBER, 2) == 0)
                        {
                            codeType = CodeType::SerialNumber;
                        }
                        else if(memcmp(&obisCode[OBIS_C], ESPDM_DEVICE_NAME, 2) == 0)
                        {
                            codeType = CodeType::DeviceName;
                        }
                        else
                        {
                            ESP_LOGW(TAG, "Unsupported OBIS code");
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Unsupported OBIS medium");
                        return abort();
                    }

                    uint8_t uint8Value;
                    uint16_t uint16Value;
                    uint32_t uint32Value;
                    float floatValue;


                    switch(dataType)
                    {
                        case DataType::DoubleLongUnsigned:
                            dataLength = 4;

                            memcpy(&uint32Value, &plaintext[currentPosition], 4); // Copy bytes to integer
                            uint32Value = swap_uint32(uint32Value); // Swap bytes

                            floatValue = uint32Value; // Ignore decimal digits for now

                            if(codeType == CodeType::ActivePowerPlus && this->active_power_plus != NULL && this->active_power_plus->state != floatValue)
                                this->active_power_plus->publish_state(floatValue);
                            else if(codeType == CodeType::ActivePowerMinus && this->active_power_minus != NULL && this->active_power_minus->state != floatValue)
                                this->active_power_minus->publish_state(floatValue);

                            else if(codeType == CodeType::ActiveEnergyPlus && this->active_energy_plus != NULL && this->active_energy_plus->state != floatValue)
                                this->active_energy_plus->publish_state(floatValue);
                            else if(codeType == CodeType::ActiveEnergyMinus && this->active_energy_minus != NULL && this->active_energy_minus->state != floatValue)
                                this->active_energy_minus->publish_state(floatValue);

                            else if(codeType == CodeType::ReactiveEnergyPlus && this->reactive_energy_plus != NULL && this->reactive_energy_plus->state != floatValue)
                                this->reactive_energy_plus->publish_state(floatValue);
                            else if(codeType == CodeType::ReactiveEnergyMinus && this->reactive_energy_minus != NULL && this->reactive_energy_minus->state != floatValue)
                                this->reactive_energy_minus->publish_state(floatValue);

                        break;
                        case DataType::LongUnsigned:
                            dataLength = 2;

                            memcpy(&uint16Value, &plaintext[currentPosition], 2); // Copy bytes to integer
                            uint16Value = swap_uint16(uint16Value); // Swap bytes

                            if(plaintext[currentPosition + 5] == Accuracy::SingleDigit)
                                floatValue = uint16Value / 10.0; // Divide by 10 to get decimal places
                            else if(plaintext[currentPosition + 5] == Accuracy::DoubleDigit)
                                floatValue = uint16Value / 100.0; // Divide by 100 to get decimal places
                            else
                                floatValue = uint16Value; // No decimal places

                            if(codeType == CodeType::VoltageL1 && this->voltage_l1 != NULL && this->voltage_l1->state != floatValue)
                                this->voltage_l1->publish_state(floatValue);
                            else if(codeType == CodeType::VoltageL2 && this->voltage_l2 != NULL && this->voltage_l2->state != floatValue)
                                this->voltage_l2->publish_state(floatValue);
                            else if(codeType == CodeType::VoltageL3 && this->voltage_l3 != NULL && this->voltage_l3->state != floatValue)
                                this->voltage_l3->publish_state(floatValue);

                            else if(codeType == CodeType::CurrentL1 && this->current_l1 != NULL && this->current_l1->state != floatValue)
                                this->current_l1->publish_state(floatValue);
                            else if(codeType == CodeType::CurrentL2 && this->current_l2 != NULL && this->current_l2->state != floatValue)
                                this->current_l2->publish_state(floatValue);
                            else if(codeType == CodeType::CurrentL3 && this->current_l3 != NULL && this->current_l3->state != floatValue)
                                this->current_l3->publish_state(floatValue);

                        break;
                        case DataType::OctetString:
                            dataLength = plaintext[currentPosition];
                            currentPosition++; // Advance past string length

                            if(codeType == CodeType::Timestamp) // Handle timestamp generation
                            {
                                char timestamp[21]; // 0000-00-00T00:00:00Z

                                uint16_t year;
                                uint8_t month;
                                uint8_t day;

                                uint8_t hour;
                                uint8_t minute;
                                uint8_t second;

                                memcpy(&uint16Value, &plaintext[currentPosition], 2);
                                year = swap_uint16(uint16Value);

                                memcpy(&month, &plaintext[currentPosition + 2], 1);
                                memcpy(&day, &plaintext[currentPosition + 3], 1);

                                memcpy(&hour, &plaintext[currentPosition + 5], 1);
                                memcpy(&minute, &plaintext[currentPosition + 6], 1);
                                memcpy(&second, &plaintext[currentPosition + 7], 1);

                                sprintf(timestamp, "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute, second);

                                this->timestamp->publish_state(timestamp);
                            }

                        break;
                        default:
                            ESP_LOGE(TAG, "Unsupported OBIS data type");
                            return abort();
                        break;
                    }

                    currentPosition += dataLength; // Skip data length

                    currentPosition += 2; // Skip break after data

                    if(plaintext[currentPosition] == 0x0F) // There is still additional data for this type, skip it
                        currentPosition += 6; // Skip additional data and additional break; this will jump out of bounds on last frame
                }
                while (currentPosition <= payloadLength); // Loop until arrived at end

                receiveBufferIndex = 0;

                ESP_LOGI(TAG, "Received valid data");

                if(this->mqtt_client != NULL)
                {
                    this->mqtt_client->publish_json(topic, [=](JsonObject &root)
                    {
                        if(this->voltage_l1 != NULL)
                        {
                            root["voltage_l1"] = this->voltage_l1->state;
                            root["voltage_l2"] = this->voltage_l2->state;
                            root["voltage_l3"] = this->voltage_l3->state;
                        }

                        if(this->current_l1 != NULL)
                        {
                            root["current_l1"] = this->current_l1->state;
                            root["current_l2"] = this->current_l2->state;
                            root["current_l3"] = this->current_l3->state;
                        }

                        if(this->active_power_plus != NULL)
                        {
                            root["active_power_plus"] = this->active_power_plus->state;
                            root["active_power_minus"] = this->active_power_minus->state;
                        }

                        if(this->active_energy_plus != NULL)
                        {
                            root["active_energy_plus"] = this->active_energy_plus->state;
                            root["active_energy_minus"] = this->active_energy_minus->state;
                        }

                        if(this->reactive_energy_plus != NULL)
                        {
                            root["reactive_energy_plus"] = this->reactive_energy_plus->state;
                            root["reactive_energy_minus"] = this->reactive_energy_minus->state;
                        }

                        if(this->timestamp != NULL)
                        {
                            root["timestamp"] = this->timestamp->state;
                        }
                    });
                }
            }
        }

        void DlmsMeter::abort()
        {
            receiveBufferIndex = 0;
        }

        uint16_t DlmsMeter::swap_uint16(uint16_t val)
        {
            return (val << 8) | (val >> 8);
        }

        uint32_t DlmsMeter::swap_uint32(uint32_t val)
        {
            val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
            return (val << 16) | (val >> 16);
        }

        void DlmsMeter::set_key(byte key[], size_t keyLength)
        {
            memcpy(&this->key[0], &key[0], keyLength);
            this->keyLength = keyLength;
        }

        void DlmsMeter::set_voltage_sensors(sensor::Sensor *voltage_l1, sensor::Sensor *voltage_l2, sensor::Sensor *voltage_l3)
        {
            this->voltage_l1 = voltage_l1;
            this->voltage_l2 = voltage_l2;
            this->voltage_l3 = voltage_l3;
        }
        void DlmsMeter::set_current_sensors(sensor::Sensor *current_l1, sensor::Sensor *current_l2, sensor::Sensor *current_l3)
        {
            this->current_l1 = current_l1;
            this->current_l2 = current_l2;
            this->current_l3 = current_l3;
        }

        void DlmsMeter::set_active_power_sensors(sensor::Sensor *active_power_plus, sensor::Sensor *active_power_minus)
        {
            this->active_power_plus = active_power_plus;
            this->active_power_minus = active_power_minus;
        }

        void DlmsMeter::set_active_energy_sensors(sensor::Sensor *active_energy_plus, sensor::Sensor *active_energy_minus)
        {
            this->active_energy_plus = active_energy_plus;
            this->active_energy_minus = active_energy_minus;
        }

        void DlmsMeter::set_reactive_energy_sensors(sensor::Sensor *reactive_energy_plus, sensor::Sensor *reactive_energy_minus)
        {
            this->reactive_energy_plus = reactive_energy_plus;
            this->reactive_energy_minus = reactive_energy_minus;
        }

        void DlmsMeter::set_timestamp_sensor(text_sensor::TextSensor *timestamp)
        {
            this->timestamp = timestamp;
        }

        void DlmsMeter::enable_mqtt(mqtt::MQTTClientComponent *mqtt_client, const char *topic)
        {
            this->mqtt_client = mqtt_client;
            this->topic = topic;
        }

        void DlmsMeter::log_packet(byte array[], size_t length)
        {
            char buffer[(length*3)];

            for (unsigned int i = 0; i < length; i++)
            {
                byte nib1 = (array[i] >> 4) & 0x0F;
                byte nib2 = (array[i] >> 0) & 0x0F;
                buffer[i*3] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
                buffer[i*3+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
                buffer[i*3+2] = ' ';
            }

            buffer[(length*3)-1] = '\0';

            ESP_LOGV(TAG, buffer);
        }
    }
}
