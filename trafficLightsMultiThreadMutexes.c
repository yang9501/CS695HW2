#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/utsname.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>

//comment out to live run
//#define DEBUG 1

#define GPIO_PATH_44 "/sys/class/gpio/gpio44" //Green 1
#define GPIO_PATH_68 "/sys/class/gpio/gpio68" //Yellow 1
#define GPIO_PATH_67 "/sys/class/gpio/gpio67" //Red 1

#define GPIO_PATH_26 "/sys/class/gpio/gpio26" //Green 2
#define GPIO_PATH_46 "/sys/class/gpio/gpio46" //Yellow 2
#define GPIO_PATH_65 "/sys/class/gpio/gpio65" //Red 2

#define GPIO_PATH_66 "/sys/class/gpio/gpio66" //Button 1
#define GPIO_PATH_69 "/sys/class/gpio/gpio69" //Button 2

#define GREEN_LIGHT_TIME 10
#define YELLOW_LIGHT_TIME 5

//Writes specified value to specified GPIO directory
static void writeLED(char *filename, char *port, char *value);

//Reads input to GPIO pin
static int readGPIO(char *filename, char *port);

static void setLightInitialState(char *greenPort, char *yellowPort, char *redPort);

//Primary light switch logic
void cycleTrafficLight1();
void cycleTrafficLight2();

//Primary button press detection
void getButton1PressDuration();
void getButton2PressDuration();

//Signal handlers for hot-swapping light cycling via button press interrupts
void trafficLight1InterruptHandler();
void trafficLight2InterruptHandler();

void testTrafficLight1();
void testTrafficLight2();

void trafficLight1Cycle();
void trafficLight2Cycle();

pthread_t thread1, thread2, thread3, thread4;
sigset_t trafficLight1Set, trafficLight2Set;

pthread_mutex_t timerMutex;
time_t start_time;
time_t end_time;

pthread_mutex_t batonMutex;
int baton = 0;

int main(void) {
    //arrays containing GPIO port definitions, representing each of the two traffic lights
	char trafficLight1Ports[3][25] = {GPIO_PATH_44, GPIO_PATH_68, GPIO_PATH_67};
	char trafficLight2Ports[3][25] = {GPIO_PATH_26, GPIO_PATH_46, GPIO_PATH_65};
    char buttonPorts[2][25] = {GPIO_PATH_66, GPIO_PATH_69};

    #ifdef DEBUG
    (void) printf("DEBUG MODE\n");
    struct utsname sysInfo;
    (void) uname(&sysInfo);
    (void) printf("%s\n", sysInfo.sysname);
    (void) printf("%s\n", sysInfo.nodename);
    (void) printf("%s\n", sysInfo.machine);
    #else
    for (int i = 0; i < 3; i++) {
        (void) writeLED("/direction", trafficLight1Ports[i], "out");
        (void) writeLED("/direction", trafficLight2Ports[i], "out");
    }

    for (int i = 0; i < 2; i++) {
        (void) writeLED("/direction", buttonPorts[i], "in");
    }
    #endif

    setLightInitialState(trafficLight1Ports[0], trafficLight1Ports[1], trafficLight1Ports[2]);
    setLightInitialState(trafficLight2Ports[0], trafficLight2Ports[1], trafficLight2Ports[2]);

    pthread_mutex_init(&batonMutex, NULL);
    pthread_mutex_init(&timerMutex, NULL);

    /* Create independent threads each of which will execute function */
    pthread_create( &thread1, NULL, (void*) getButton1PressDuration, NULL);
    pthread_create( &thread2, NULL, (void*) getButton2PressDuration, NULL);
    //pthread_create( &thread3, NULL, (void *) test, NULL);
    //pthread_create( &thread4, NULL, (void *) cycleTrafficLight2, NULL);

    pthread_create( &thread3, NULL, (void *) testTrafficLight1, NULL);
    pthread_create( &thread4, NULL, (void *) testTrafficLight2, NULL);

    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);

	return 0;
}

