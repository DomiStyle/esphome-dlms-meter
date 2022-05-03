/*
 * Data types as per specification
 */

enum DataType
{
    NullData = 0x00,
    Boolean = 0x03,
    BitString = 0x04,
    DoubleLong = 0x05,
    DoubleLongUnsigned = 0x06,
    OctetString = 0x09,
    VisibleString = 0x0A,
    Utf8String = 0x0C,
    BinaryCodedDecimal = 0x0D,
    Integer = 0x0F,
    Long = 0x10,
    Unsigned = 0x11,
    LongUnsigned = 0x12,
    Long64 = 0x14,
    Long64Unsigned = 0x15,
    Enum = 0x16,
    Float32 = 0x17,
    Float64 = 0x18,
    DateTime = 0x19,
    Date = 0x1A,
    Time = 0x1B,

    Array = 0x01,
    Structure = 0x02,
    CompactArray = 0x13
};

enum Medium
{
    Abstract = 0x00,
    Electricity = 0x01,
    Heat = 0x06,
    Gas = 0x07,
    Water = 0x08
};

enum CodeType
{
    Unknown,
    Timestamp,
    SerialNumber,
    DeviceName,
    MeterNumber,
    VoltageL1,
    VoltageL2,
    VoltageL3,
    CurrentL1,
    CurrentL2,
    CurrentL3,
    ActivePowerPlus,
    ActivePowerMinus,
    PowerFactor,
    ActiveEnergyPlus,
    ActiveEnergyMinus,
    ReactiveEnergyPlus,
    ReactiveEnergyMinus
};

enum Accuracy
{
    SingleDigit = 0xFF,
    DoubleDigit = 0xFE
};

/*
 * Data structure
 */

static const int DECODER_START_OFFSET = 20; // Offset for start of OBIS decoding, skip header, timestamp and break block

static const int OBIS_TYPE_OFFSET = 0;
static const int OBIS_LENGTH_OFFSET = 1;

static const int OBIS_CODE_OFFSET = 2;

static const int OBIS_A = 0;
static const int OBIS_B = 1;
static const int OBIS_C = 2;
static const int OBIS_D = 3;
static const int OBIS_E = 4;
static const int OBIS_F = 5;

/*
 * Metadata
 */

static byte ESPDM_TIMESTAMP[]
{
    0x01, 0x00
};

static const byte ESPDM_SERIAL_NUMBER[]
{
    0x60, 0x01
};

static const byte ESPDM_DEVICE_NAME[]
{
    0x2A, 0x00
};

/*
 * Voltage
 */

static byte ESPDM_VOLTAGE_L1[]
{
    0x20, 0x07
};

static const byte ESPDM_VOLTAGE_L2[]
{
    0x34, 0x07
};

static const byte ESPDM_VOLTAGE_L3[]
{
    0x48, 0x07
};

/*
 * Current
 */

static const byte ESPDM_CURRENT_L1[]
{
    0x1F, 0x07
};

static const byte ESPDM_CURRENT_L2[]
{
    0x33, 0x07
};

static const byte ESPDM_CURRENT_L3[]
{
    0x47, 0x07
};

/*
 * Power
 */

static const byte ESPDM_ACTIVE_POWER_PLUS[]
{
    0x01, 0x07
};

static const byte ESPDM_ACTIVE_POWER_MINUS[]
{
    0x02, 0x07
};
static const byte ESPDM_POWER_FACTOR[]
{
    0x0D, 0x07
};

/*
 * Active energy
 */

static const byte ESPDM_ACTIVE_ENERGY_PLUS[]
{
    0x01, 0x08
};
static const byte ESPDM_ACTIVE_ENERGY_MINUS[]
{
    0x02, 0x08
};

/*
 * Reactive energy
 */

static const byte ESPDM_REACTIVE_ENERGY_PLUS[]
{
    0x03, 0x08
};
static const byte ESPDM_REACTIVE_ENERGY_MINUS[]
{
    0x04, 0x08
};
