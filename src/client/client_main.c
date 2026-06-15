#include "client.h"
#include "config.h"

int main(void) {
    start_client(SERVER_IP, SERVER_PORT);
    return 0;
}
