/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

extern void import_Toplevel_namespace(void);
extern void import_Debug_namespace(void);
extern void import_File_namespace(void);
extern void import_History_namespace(void);
extern void import_Math_namespace(void);
#ifdef BSD_RANDOM
extern void import_BSDRandom_namespace(void);
#endif
extern void import_Semaphore_namespace(void);
extern void import_String_namespace(void);
extern void import_Thread_namespace(void);
extern void import_Command_namespace(void);
#ifdef GCD_DEBUG
extern void import_Gcd_namespace(void);
#endif
extern void import_Environ_namespace(void);
extern void import_Socket_namespace(void);
extern void import_Foreign_namespace(void);
extern void import_PID_namespace(void);
extern void import_Date_namespace(void);
