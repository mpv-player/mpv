//------------------------------------------------------------------------------
// File: Measure.h
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

/*
   The idea is to pepper the source code with interesting measurements and
   have the last few thousand of these recorded in a circular buffer that
   can be post-processed to give interesting numbers.

   WHAT THE LOG LOOKS LIKE:

  Time (sec)   Type        Delta  Incident_Name
    0.055,41  NOTE      -.       Incident Nine  - Another note
    0.055,42  NOTE      0.000,01 Incident Nine  - Another note
    0.055,44  NOTE      0.000,02 Incident Nine  - Another note
    0.055,45  STOP      -.       Incident Eight - Also random
    0.055,47  START     -.       Incident Seven - Random
    0.055,49  NOTE      0.000,05 Incident Nine  - Another note
    ------- <etc.  there is a lot of this> ----------------
    0.125,60  STOP      0.000,03 Msr_Stop
    0.125,62  START     -.       Msr_Start
    0.125,63  START     -.       Incident Two   - Start/Stop
    0.125,65  STOP      0.000,03 Msr_Start
    0.125,66  START     -.       Msr_Stop
    0.125,68  STOP      0.000,05 Incident Two   - Start/Stop
    0.125,70  STOP      0.000,04 Msr_Stop
    0.125,72  START     -.       Msr_Start
    0.125,73  START     -.       Incident Two   - Start/Stop
    0.125,75  STOP      0.000,03 Msr_Start
    0.125,77  START     -.       Msr_Stop
    0.125,78  STOP      0.000,05 Incident Two   - Start/Stop
    0.125,80  STOP      0.000,03 Msr_Stop
    0.125,81  NOTE      -.       Incident Three - single Note
    0.125,83  START     -.       Incident Four  - Start, no stop
    0.125,85  START     -.       Incident Five  - Single Start/Stop
    0.125,87  STOP      0.000,02 Incident Five  - Single Start/Stop

Number      Average       StdDev     Smallest      Largest Incident_Name
    10     0.000,58     0.000,10     0.000,55     0.000,85 Incident One   - Note
    50     0.000,05     0.000,00     0.000,05     0.000,05 Incident Two   - Start/Stop
     1     -.           -.           -.           -.       Incident Three - single Note
     0     -.           -.           -.           -.       Incident Four  - Start, no stop
     1     0.000,02     -.           0.000,02     0.000,02 Incident Five  - Single Start/Stop
     0     -.           -.           -.           -.       Incident Six   - zero occurrences
   100     0.000,25     0.000,12     0.000,02     0.000,62 Incident Seven - Random
   100     0.000,79     0.000,48     0.000,02     0.001,92 Incident Eight - Also random
  5895     0.000,01     0.000,01     0.000,01     0.000,56 Incident Nine  - Another note
    10     0.000,03     0.000,00     0.000,03     0.000,04 Msr_Note
    50     0.000,03     0.000,00     0.000,03     0.000,04 Msr_Start
    50     0.000,04     0.000,03     0.000,03     0.000,31 Msr_Stop

  WHAT IT MEANS:
    The log shows what happened and when.  Each line shows the time at which
    something happened (see WHAT YOU CODE below) what it was that happened
    and (if approporate) the time since the corresponding previous event
    (that's the delta column).

    The statistics show how many times each event occurred, what the average
    delta time was, also the standard deviation, largest and smalles delta.

   WHAT YOU CODE:

   Before anything else executes: - register your ids

    int id1     = Msr_Register("Incident One   - Note");
    int id2     = Msr_Register("Incident Two   - Start/Stop");
    int id3     = Msr_Register("Incident Three - single Note");
    etc.

   At interesting moments:

       // To measure a repetitive event - e.g. end of bitblt to screen
       Msr_Note(Id9);             // e.g. "video frame hiting the screen NOW!"

           or

       // To measure an elapsed time e.g. time taken to decode an MPEG B-frame
       Msr_Start(Id2);            // e.g. "Starting to decode MPEG B-frame"
         . . .
       MsrStop(Id2);              //      "Finished MPEG decode"

   At the end:

       HANDLE hFile;
       hFile = CreateFile("Perf.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
       Msr_Dump(hFile);           // This writes the log out to the file
       CloseHandle(hFile);

           or

       Msr_Dump(NULL);            // This writes it to DbgLog((LOG_TRACE,0, ... ));
                                  // but if you are writing it out to the debugger
                                  // then the times are probably all garbage because
                                  // the debugger can make things run awfully slow.

    A given id should be used either for start / stop or Note calls.  If Notes
    are mixed in with Starts and Stops their statistics will be gibberish.

    If you code the calls in upper case i.e. MSR_START(idMunge); then you get
    macros which will turn into nothing unless PERF is defined.

    You can reset the statistical counts for a given id by calling Reset(Id).
    They are reset by default at the start.
    It logs Reset as a special incident, so you can see it in the log.

    The log is a circular buffer in storage (to try to minimise disk I/O).
    It overwrites the oldest entries once full.  The statistics include ALL
    incidents since the last Reset, whether still visible in the log or not.
*/

