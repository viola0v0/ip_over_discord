#ifdef _WIN32
  #include <windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "base64.h"

int main() {
#ifdef _WIN32
    // 切换控制台输入输出到 UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setlocale(LC_ALL, "");
    const char *tests[] = {
        "", "f", "fo", "foo", "foob", "fooba", "foobar"
    };
    for (int i = 0; i < sizeof(tests)/sizeof(*tests); i++) {
        const char *s = tests[i];
        size_t enc_len, dec_len;
        char *enc = base64_encode((const unsigned char*)s, strlen(s), &enc_len);
        unsigned char *dec = base64_decode(enc, &dec_len);
        printf("Input: \"%s\"\n  Encoded: \"%s\"\n  Decoded: \"%.*s\"\n\n",
               s, enc, (int)dec_len, dec);
        free(enc);
        free(dec);
    }
    base64_cleanup();
    return 0;
}
