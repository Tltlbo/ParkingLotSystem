#include <stdio.h>\nint main() { char s=0; int n=0, st=0; int r = sscanf("A-01:1", "%c-%d:%d", &s, &n, &st); printf("r=%d s=%c n=%d st=%d\\n", r, s, n, st); return 0; }