#ifndef __MEASURE__
#define __MEASURE__

#ifdef PERF
#define MSR_INIT() Msr_Init()
#define MSR_TERMINATE() Msr_Terminate()
#define MSR_REGISTER(a) Msr_Register(a)
#define MSR_RESET(a) Msr_Reset(a)
#define MSR_CONTROL(a) Msr_Control(a)
#define MSR_START(a) Msr_Start(a)
#define MSR_STOP(a) Msr_Stop(a)
#define MSR_NOTE(a) Msr_Note(a)
#define MSR_INTEGER(a, b) Msr_Integer(a, b)
#define MSR_DUMP(a) Msr_Dump(a)
#define MSR_DUMPSTATS(a) Msr_DumpStats(a)
#else
#define MSR_INIT() ((void)0)
#define MSR_TERMINATE() ((void)0)
#define MSR_REGISTER(a) 0
#define MSR_RESET(a) ((void)0)
#define MSR_CONTROL(a) ((void)0)
#define MSR_START(a) ((void)0)
#define MSR_STOP(a) ((void)0)
#define MSR_NOTE(a) ((void)0)
#define MSR_INTEGER(a, b) ((void)0)
#define MSR_DUMP(a) ((void)0)
#define MSR_DUMPSTATS(a) ((void)0)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    // This must be called first - (called by the DllEntry)

    void WINAPI Msr_Init(void);

    // Call this last to clean up (or just let it fall off the end - who cares?)

    void WINAPI Msr_Terminate(void);

    // Call this to get an Id for an "incident" that you can pass to Start, Stop or Note
    // everything that's logged is called an "incident".

    int WINAPI Msr_Register(__in LPTSTR Incident);

    // Reset the statistical counts for an incident

    void WINAPI Msr_Reset(int Id);

// Reset all the counts for all incidents
#define MSR_RESET_ALL 0
#define MSR_PAUSE 1
#define MSR_RUN 2

    void WINAPI Msr_Control(int iAction);

    // log the start of an operation

    void WINAPI Msr_Start(int Id);

    // log the end of an operation

    void WINAPI Msr_Stop(int Id);

    // log a one-off or repetitive operation

    void WINAPI Msr_Note(int Id);

    // log an integer (on which we can see statistics later)
    void WINAPI Msr_Integer(int Id, int n);

    // print out all the vaialable log (it may have wrapped) and then the statistics.
    // When the log wraps you lose log but the statistics are still complete.
    // hFIle==NULL => use DbgLog
    // otherwise hFile must have come from CreateFile or OpenFile.

    void WINAPI Msr_Dump(HANDLE hFile);

    // just dump the statistics - never mind the log

    void WINAPI Msr_DumpStats(HANDLE hFile);

    // Type definitions in case you want to declare a pointer to the dump functions
    // (makes it a trifle easier to do dynamic linking
    // i.e. LoadModule, GetProcAddress and call that)

    // Typedefs so can declare MSR_DUMPPROC *MsrDumpStats; or whatever
    typedef void WINAPI MSR_DUMPPROC(HANDLE hFile);
    typedef void WINAPI MSR_CONTROLPROC(int iAction);

#ifdef __cplusplus
}
#endif

#endif // __MEASURE__