void testTrafficLight1() {
    while(1) {
        //If it's traffic light 1's turn
        if(baton == 0) {
            trafficLight1Cycle();
            pthread_mutex_lock(&batonMutex);
            baton = 1;
            pthread_mutex_unlock(&batonMutex);
        }
        //If it's traffic light 2's turn
        if(baton == 1) {
            setLightInitialState(GPIO_PATH_44, GPIO_PATH_68, GPIO_PATH_67);
        }
    }
}

void testTrafficLight2() {
    while(1) {
        //If it's traffic light 1's turn
        if(baton == 0) {
            setLightInitialState(GPIO_PATH_26, GPIO_PATH_46, GPIO_PATH_65);
        }
        if(baton == 1) {
            trafficLight2Cycle();
            pthread_mutex_lock(&batonMutex);
            baton = 0;
            pthread_mutex_unlock(&batonMutex);
        }
    }
}

void trafficLight1Cycle() {
    (void) writeLED("/value", GPIO_PATH_67, "0");
    (void) writeLED("/value", GPIO_PATH_44, "1");
    pthread_mutex_lock(&timerMutex);
    start_time = time(NULL);
    pthread_mutex_unlock(&timerMutex);
    sleep(GREEN_LIGHT_TIME);
    (void) writeLED("/value", GPIO_PATH_44, "0");
    (void) writeLED("/value", GPIO_PATH_68, "1");
    sleep(YELLOW_LIGHT_TIME);
    (void) writeLED("/value", GPIO_PATH_68, "0");
    (void) writeLED("/value", GPIO_PATH_67, "1");
}

void trafficLight2Cycle() {
    (void) writeLED("/value", GPIO_PATH_65, "0");
    (void) writeLED("/value", GPIO_PATH_26, "1");
    pthread_mutex_lock(&timerMutex);
    start_time = time(NULL);
    pthread_mutex_unlock(&timerMutex);
    sleep(GREEN_LIGHT_TIME);
    (void) writeLED("/value", GPIO_PATH_26, "0");
    (void) writeLED("/value", GPIO_PATH_46, "1");
    sleep(YELLOW_LIGHT_TIME);
    (void) writeLED("/value", GPIO_PATH_46, "0");
    (void) writeLED("/value", GPIO_PATH_65, "1");
}

//SIGILL: TrafficLight1's signal to be interrupted and set to red
//SIGALRM: TrafficLight1's signal to start cycling

//SIGHUP: TrafficLight2's signal to be interrupted and set to red
//SIGBUS: TrafficLight2's signal to start cycling


//TODO: ***********************************************************************************************************************
//1. Which light begins first? Start the sequence in the main() function by sending a SIGALRM signal to get the ball rolling.
//2. Implement sigwait() within the trafficlight cycling code to make the lights wait to receive a signal to cycle
//3. Figure out how to implement DEBUG MODE
//4. Refactor the code by passing if arguments instead of hardcoding all the ports
//*****************************************************************************************************************************

//Button2 tells trafficLight1 that trafficLight2 needs to start cycling immediately
void trafficLight1InterruptHandler() {
    //Set trafficLight1 to Red
    setLightInitialState(GPIO_PATH_44, GPIO_PATH_68, GPIO_PATH_67);
    signal(SIGILL, trafficLight1InterruptHandler);
    //Tell opposite light to start cycling
    pthread_kill(thread4, SIGBUS);
}

//Button1 tells trafficLight2 that trafficLight1 needs to start cycling immediately
void trafficLight2InterruptHandler() {
    //Set trafficLight2 to Red
    setLightInitialState(GPIO_PATH_26, GPIO_PATH_46, GPIO_PATH_65);
    signal(SIGHUP, trafficLight2InterruptHandler);
    //Tell opposite light to start cycling
    pthread_kill(thread3, SIGALRM);
}

