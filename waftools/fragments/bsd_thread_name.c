#include <pthread.h>
#include <pthread_np.h>
int main(int argc, char **argv) {
    pthread_set_name_np(pthread_self(), "ducks");
    return 0;
}
