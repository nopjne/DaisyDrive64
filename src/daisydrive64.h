#define OVERCLOCK 1

extern uint32_t RomMaxSize;
extern unsigned char *ram;
extern uint32_t *const LogBuffer;
extern BYTE *const EepromInputLog;
extern BYTE *const Sram4Buffer;

#define USER_LED_PORTC (1 << 7)
#define WRAP_ROM_INDEX(Index) (Index % (sizeof(RomSettings)/sizeof(RomSettings[0])))
#define USE_OPEN_DRAIN_OUTPUT 0