void getButton1PressDuration() {
    //https://www.youtube.com/watch?v=b2_jS3ZMwtM
    //https://forum.beagleboard.org/t/reading-gpio-state-in-beagle-bone-black/1649
    //https://www.dummies.com/article/technology/computers/hardware/beaglebone/setting-beaglebone-gpios-as-inputs-144958/
    //https://learn.adafruit.com/connecting-a-push-button-to-beaglebone-black/wiring
    time_t start_time;
    time_t end_time;
    int pressedFlag = 0;
    int signalSentFlag = 0;
    int gpioValue;
    while(1) {
        gpioValue = readGPIO("/value", GPIO_PATH_66);
        if(gpioValue == 1){
            //first press detected
            if(pressedFlag == 0) {
                start_time = time(NULL);
                pressedFlag = 1;
            }
            else {
                end_time = time(NULL);
                if(((end_time - start_time) >= 5)) {
                    //SEND SIGNAL TO OPPOSITE LIGHT HANDLER TO INTERRUPT AND RESET TO RED
                    if(signalSentFlag == 0) {
                        pthread_kill(thread4, SIGHUP);
                        signalSentFlag = 1;
                    }
                }
            }
        }
        if(gpioValue == 0) {
            //if the button is let go after being pressed
            if(pressedFlag == 1) {
                end_time = time(NULL);
                pressedFlag = 0;
                signalSentFlag = 0;
                printf("Button press time: %ld\n", end_time - start_time);
                fflush( stdout );
            }
        }
    }
}

void getButton2PressDuration() {
    time_t start_time;
    time_t end_time;
    int pressedFlag = 0;
    int signalSentFlag = 0;
    int gpioValue;
    while(1) {
        gpioValue = readGPIO("/value", GPIO_PATH_69);
        if(gpioValue == 1){
            //first press detected
            if(pressedFlag == 0) {
                start_time = time(NULL);
                fflush( stdout );
                pressedFlag = 1;
            }
            else {
                end_time = time(NULL);
                if(((end_time - start_time) >= 5)) {
                    //Send signal only once
                    if(signalSentFlag == 0) {
                        pthread_kill(thread3, SIGILL);
                        signalSentFlag = 1;
                    }
                }
            }
        }
        if(gpioValue == 0) {
            //if the button is let go after being pressed
            if(pressedFlag == 1) {
                end_time = time(NULL);
                pressedFlag = 0;
                signalSentFlag = 0;
                printf("Button press time: %ld\n", end_time - start_time);
                fflush( stdout );
            }
        }
    }
}

static void setLightInitialState(char *greenPort, char *yellowPort, char *redPort) {
    #ifdef DEBUG
    (void) printf("Green off: %s\n", greenPort);
    (void) printf("Yellow off: %s\n", yellowPort);
    (void) printf("Red on: %s\n", redPort);
    #else
    (void) writeLED("/value", greenPort, "0");
    (void) writeLED("/value", yellowPort, "0");
    (void) writeLED("/value", redPort, "1");
    #endif
}

