#include <stdio.h>
#include <inttypes.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("Usage: bintoarray filename arrayname");
        return 1;
    }

    bool headeronly = false;
    if ((argc >= 4) && (strcmp(argv[3], "-h") == 0)) {
        headeronly = true;
    }

    char* inp = argv[1];
    FILE* file;
    errno_t success = fopen_s(&file, inp, "rb");
    if (success != 0) {
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t sizefile = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (headeronly == false) {
        printf("unsigned char %s[%i] = {\n    ", argv[2], (int)sizefile);
    
        uint32_t i = 0;
        while (feof(file) == false) {
            uint8_t byte;
            if (fread(&byte, 1, 1, file) != 0) {
                i += 1;
                printf("0x%02X, ", byte);
                if ((i % 16) == 0) {
                    printf("\n    ");
                }
            }
        }
        fclose(file);
        printf("};\n");
    } else {
        printf("extern unsigned char %s[%i];\n", argv[2], (int)sizefile);
    }

    return 0;
}