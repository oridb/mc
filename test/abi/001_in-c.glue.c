/* CFLAGS: -I/usr/include/ */
/* CFLAGS: -I/usr/include */
/* LIBS: c */

#include <stdint.h>
#include "001_types.h"

extern t1 fn_1_myr(void);

t1
fn_1_c(void)
{
    return (t1) {.field_1=(t2) {.field_1=249229305184256,.field_2=3.36328125}};
}

int
const check_c_to_myr_fns(void)
{
    t1 ret_1 = fn_1_myr();
    if (!(((ret_1.field_1.field_1==249229305184256) && (ret_1.field_1.field_2==3.36328125)))) {
        return -1;
    }

    return 0;
}