void cycleTrafficLight1() {
    time_t startTime,endTime;
    int colorTimerFlag = 0;
    int sig;
    while(1) {
        sigwait(&trafficLight1Set, &sig);
        if (readGPIO("/value", GPIO_PATH_67)) { //If light is currently Red
            //Turn red off, turn green on
            (void) writeLED("/value", GPIO_PATH_67, "0");
            (void) writeLED("/value", GPIO_PATH_44, "1");
        }
        while(readGPIO("/value", GPIO_PATH_67) == 0) {
            if (readGPIO("/value", GPIO_PATH_44)) { //If light is currently Green
                if (colorTimerFlag == 0) {
                    colorTimerFlag = 1;
                    startTime = time(NULL);
                }
                if (colorTimerFlag == 1) {
                    endTime = time(NULL);
                    if (endTime - startTime >= GREEN_LIGHT_TIME) {
                        //if 10 seconds have elapsed since the light has turned green, turn green light off and yellow light on
                        colorTimerFlag = 0;
                        (void) writeLED("/value", GPIO_PATH_44, "0");
                        (void) writeLED("/value", GPIO_PATH_68, "1");
                    }
                }
            }

            if (readGPIO("/value", GPIO_PATH_68)) { //If light is currently Yellow
                if (colorTimerFlag == 0) { //If this is the first iteration
                    colorTimerFlag = 1;
                    startTime = time(NULL);
                }
                if (colorTimerFlag == 1) {
                    endTime = time(NULL);
                    if (endTime - startTime >= YELLOW_LIGHT_TIME) {
                        //if 10 seconds have elapsed since the light has turned yellow, turn yellow light off and red light on
                        colorTimerFlag = 0;
                        (void) writeLED("/value", GPIO_PATH_68, "0");
                        (void) writeLED("/value", GPIO_PATH_67, "1");
                        //SEND SIGNAL TO OTHER TRAFFIC LIGHT THREAD TO START CYCLING
                        pthread_kill(thread4, SIGBUS);
                    }
                }
            }
        }
    }
}

void cycleTrafficLight2() {
    time_t startTime,endTime;
    int colorTimerFlag = 0;
    int sig;
    while(1) {
        sigwait(&trafficLight2Set, &sig);
        if (readGPIO("/value", GPIO_PATH_65)) { //If light is currently Red
            //Turn red off, turn green on
            (void) writeLED("/value", GPIO_PATH_65, "0");
            (void) writeLED("/value", GPIO_PATH_26, "1");
        }
        while(readGPIO("/value", GPIO_PATH_65) == 0) {
            if (readGPIO("/value", GPIO_PATH_26)) { //If light is currently Green
                if (colorTimerFlag == 0) {
                    colorTimerFlag = 1;
                    startTime = time(NULL);
                }
                if (colorTimerFlag == 1) {
                    endTime = time(NULL);
                    if (endTime - startTime >= GREEN_LIGHT_TIME) {
                        //if 10 seconds have elapsed since the light has turned green, turn green light off and yellow light on
                        colorTimerFlag = 0;
                        (void) writeLED("/value", GPIO_PATH_26, "0");
                        (void) writeLED("/value", GPIO_PATH_46, "1");
                    }
                }
            }

            if (readGPIO("/value", GPIO_PATH_46)) { //If light is currently Yellow
                if (colorTimerFlag == 0) { //If this is the first iteration of red
                    colorTimerFlag = 1;
                    startTime = time(NULL);
                }
                if (colorTimerFlag == 1) {
                    endTime = time(NULL);
                    if (endTime - startTime >= YELLOW_LIGHT_TIME) {
                        //if 10 seconds have elapsed since the light has turned yellow, turn yellow light off and red light on
                        colorTimerFlag = 0;
                        (void) writeLED("/value", GPIO_PATH_46, "0");
                        (void) writeLED("/value", GPIO_PATH_65, "1");
                        //SEND SIGNAL TO OTHER TRAFFIC LIGHT THREAD
                        pthread_kill(thread3, SIGALRM);
                    }
                }
            }
        }
    }
}

static int readGPIO(char *filename, char *port) {
    FILE* fp; //create file pointer
    char fullFileName[100]; //store path and filename
    int val;
    (void) sprintf(fullFileName, "%s%s", port, filename); //write path/name
    fp = fopen(fullFileName, "r"); //open file for writing
    (void) fscanf(fp, "%d", &val);
    fclose(fp);
    return val;
}

static void writeLED(char *filename, char *port, char *value) {
	FILE* fp; //create file pointer
	char fullFileName[100]; //store path and filename
    (void) sprintf(fullFileName, "%s%s", port, filename); //write path/name
	fp = fopen(fullFileName, "w+"); //open file for writing
    (void) fprintf(fp, "%s", value); // send value to the file
    (void) fclose(fp); //close the file using the file pointer
}
