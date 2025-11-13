#include <zephyr/drivers/smbus.h>

int pmbus_set_page(const struct smbus_dt_spec *dev, int page, int phase);
int pmbus_write_byte(const struct smbus_dt_spec *dev, int page, uint8_t value);
int pmbus_write_word_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint16_t word);
int pmbus_write_byte_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t val);
int pmbus_read_word_data(const struct smbus_dt_spec *dev, int page, int phase, uint8_t reg, uint16_t *val);
int pmbus_read_byte_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t *val);
int pmbus_read_block_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t *count, uint8_t *val);

enum pmbus_cmds {
	PMBUS_PAGE			= 0x00,
	PMBUS_OPERATION			= 0x01,
	PMBUS_ON_OFF_CONFIG		= 0x02,
	PMBUS_CLEAR_FAULT		= 0x03,
	PMBUS_PHASE			= 0x04,

	PMBUS_WRITE_PROTECT		= 0x10,
	PMBUS_STORE			= 0x15,
	PMBUS_RESTORE			= 0x16,
	PMBUS_CAPABILITY		= 0x19,	

	PMBUS_VOUT_MODE			= 0x20,
	PMBUS_VOUT_CMD			= 0x21,
	PMBUS_VOUT_MAX			= 0x24,
	PMBUS_VOUT_MARGIN_HI		= 0x25,
	PMBUS_VOUT_MARGIN_LOW		= 0x26,
	PMBUS_VOUT_SCALE_LOOP		= 0x29,
	PMBUS_VOUT_MIN			= 0x2B,

	PMBUS_COEFFICIENT		= 0x30,
	PMBUS_VIN_ON			= 0x35,
	PMBUS_VIN_OFF			= 0x36,
	PMBUS_IOUT_CAL_GAIN		= 0x38,
	PMBUS_IOUT_CAL_OFFSET		= 0x39,
	PMBUS_FAN_CONFIG_12		= 0x3A,
	PMBUS_FAN_COMMAND_1		= 0x3B,
	PMBUS_FAN_COMMAND_2		= 0x3C,
	PMBUS_FAN_CONFIG_34		= 0x3D,
	PMBUS_FAN_COMMAND_3		= 0x3E,
	PMBUS_FAN_COMMAND_4		= 0x3F,

	PMBUS_IOUT_OC_FAULT_LIMIT	= 0x46,
	PMBUS_IUOT_OC_WARN_LIMIT	= 0x4A,
	PMBUS_VBOOT_SET_FOR_X0H_ADDR	= 0x4D,
	PMBUS_OT_FAULT_LIMIT		= 0x4F,

	PMBUS_OT_WARN_LIMIT		= 0x51,
	PMBUS_VIN_OV_FAULT_LIMIT	= 0x55,
	PMBUS_VIN_OV_WARN_LIMIT		= 0x57,
	PMBUS_VBOOT_SET_FOR_X4H_ADDR	= 0x5E,
	PMBUS_VBOOT_SET_FOR_X8H_ADDR	= 0x5F,

	PMBUS_TON_DELAY			= 0x60,
	PMBUS_TON_RISE			= 0x61,
	PMBUS_TOFF_DELAY		= 0x64,
	PMBUS_TOFF_FALL			= 0x65,
	PMBUS_VBOOT_SET_FOR_XEH_ADDR	= 0x6A,

	PMBUS_STATUS_WORD		= 0x79,
	PMBUS_STATUS_VOUT		= 0x7A,
	PMBUS_STATUS_IOUT		= 0x7B,
	PMBUS_STATUS_INPUT		= 0x7C,
	PMBUS_STATUS_TEMP		= 0x7D,
	PMBUS_STATUS_CML		= 0x7E,

	PMBUS_STATUS_MFR_SPECIFIC	= 0x80,
	PMBUS_READ_VIN			= 0x88,
	PMBUS_READ_VOUT			= 0x8B,
	PMBUS_READ_IOUT			= 0x8C,	
	PMBUS_READ_TEMP_1		= 0x8D,
	PMBUS_READ_TEMP_2		= 0x8E,
	PMBUS_READ_TEMP_3		= 0x8F,

	PMBUS_READ_FAN_SPEED_1		= 0x90,
	PMBUS_READ_FAN_SPEED_2		= 0x91,
	PMBUS_READ_FAN_SPEED_3		= 0x92,
	PMBUS_READ_FAN_SPEED_4		= 0x93,
	PMBUS_READ_DUTY_CYCLE		= 0x94,
	PMBUS_READ_FREQUENCY		= 0x95,

	PMBUS_REVISION			= 0x98,
	PMBUS_MFR_ID			= 0x99,
	PMBUS_MFR_MODEL			= 0x9A,
	PMBUS_MFR_REVISION		= 0x9B,
	PMBUS_MFR_LOCATION		= 0x9C,
	PMBUS_MFR_DATA			= 0x9D,
	PMBUS_MFR_SERIAL		= 0x9E,

};
