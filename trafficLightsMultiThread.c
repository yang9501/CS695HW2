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

void getButton1PressDuration(void *trafficLight1ThreadId);
void getButton2PressDuration(void *trafficLight2ThreadId);

void testHandler();
void testLoop();
void testSendSignal();

pthread_t thread1, thread2, thread3, thread4;

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
    signal(SIGILL, testHandler);
    /* Create independent threads each of which will execute function */
    pthread_create( &thread1, NULL, (void*) getButton1PressDuration, (void*) &thread4);
    pthread_create( &thread2, NULL, (void*) getButton2PressDuration, (void*) &thread3);
    pthread_create( &thread3, NULL, (void *) cycleTrafficLight1, NULL);
    pthread_create( &thread4, NULL, (void *) cycleTrafficLight2, NULL);
    //pthread_create( &thread4, NULL, (void *) testSendSignal, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
	return 0;
}

void testHandler() {
    sleep(2);
    printf("----------------------------\n");
    sleep(2);
    signal(SIGILL, testHandler);
}

void testSendSignal() {
    sleep(2);
    pthread_kill(thread3, SIGILL);
    sleep(5);
    pthread_kill(thread3, SIGILL);
}

void testLoop() {
    while(1){
        printf("hello\n");
    }
}

void getButton1PressDuration(void *trafficLight1ThreadId) {
    //https://www.youtube.com/watch?v=b2_jS3ZMwtM
    //https://forum.beagleboard.org/t/reading-gpio-state-in-beagle-bone-black/1649
    //https://www.dummies.com/article/technology/computers/hardware/beaglebone/setting-beaglebone-gpios-as-inputs-144958/
    //https://learn.adafruit.com/connecting-a-push-button-to-beaglebone-black/wiring
    time_t start_time;
    time_t end_time;
    int pressedFlag = 0;
    int gpioValue;
    while(1) {
        gpioValue = readGPIO("/value", GPIO_PATH_66);
        //printf("%d", pressedFlag);
        if(gpioValue == 1){
            //first press detected
            if(pressedFlag == 0) {
                //printf("FIRST PRESSED");
                start_time = time(NULL);
                //printf("%ld", start_time);
                fflush( stdout );   //https://stackoverflow.com/questions/16870059/printf-not-printing-to-screen
                pressedFlag = 1;
            }
            else {
                end_time = time(NULL);
                if(((end_time - start_time) >= 5)) {
                    printf("GREATER THAN 5\n");
                    fflush( stdout );
                    //SEND SIGINT SIGNAL
                    pthread_kill(thread3, SIGILL);
                }
            }
        }
        if(gpioValue == 0) {
            //if the button is let go after being pressed
            if(pressedFlag == 1) {
                end_time = time(NULL);
                pressedFlag = 0;
                printf("Button press time: %ld\n", end_time - start_time);
                fflush( stdout );
            }
        }
    }
}

void getButton2PressDuration(void *trafficLight2ThreadId) {
    time_t start_time;
    time_t end_time;
    int pressedFlag = 0;
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
                    printf("GREATER THAN 5\n");
                    fflush( stdout );
                    //SEND SIGINT SIGNAL
                    //pthread_kill((pthread *)trafficLight2ThreadId, SIGINT);
                }
            }
        }
        if(gpioValue == 0) {
            //if the button is let go after being pressed
            if(pressedFlag == 1) {
                end_time = time(NULL);
                pressedFlag = 0;
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
    while(1) {
        //INSERT SIGNAL WAIT HERE
        if (readGPIO("/value", GPIO_PATH_67)) { //If light is currently Red
            //Turn red off, turn green on
            (void) writeLED("/value", GPIO_PATH_67, "0");
            (void) writeLED("/value", GPIO_PATH_44, "1");
        }

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
            if (colorTimerFlag == 0) { //If this is the first iteration of red
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
                    //SEND SIGNAL TO OTHER TRAFFIC LIGHT THREAD
                }
            }
        }
    }
}

void cycleTrafficLight2() {
    time_t startTime,endTime;
    int colorTimerFlag = 0;
    while(1) {
        //INSERT SIGNAL WAIT HERE
        if (readGPIO("/value", GPIO_PATH_65)) { //If light is currently Red
            //Turn red off, turn green on
            (void) writeLED("/value", GPIO_PATH_65, "0");
            (void) writeLED("/value", GPIO_PATH_26, "1");
        }

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
