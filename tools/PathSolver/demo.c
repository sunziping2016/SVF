#include <stdio.h>

extern int a;

int main() {
    int b, *c = 0;
    if (a) {
        b = 1;
    } else {
        b = 0;
        c = &a;
    }

    if (b == 0) {
        // shouldn't report npd
        return *c;
    }
    return 0;
}
