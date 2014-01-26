/* 240562:tc.chen fixed H2DCE_DEBUG_READ_DM byte alignment */
#define MSG_LENGTH              16
#define MSG_LENGTH_WORDS        32

#define	NEW_MP_PAYLOAD_SIZE	12
/* 240562:tc.chen start */
typedef struct {
	unsigned short iFunction;
	unsigned short iGroup;
	unsigned short iAddress;
	unsigned short iIndex;
	unsigned short iPayload[12];
}MPMessage;
/* 240562:tc.chen end */

// Group Fields
#define	GROUP_UNDEFINED		(0)
#define	GROUP_CNTL		(1)
#define	GROUP_STAT		(2)
#define	GROUP_INFO		(3)
#define	GROUP_TEST		(4)
#define	GROUP_OPTN		(5)
#define	GROUP_RATE		(6)
#define	GROUP_PLAM		(7)
#define GROUP_CNFG		(8)

// CMV mode OpCodes 
// H2D Host-to-DSP
#define	H2D_CMV_READ				(0x00)
#define	H2D_CMV_WRITE				(0x04)
#define	H2D_CMV_INDICATE_REPLY			(0x10)
#define	H2D_ERROR_OPCODE_UNKNOWN		(0x20)
#define	H2D_ERROR_CMV_UNKNOWN			(0x30)

// D2H DSP-to-Host
#define	D2H_CMV_READ_REPLY			(0x01)
#define	D2H_CMV_WRITE_REPLY			(0x05)
#define	D2H_CMV_INDICATE			(0x11)
#define	D2H_ERROR_OPCODE_UNKNOWN		(0x21)
#define	D2H_ERROR_CMV_UNKNOWN			(0x31)
#define D2H_ERROR_CMV_READ_NOT_AVAILABLE	(0x41)
#define	D2H_ERROR_CMV_WRITE_ONLY		(0x51)
#define	D2H_ERROR_CMV_READ_ONLY			(0x61)

// Debug Mode OpCodes
//	Host-to-DSP
#define	H2D_DEBUG_READ_DM			(0x02)
#define	H2D_DEBUG_READ_PM			(0x06)
#define	H2D_DEBUG_WRITE_DM			(0x0A)
#define	H2D_DEBUG_WRITE_PM			(0x0E)
#define	H2D_DEBUG_READ_EXT_DM			(0x12)
#define	H2D_DEBUG_READ_EXT_PM			(0x16)
#define	H2D_DEBUG_WRITE_EXT_DM			(0x1A)
#define	H2D_DEBUG_WRITE_EXT_PM			(0x1E)

//	DSP-to-Host
#define	D2H_DEBUG_READ_DM_REPLY			(0x03)
#define	D2H_DEBUG_READ_PM_REPLY			(0x07)
#define	D2H_DEBUG_WRITE_DM_REPLY		(0x0B)
#define	D2H_DEBUG_WRITE_PM_REPLY		(0x0F)
#define	D2H_DEBUG_READ_EXT_DM_REPLY		(0x13)
#define	D2H_DEBUG_READ_EXT_PM_REPLY		(0x17)
#define	D2H_DEBUG_WRITE_EXT_DM_REPLY		(0x1B)
#define	D2H_DEBUG_WRITE_EXT_PM_REPLY		(0x1F)

#define	D2H_ERROR_ADDR_UNKNOWN			(0x33)

//	Host-to-DCE
#define H2DCE_DEBUG_RESET			(0x02)
#define H2DCE_DEBUG_REBOOT			(0x04)
#define H2DCE_DEBUG_READ_MEI			(0x06)
#define H2DCE_DEBUG_WRITE_MEI			(0x08)
#define	H2DCE_DEBUG_DOWNLOAD			(0x0A)
#define	H2DCE_DEBUG_RUN				(0x0C)
#define	H2DCE_DEBUG_HALT			(0x0E)
#define	H2DCE_DEBUG_REMOTE			(0x10)
#define	H2DCE_DEBUG_READBUF			(0x12)
#define	H2DCE_DEBUG_READDEBUG			(0x14)
#define	H2DCE_DEBUG_DESCRIPTORSET		(0x16)
#define	H2DCE_DEBUG_DESCRIPTORSTAT		(0x18)
#define	H2DCE_DEBUG_DESCRIPTORLOG		(0x1A)
#define	H2DCE_DEBUG_DESCRIPTORCLEAR		(0x1C)
#define	H2DCE_DEBUG_DESCRIPTORREAD		(0x1E)

//	DCE-to-Host
#define DCE2H_DEBUG_RESET_ACK			(0x03)
#define DCE2H_DEBUG_REBOOT_ACK			(0x05)
#define DCE2H_DEBUG_READ_MEI_REPLY		(0x07)
#define DCE2H_DEBUG_WRITE_MEI_REPLY		(0x09)
#define DCE2H_ERROR_OPCODE_UNKNOWN		(0x0B)
#define DCE2H_ERROR_ADDR_UNKNOWN		(0x0D)
#define	D2DCE_DEBUG_READBUF_ACK			(0x13)
#define	H2DCE_DEBUG_READDEBUG_ACK		(0x15)

#define DCE2H_DEBUG_HALT_ACK                  	(0x0F)
#define	DCE2H_ERROR_DMA				(0x34)

#define DCE_COMMAND				(0x1000)
// Mode Field options
#define MODE_CMV				(0)
#define MODE_DEBUG				(1)

// Direction field options
#define DIR_H2D					(0)
#define DIR_D2H					(1)

// Reserved bits
#define	NEWMP_RESERVED_CMV_READ			(0xA)
#define	NEWMP_RESERVED_CMV_WRITE_NO_ACK		(0x4)
#define	NEWMP_RESERVED_CMV_WRITE_ACK		(0x6)
#define NEW_MP_SIZE				(16+1)

