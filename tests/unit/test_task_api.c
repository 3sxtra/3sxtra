#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdbool.h>

#include "main.h"
#include "structs.h"
#include "port/task_api.h"

extern struct _TASK task[11];

static void test_task_api(void **state) {
    (void)state;

    // Reset tasks
    for (int i = 0; i < 11; i++) {
        task[i].condition = 0;
        task[i].r_no[0] = 0;
        task[i].r_no[2] = 0;
        task[i].free[0] = 0;
    }

    assert_false(Task_IsActive(TASK_GAME));
    Task_Activate(TASK_GAME);
    assert_true(Task_IsActive(TASK_GAME));
    assert_int_equal(task[TASK_GAME].condition, 1);

    Task_Deactivate(TASK_GAME);
    assert_false(Task_IsActive(TASK_GAME));
    assert_int_equal(task[TASK_GAME].condition, 0);

    struct _TASK* p = Task_GetPtr(TASK_PAUSE);
    assert_ptr_equal(p, &task[TASK_PAUSE]);

    PauseTask_SetPhase(5);
    assert_int_equal(task[TASK_PAUSE].r_no[2], 5);

    PauseTask_SetTimer(100);
    assert_int_equal(task[TASK_PAUSE].free[0], 100);

    Saver2_Task_SetPhase(2);
    assert_int_equal(task[7].r_no[0], 2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_task_api),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
