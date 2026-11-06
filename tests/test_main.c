#include "test.h"

int g_failures;
int g_checks;

void test_program_all(void);
void test_player_all(void);
void test_app_all(void);

int main(void)
{
    test_program_all();
    test_player_all();
    test_app_all();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
