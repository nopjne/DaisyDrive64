#define OVERCLOCK 1

extern uint32_t RomMaxSize;
extern unsigned char *ram;
extern uint32_t *LogBuffer;
extern BYTE *EepromInputLog;
extern BYTE *Sram4Buffer;

#define USER_LED_PORTC (1 << 7)