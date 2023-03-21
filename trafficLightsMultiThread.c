#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/utsname.h>
#include <stdint.h>
#include <time.h>

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
#define BUTTON_PRESS_DURATION 5

//Writes specified value to specified GPIO directory
static void writeLED(char *filename, char *port, char *value);

//Reads input to GPIO pin
static uint32_t readGPIO(char *filename, char *port);

static void setLightInitialState(char *greenPort, char *yellowPort, char *redPort);

//Primary light switch logic
void trafficLightCycle(char trafficLight2Ports[3][25]);

//Primary button press detection
void getButtonPressDuration(void *buttonPort);

void trafficLight1Thread(char trafficLight1Ports[3][25]);
void trafficLight2Thread(char trafficLight2Ports[3][25]);

pthread_mutex_t timerMutex;
time_t startTime;
time_t endTime;

//Baton represents the flow of control between traffic lights
pthread_mutex_t batonMutex;
uint32_t baton = 0;

uint32_t main(void) {
    //arrays containing GPIO port definitions, representing each of the two traffic lights and the button inputs
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
    for (uint32_t i = 0; i < 3; i++) {
        (void) writeLED("/direction", trafficLight1Ports[i], "out");
        (void) writeLED("/direction", trafficLight2Ports[i], "out");
    }

    for (uint32_t i = 0; i < 2; i++) {
        (void) writeLED("/direction", buttonPorts[i], "in");
    }
    #endif

    //Initialize GPIO port values
    setLightInitialState(trafficLight1Ports[0], trafficLight1Ports[1], trafficLight1Ports[2]);
    setLightInitialState(trafficLight2Ports[0], trafficLight2Ports[1], trafficLight2Ports[2]);

    (void) pthread_mutex_init(&batonMutex, NULL);
    (void) pthread_mutex_init(&timerMutex, NULL);

    /* Create independent threads each of which will execute function */
    pthread_t thread1, thread2, thread3, thread4;
    #ifdef DEBUG
    //Since the button/interrupt functionality depends on GPIO input, disable it during debug mode
    pthread_create( &thread3, NULL, (void *) trafficLight1Thread, trafficLight1Ports);
    pthread_create( &thread4, NULL, (void *) trafficLight2Thread, trafficLight2Ports);
    #else
    pthread_create( &thread1, NULL, (void*) getButtonPressDuration, (void*) buttonPorts[0]);
    (void) pthread_create( &thread2, NULL, (void*) getButtonPressDuration, (void*) buttonPorts[1]);
    (void) pthread_create( &thread3, NULL, (void *) trafficLight1Thread, trafficLight1Ports);
    (void) pthread_create( &thread4, NULL, (void *) trafficLight2Thread, trafficLight2Ports);
    #endif

    pthread_join(thread3, NULL);
    (void) pthread_join(thread4, NULL);

	return 0;
}

void trafficLight1Thread(char trafficLight1Ports[3][25]) {
    uint32_t initialStateFlag = 0;
    while(1) {
        //If it's traffic light 1's turn
        if(baton == 0) {
            initialStateFlag = 0;
            trafficLightCycle(trafficLight1Ports);
            (void) pthread_mutex_lock(&batonMutex);
            baton = 1;
            (void) pthread_mutex_unlock(&batonMutex);
        }
        //If it's traffic light 2's turn
        if(baton == 1) {
            if(initialStateFlag == 0) {
                setLightInitialState(trafficLight1Ports[0], trafficLight1Ports[1], trafficLight1Ports[2]);
                initialStateFlag = 1;
            }
        }
    }
}

void trafficLight2Thread(char trafficLight2Ports[3][25]) {
    uint32_t initialStateFlag = 0;
    while(1) {
        //If it's traffic light 1's turn
        if(baton == 0) {
            if(initialStateFlag == 0) {
                setLightInitialState(trafficLight2Ports[0], trafficLight2Ports[1], trafficLight2Ports[2]);
                initialStateFlag = 1;
            }
        }
        //If it's traffic light 2's turn
        if(baton == 1) {
            initialStateFlag = 0;
            trafficLightCycle(trafficLight2Ports);
            (void) pthread_mutex_lock(&batonMutex);
            baton = 0;
            (void) pthread_mutex_unlock(&batonMutex);
        }
    }
}

