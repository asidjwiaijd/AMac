#include "sd_maintenance.h"
#include <stdio.h>
#include <string.h>

// A real temporary filesystem backs the production command processor. The
// Python tests drive it through exactly the same replies as the serial client.
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    SdMaintenance files(argv[1]);
    if (!files.recover()) return 3;
    char line[1024],reply[600];
    while (fgets(line,sizeof(line),stdin)) {
        line[strcspn(line,"\r\n")]=0;
        files.execute(line,reply,sizeof(reply));
        printf("@B2 %s\n",reply); fflush(stdout);
    }
}
