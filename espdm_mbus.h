/*
 * Data structure
 */

static const int MBUS_HEADER_INTRO_LENGTH = 4; // Header length for the intro (0x68, length, length, 0x68)
static const int MBUS_FULL_HEADER_LENGTH = 9; // Total header length
static const int MBUS_FOOTER_LENGTH = 2; // Footer after frame

static const int MBUS_MAX_FRAME_LENGTH = 250; // Maximum size of frame

static const int MBUS_START1_OFFSET = 0; // Offset of first start byte
static const int MBUS_LENGTH1_OFFSET = 1; // Offset of first length byte
static const int MBUS_LENGTH2_OFFSET = 2; // Offset of (duplicated) second length byte
static const int MBUS_START2_OFFSET = 3; // Offset of (duplicated) second start byte
