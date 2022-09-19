#define OVERCLOCK 1

extern uint32_t RomMaxSize;
extern unsigned char *ram;
extern uint32_t *const LogBuffer;
extern BYTE *const EepromInputLog;
extern BYTE *const Sram4Buffer;

#define USER_LED_PORTC (1 << 7)
#define WRAP_ROM_INDEX(Index) (Index % (sizeof(RomName)/sizeof(RomName[0])))