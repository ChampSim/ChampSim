#include <stdio.h>
#include <stdlib.h>
#include <random>
#include <time.h>

#define NUM_MIX  100
#define NUM_CPUS 4
#define NUM_TRACE 20

using namespace std;
default_random_engine generator;

int main()
{
    int benchmark[NUM_MIX][NUM_CPUS];
    for (int i=0; i<NUM_MIX; i++)
        for (int j=0; j<NUM_CPUS; j++)
            benchmark[i][j] = -1;

    // Random seed
    generator.seed(time(NULL));

    // Set range
    const int rand_min = 1;
    const int rand_max = NUM_TRACE; 
    uniform_int_distribution<int> distribution(rand_min, rand_max);

    int temp_rand;
    bool do_again = false;
    
    for (int i = 0; i < NUM_MIX; i++) {
        //printf("MIX%2d: ", i+1);
        for (int j = 0; j < NUM_CPUS; j++) {
            do  {
                do_again = false;
                temp_rand = distribution(generator); // Generate random integer flat in [rand_min, rand_mix]
                for (int k = 0; k < j; k++) {
                    if (temp_rand == benchmark[i][k]) {
                        do_again = true;
                        break;
                    }
                }
            } while (do_again);

            benchmark[i][j] = temp_rand;
            printf("%d ", benchmark[i][j]);
        }
        printf("\n");
    }

    return 0;
}
