#include "espdm.h"
#include "espdm_mbus.h"
#include "espdm_dlms.h"
#include "espdm_obis.h"
#if defined(ESP8266)
#include <bearssl/bearssl.h>
#endif

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
                uint8_t c;
                this->read_byte(&c);
                this->receiveBuffer.push_back(c);

                this->lastRead = currentTime;
                //fix for ESPHOME 2022.12 -> added 10ms delay
                delay(10);
            }

            if(!this->receiveBuffer.empty() && currentTime - this->lastRead > this->readTimeout)
            {
                log_packet(this->receiveBuffer);

                // Verify and parse M-Bus frames

                ESP_LOGV(TAG, "Parsing M-Bus frames");

                uint16_t frameOffset = 0; // Offset is used if the M-Bus message is split into multiple frames
                std::vector<uint8_t> mbusPayload; // Contains the data of the payload

                while(true)
                {
                    ESP_LOGV(TAG, "MBUS: Parsing frame");

                    // Check start bytes
                    if(this->receiveBuffer[frameOffset + MBUS_START1_OFFSET] != 0x68 || this->receiveBuffer[frameOffset + MBUS_START2_OFFSET] != 0x68)
                    {
                        ESP_LOGE(TAG, "MBUS: Start bytes do not match");
                        return abort();
                    }

                    // Both length bytes must be identical
                    if(this->receiveBuffer[frameOffset + MBUS_LENGTH1_OFFSET] != this->receiveBuffer[frameOffset + MBUS_LENGTH2_OFFSET])
                    {
                        ESP_LOGE(TAG, "MBUS: Length bytes do not match");
                        return abort();
                    }

                    uint8_t frameLength = this->receiveBuffer[frameOffset + MBUS_LENGTH1_OFFSET]; // Get length of this frame

                    // Check if received data is enough for the given frame length
                    if(this->receiveBuffer.size() - frameOffset < frameLength + 3)
                    {
                        ESP_LOGE(TAG, "MBUS: Frame too big for received data");
                        return abort();
                    }

                    if(this->receiveBuffer[frameOffset + frameLength + MBUS_HEADER_INTRO_LENGTH + MBUS_FOOTER_LENGTH - 1] != 0x16)
                    {
                        ESP_LOGE(TAG, "MBUS: Invalid stop byte");
                        return abort();
                    }

                    mbusPayload.insert(mbusPayload.end(), &this->receiveBuffer[frameOffset + MBUS_FULL_HEADER_LENGTH], &this->receiveBuffer[frameOffset + MBUS_HEADER_INTRO_LENGTH + frameLength]);

                    frameOffset += MBUS_HEADER_INTRO_LENGTH + frameLength + MBUS_FOOTER_LENGTH;

                    if(frameOffset >= this->receiveBuffer.size()) // No more data to read, exit loop
                    {
                        break;
                    }
                }

                // Verify and parse DLMS header

                ESP_LOGV(TAG, "Parsing DLMS header");

                if(mbusPayload.size() < 20) // If the payload is too short we need to abort
                {
                    ESP_LOGE(TAG, "DLMS: Payload too short");
                    return abort();
                }

                if(mbusPayload[DLMS_CIPHER_OFFSET] != 0xDB) // Only general-glo-ciphering is supported (0xDB)
                {
                    ESP_LOGE(TAG, "DLMS: Unsupported cipher");
                    return abort();
                }

                uint8_t systitleLength = mbusPayload[DLMS_SYST_OFFSET];

                if(systitleLength != 0x08) // Only system titles with length of 8 are supported
                {
                    ESP_LOGE(TAG, "DLMS: Unsupported system title length");
                    return abort();
                }

                uint16_t messageLength = mbusPayload[DLMS_LENGTH_OFFSET];
                int headerOffset = 0;

                if(messageLength == 0x82)
                {
                    ESP_LOGV(TAG, "DLMS: Message length > 127");

                    memcpy(&messageLength, &mbusPayload[DLMS_LENGTH_OFFSET + 1], 2);
                    messageLength = swap_uint16(messageLength);

                    headerOffset = DLMS_HEADER_EXT_OFFSET; // Header is now 2 bytes longer due to length > 127
                }
                else
                {
                    ESP_LOGV(TAG, "DLMS: Message length <= 127");
                }

                messageLength -= DLMS_LENGTH_CORRECTION; // Correct message length due to part of header being included in length

                if(mbusPayload.size() - DLMS_HEADER_LENGTH - headerOffset != messageLength)
                {
                    ESP_LOGE(TAG, "DLMS: Message has invalid length");
                    return abort();
                }

                if(mbusPayload[headerOffset + DLMS_SECBYTE_OFFSET] != 0x21) // Only certain security suite is supported (0x21)
                {
                    ESP_LOGE(TAG, "DLMS: Unsupported security control byte");
                    return abort();
                }

                // Decryption

                ESP_LOGV(TAG, "Decrypting payload");

                uint8_t iv[12]; // Reserve space for the IV, always 12 bytes
                // Copy system title to IV (System title is before length; no header offset needed!)
                // Add 1 to the offset in order to skip the system title length byte
                memcpy(&iv[0], &mbusPayload[DLMS_SYST_OFFSET + 1], systitleLength);
                memcpy(&iv[8], &mbusPayload[headerOffset + DLMS_FRAMECOUNTER_OFFSET], DLMS_FRAMECOUNTER_LENGTH); // Copy frame counter to IV

                uint8_t plaintext[messageLength];

#if defined(ESP8266)
                memcpy(plaintext, &mbusPayload[headerOffset + DLMS_PAYLOAD_OFFSET], messageLength);
                br_gcm_context gcmCtx;
                br_aes_ct_ctr_keys bc;
                br_aes_ct_ctr_init(&bc, this->key, this->keyLength);
                br_gcm_init(&gcmCtx, &bc.vtable, br_ghash_ctmul32);
                br_gcm_reset(&gcmCtx, iv, sizeof(iv));
                br_gcm_flip(&gcmCtx);
                br_gcm_run(&gcmCtx, 0, plaintext, messageLength);
#elif defined(ESP32)
                mbedtls_gcm_init(&this->aes);
                mbedtls_gcm_setkey(&this->aes, MBEDTLS_CIPHER_ID_AES, this->key, this->keyLength * 8);

                mbedtls_gcm_auth_decrypt(&this->aes, messageLength, iv, sizeof(iv), NULL, 0, NULL, 0, &mbusPayload[headerOffset + DLMS_PAYLOAD_OFFSET], plaintext);

                mbedtls_gcm_free(&this->aes);
#else
  #error "Invalid Platform"
#endif

                if(plaintext[0] != 0x0F || plaintext[5] != 0x0C)
                {
                    ESP_LOGE(TAG, "OBIS: Packet was decrypted but data is invalid");
                    return abort();
                }

                // Decoding

                ESP_LOGV(TAG, "Decoding payload");

                int currentPosition = DECODER_START_OFFSET;

                do
                {
                    if(plaintext[currentPosition + OBIS_TYPE_OFFSET] != DataType::OctetString)
                    {
                        ESP_LOGE(TAG, "OBIS: Unsupported OBIS header type");
                        return abort();
                    }

                    uint8_t obisCodeLength = plaintext[currentPosition + OBIS_LENGTH_OFFSET];

                    if(obisCodeLength != 0x06)
                    {
                        ESP_LOGE(TAG, "OBIS: Unsupported OBIS header length");
                        return abort();
                    }

                    uint8_t obisCode[obisCodeLength];
                    memcpy(&obisCode[0], &plaintext[currentPosition + OBIS_CODE_OFFSET], obisCodeLength); // Copy OBIS code to array

                    currentPosition += obisCodeLength + 2; // Advance past code, position and type

                    uint8_t dataType = plaintext[currentPosition];
                    currentPosition++; // Advance past data type

                    uint8_t dataLength = 0x00;

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
                            ESP_LOGW(TAG, "OBIS: Unsupported OBIS code");
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
                            ESP_LOGW(TAG, "OBIS: Unsupported OBIS code");
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "OBIS: Unsupported OBIS medium");
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
                            ESP_LOGE(TAG, "OBIS: Unsupported OBIS data type");
                            return abort();
                        break;
                    }

                    currentPosition += dataLength; // Skip data length

                    currentPosition += 2; // Skip break after data

                    if(plaintext[currentPosition] == 0x0F) // There is still additional data for this type, skip it
                        currentPosition += 6; // Skip additional data and additional break; this will jump out of bounds on last frame
                }
                while (currentPosition <= messageLength); // Loop until arrived at end

                this->receiveBuffer.clear(); // Reset buffer

                ESP_LOGI(TAG, "Received valid data");

                if(this->mqtt_client != NULL)
                {
                    this->mqtt_client->publish_json(this->topic, [=](JsonObject root)
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
            this->receiveBuffer.clear();
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

        void DlmsMeter::set_key(uint8_t key[], size_t keyLength)
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

        void DlmsMeter::log_packet(std::vector<uint8_t> data)
        {
            ESP_LOGV(TAG, format_hex_pretty(data).c_str());
        }
    }
}
