/*
 * Data structure
 */

static const int DLMS_HEADER_LENGTH = 16; // Length of the header (total message length <= 127)
static const int DLMS_HEADER_EXT_OFFSET = 2; // Length to offset when header is extended length (total message length > 127)

static const int DLMS_CIPHER_OFFSET = 0; // Offset at which used cipher suite is stored
static const int DLMS_SYST_OFFSET = 1; // Offset at which length of system title is stored

static const int DLMS_LENGTH_OFFSET = 10; // Offset at which message length is stored
static const int DLMS_LENGTH_CORRECTION = 5; // Part of the header is included in the DLMS length field and needs to be removed

// Bytes after length may be shifted depending on length field

static const int DLMS_SECBYTE_OFFSET = 11; // Offset of the security byte

static const int DLMS_FRAMECOUNTER_OFFSET = 12; // Offset of the frame counter
static const int DLMS_FRAMECOUNTER_LENGTH = 4; // Length of the frame counter (always 4)

static const int DLMS_PAYLOAD_OFFSET = 16; // Offset at which the encrypted payload begins
