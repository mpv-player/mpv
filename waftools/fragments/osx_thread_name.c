#include <pthread.h>
int main(int argc, char **argv) {
    pthread_setname_np("ducks");
    return 0;
}