void trafficLightCycle(char trafficLightPorts[3][25]) {
    (void) pthread_mutex_lock(&timerMutex);
    startTime = time(NULL);
    (void) pthread_mutex_unlock(&timerMutex);
    #ifdef DEBUG
    (void) printf("Red off: %s\n", trafficLightPorts[2]);
    (void) printf("Green on: %s\n", trafficLightPorts[0]);
    #else
    (void) writeLED("/value", trafficLightPorts[2], "0");
    (void) writeLED("/value", trafficLightPorts[0], "1");
    #endif

    while(1) {
        (void) pthread_mutex_lock(&timerMutex);
        endTime = time(NULL);
        time_t runTime = endTime - startTime;
        (void) pthread_mutex_unlock(&timerMutex);
        if (runTime >= GREEN_LIGHT_TIME) {
            //if GREEN_LIGHT_TIME seconds have elapsed since the cycle was initiated, turn green light off and yellow light on
            #ifdef DEBUG
            (void) printf("Green off: %s\n", trafficLightPorts[0]);
            (void) printf("Yellow on: %s\n", trafficLightPorts[1]);
            #else
            (void) writeLED("/value", trafficLightPorts[0], "0");
            (void) writeLED("/value", trafficLightPorts[1], "1");
            #endif
            break;
        }
    }

    while(1) {
        endTime = time(NULL);
        (void) pthread_mutex_lock(&timerMutex);
        time_t runTime = endTime - startTime;
        (void) pthread_mutex_unlock(&timerMutex);
        if (runTime >= YELLOW_LIGHT_TIME + GREEN_LIGHT_TIME) {
            //if YELLOW_LIGHT_TIME + GREEN_LIGHT_TIME seconds have elapsed since the cycle was initiated, turn yellow light off and red light on
            #ifdef DEBUG
            (void) printf("Yellow off: %s\n", trafficLightPorts[1]);
            (void) printf("Red on: %s\n", trafficLightPorts[2]);
            #else
            (void) writeLED("/value", trafficLightPorts[1], "0");
            (void) writeLED("/value", trafficLightPorts[2], "1");
            #endif
            break;
        }
    }
}

void getButtonPressDuration(void *buttonPort) {
    time_t buttonStartTime;
    time_t buttonEndTime;
    uint32_t pressedFlag = 0;
    uint32_t signalSentFlag = 0;
    uint32_t gpioValue;
    while(1) {
        gpioValue = readGPIO("/value", (char *) buttonPort);
        if(gpioValue == 1){
            //first press detected
            if(pressedFlag == 0) {
                buttonStartTime = time(NULL);
                pressedFlag = 1;
            }
            else {
                buttonEndTime = time(NULL);
                if(((buttonEndTime - buttonStartTime) >= BUTTON_PRESS_DURATION)) {
                    if(signalSentFlag == 0) {
                        //If the buttonPort corresponds with trafficLight1, and trafficLight1 is red, then allow the interrupt
                        if(strcmp((char*) buttonPort,  GPIO_PATH_66) == 0) {
                            if(readGPIO("/value", GPIO_PATH_67) == 1) {
                                (void) pthread_mutex_lock(&timerMutex);
                                //If the light has been cycling for less time than the green light time(the light is currently green), then allow the interrupt
                                if(endTime - startTime < GREEN_LIGHT_TIME) {
                                    //Manually adjust the start time to trick the main logic loop to think it's time to end the light cycle
                                    startTime = startTime - (GREEN_LIGHT_TIME + startTime - endTime);
                                }
                                (void) pthread_mutex_unlock(&timerMutex);
                                signalSentFlag = 1;
                            }
                        }
                        //If the buttonPort corresponds with trafficLight2, and trafficLight2 is red, then allow the interrupt
                        if(strcmp((char*) buttonPort,  GPIO_PATH_69) == 0) {
                            if(readGPIO("/value", GPIO_PATH_65) == 1) {
                                (void) pthread_mutex_lock(&timerMutex);
                                //If the light has been cycling for less time than the green light time(the light is currently green), then allow the interrupt
                                if(endTime - startTime < GREEN_LIGHT_TIME) {
                                    //Manually adjust the start time to trick the main logic loop to think it's time to end the light cycle
                                    startTime = startTime - (GREEN_LIGHT_TIME + startTime - endTime);
                                }
                                (void) pthread_mutex_unlock(&timerMutex);
                                signalSentFlag = 1;
                            }
                        }
                    }
                }
            }
        }
        if(gpioValue == 0) {
            //if the button is let go after being pressed
            if(pressedFlag == 1) {
                buttonEndTime = time(NULL);
                pressedFlag = 0;
                signalSentFlag = 0;
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

static uint32_t readGPIO(char *filename, char *port) {
    FILE* fp; //create file pointer
    char fullFileName[100]; //store path and filename
    uint32_t val;
    (void) sprintf(fullFileName, "%s%s", port, filename); //write path/name
    fp = fopen(fullFileName, "r"); //open file for writing
    (void) fscanf(fp, "%d", &val);
    (void) fclose(fp);
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