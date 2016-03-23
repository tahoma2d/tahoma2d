
To compile SuperLU:

    Use the Visual Studio project located in the /prj folder.

IMPORTANT (THREAD-SAFETY):

    A slight modification had to be applied to the original sources of the used version (4.1).

    The edit concerns the presence of static variables, that make SuperLU *NOT* thread-safe.
    There is a SuperLU version for parallel architectures (SMT), but it's obviously geared
    toward internal thread management - which is not what we want.

    The only edit we've done, located in "dgstrf.c", fixes a crash when allocating resources
    for LU factorization:

        In "void dgstrf (..)":

            > /*static*/ GlobalLU_t Glu; /* persistent to facilitate multiple factors.  */

                                         /* DANIELE: tolto static, nocivo x multithread */

            > memset(&Glu, 0, sizeof(GlobalLU_t));       // static inizializzava tutto a 0

    There are other sparse static variables, which we've not had the time to double-check.
    Everything seems to work.

    In case it is necessary to further modify source files, remember fiy:

    * Files NOT starting with "d" - ie "double" - are not interesting; they are intended
      for single float and complex numbers, we're only sticking to doubles.

    * We're mainly interested in LU factorization, meaning only dgstrf ramification should
      be tracked.
