Video link: ***NOTE: RUNTIME OF GREENLIGHTS IS REDUCED TO 10 SECONDS FOR TESTING/PRACTICALITY PURPOSES***
https://youtu.be/L9B8R9sqi3o



To run trafficLights.c:
1. gcc -pthread trafficLightsMultiThread.c -o trafficLightsMultiThread
2. ./trafficLightsMultiThread

MISRA Reflection:

Many of the violations are continuations of issues previously seen in the first implementation of the trafficLight project.  Again, issues regarding the 2012 MISRA coding standard stemmed from the use of array reference formatting and un-explicit use of function return values. To fix the function return value warning, I cast all unused return values to void type.

There is a very large number of violations that originate from the “Basic numerical type ‘char’ used in ‘char *’ warning.  Again, MISRA guidelines state that typedefs that indicate size and signedness should be typically used.  However, since the contents being passed are intended to be strings, the use of this particular type is correct.  Any other signed and size-indicated type would result in a compiler warning when passed into the sprintf() function or many other standard library functionality.  Other issues included naming conventions, in which variable such as thread1 and thread2 were too ambiguous for the MISRA's standards.  However, some of these are unavoidable or unwarranted.  In my opinion, the variable names trafficLight1Thread and trafficLightCycle are sufficiently different to avoid confusion to a human reader.

The other issue involves the use of standard library functions.  The first warning involves the use of the ‘printf’ function, while the last five warnings on the report are regarding the use of ‘sprintf’. However, in addition to the stdio.h warnings, time.h is also included.  I attempted to mitigate undefined behavior with the use of time.h by using it in conjunction with mutexes as much as possible.


