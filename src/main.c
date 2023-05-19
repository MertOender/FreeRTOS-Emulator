#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "gfx_ball.h"
#include "gfx_draw.h"
#include "gfx_font.h"
#include "gfx_event.h"
#include "gfx_sound.h"
#include "gfx_utils.h"
#include "gfx_FreeRTOS_utils.h"
#include "gfx_print.h"


#ifdef TRACE_FUNCTIONS
#include "tracer.h"
#endif

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

double angle = 0;

#define SquareWidth 90

static TaskHandle_t DemoTask = NULL;
static TaskHandle_t BufferSwap = NULL;

SemaphoreHandle_t DrawSignal = NULL;


typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

//Makes the Circle and Box symbols turn araound the x and y coordinates of mouse
//by increasing the angle 0.5 every time it is called
void SymbolsRotating(short int *x_Circle, short int *y_Circle, short int *x_Box,
                     short int *y_Box, int *mouse_readed_x ,int *mouse_readed_y ){
        angle += 0.05;
        *x_Circle= *(mouse_readed_x) + 140 * cos(angle);
        *y_Circle= *(mouse_readed_y) + 140 * sin(angle);
        *x_Box = *(mouse_readed_x) -  140 * cos(angle)- SquareWidth/2;
        *y_Box= *(mouse_readed_y) - 140 * sin(angle) - SquareWidth/2;
}     

// Counts the number of times A B C D buttons clicked 
// only if the incoming signal from the keyboard turns from 0 to 1
void buttons_count(int *A_counter, int* B_counter , int *C_counter,
                   int *D_counter, int *button_A, int *button_B,int *button_C,int *button_D){   
    
    if(buttons.buttons[KEYCODE(A)] != *button_A){
        if(buttons.buttons[KEYCODE(A)]){
        *A_counter = *A_counter + 1;
        *button_A =1;
        } 
    }
    if(!buttons.buttons[KEYCODE(A)])
    *button_A =0;
if(buttons.buttons[KEYCODE(B)] != *button_B){
        if(buttons.buttons[KEYCODE(B)]){
        *B_counter = *B_counter + 1;
        *button_B =1;
        }      
    }
    if(!buttons.buttons[KEYCODE(B)])
    *button_B =0;
if(buttons.buttons[KEYCODE(C)] != *button_C){
        if(buttons.buttons[KEYCODE(C)]){
        *C_counter = *C_counter + 1;
        *button_C =1;
        }     
    }
    if(!buttons.buttons[KEYCODE(C)])
    *button_C =0;
    if(buttons.buttons[KEYCODE(D)] != *button_D){
        if(buttons.buttons[KEYCODE(D)]){
        *D_counter = *D_counter + 1;
        *button_D =1;
        }
    }
    if(!buttons.buttons[KEYCODE(D)])
    *button_D =0;
    // Resets the counter when there is a left click signal from the mouse
    if(gfxEventGetMouseLeft()){
        *A_counter = 0;
        *B_counter = 0;
        *C_counter = 0;
        *D_counter = 0;
    }
}
//Adds or subtracts 1 pixel from the x Position of the moving text depending on the last collision surface
void TextMoving(int *xPositionofText, int TextSize, int *control_max_width, int *x_position_mouse){
   if(*xPositionofText< SCREEN_WIDTH-TextSize-*x_position_mouse && !*control_max_width){  
    *xPositionofText += 1;
        
   }
   if(*xPositionofText>= SCREEN_WIDTH-TextSize-*x_position_mouse){
            *control_max_width = 1;
        }
    //control max width is 1 when the collision with the right side of the screen happened 
    // and back to 0 if the text touches the left side

    if(*control_max_width){  
    *xPositionofText -= 1;
    if(*xPositionofText + *x_position_mouse<= 0){
            *control_max_width = 0;
        }
   }
        
        
       
}
    

void vSwapBuffers(void *pvParameters)
{
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t frameratePeriod = 20;

    while (1) {
        gfxDrawUpdateScreen();
        gfxEventFetchEvents(FETCH_EVENT_BLOCK);
        xSemaphoreGive(DrawSignal);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(frameratePeriod));
    }
}

void vDemoTask(void *pvParameters)
{
    // structure to store time retrieved from Linux kernel
    static struct timespec the_time;
    static char number_of_loops[100];
    static int number_of_loops_width = 0;
    static char moving_string[100];
    static int moving_string_width = 0;
    static char output_buttons_A[100];
    static char output_buttons_B[100];
    static char output_buttons_C[100];
    static char output_buttons_D[100];
    static char mouse_coordinates_x[100];
    static char mouse_coordinates_y[100];
    static int mouse_coordinates_width = 0;
    static int button_not_A = 0;
   static int button_not_B = 0;
   static int button_not_C = 0;
   static int button_not_D = 0;
    
    static int moving_string_x_position = 0;
    static int text_reached_max_width =0;
    short int  XpositionBox;
    short int YpositionSymbols;
    short int xCircle;
    short int yCircle;
    long int loop_number =0;
    int button_count_A = 0;
    int button_count_B = 0;
    int button_count_C = 0;
    int button_count_D = 0;
    int mouse_x;
    int mouse_y;
    
    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call gfxDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.
    gfxDrawBindThread();

    while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                gfxEventFetchEvents(
                    FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
                xGetButtonInput(); // Update global input

                // `buttons` is a global shared variable and as such needs to be
                // guarded with a mutex, mutex must be obtained before accessing the
                // resource and given back when you're finished. If the mutex is not
                // given back then no other task can access the reseource.
                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
                    if (buttons.buttons[KEYCODE(
                                            Q)]) { // Equiv to SDL_SCANCODE_Q
                        exit(EXIT_SUCCESS);
                    }
                    xSemaphoreGive(buttons.lock);
                }

                gfxDrawClear(White); // Clear screen

                clock_gettime(
                    CLOCK_REALTIME,
                    &the_time); // Get kernel real time

                //called the function that counts the number of times A B C D buttons clicked with debounce logic
                buttons_count(&button_count_A, &button_count_B , &button_count_C , &button_count_D,
                                &button_not_A, &button_not_B,&button_not_C,&button_not_D);
                
                
                //The x and y coordinates of mouse stored in 2 int variables
                mouse_x=gfxEventGetMouseX();
                mouse_y=gfxEventGetMouseY();

                
                
                
                //Number of times that the demo task loop been executed turned into Binary 
                sprintf(number_of_loops,
                        "Number of loops executed : %ld . Press Q to quit",
                        (long int)loop_number);
                // moving text string turned into binary
                sprintf(moving_string, "This text is moving");

                // Number of times that A B C or D buttons clicked turned into Binary
                    sprintf(output_buttons_A,
                        "A: %d", 
                        button_count_A);
                    sprintf(output_buttons_B,
                        "B: %d", 
                        button_count_B);
                    sprintf(output_buttons_C,
                        "C: %d", 
                        button_count_C);
                    sprintf(output_buttons_D,
                        "D: %d", 
                       button_count_D);
                
                // x and y coordinates of mouse turned into binary 
                sprintf(mouse_coordinates_x,"X: %d", mouse_x);
                sprintf(mouse_coordinates_y,"Y: %d", mouse_y);
                       

                // Get the width of the string on the screen so we can center it
                // Returns 0 if width was successfully obtained
                // 
                if(!gfxGetTextSize((char *)number_of_loops,
                                    &number_of_loops_width,
                                    NULL)){
                    gfxDrawText(
                        number_of_loops,
                        mouse_x - (number_of_loops_width/2),
                        mouse_y+(SCREEN_HEIGHT/2) - 50 -
                        DEFAULT_FONT_SIZE / 2,
                        TUMBlue);          }
                    //Drawing number of times A B C and D buttons are clicked depending on x and y coordinates of mouse    
                    gfxDrawText(output_buttons_A, mouse_x-(SCREEN_WIDTH/2)+20,mouse_y- 30, TUMBlue);
                    gfxDrawText(output_buttons_B,mouse_x-(SCREEN_WIDTH/2)+20, mouse_y -15,TUMBlue);
                    gfxDrawText(output_buttons_C,mouse_x-(SCREEN_WIDTH/2)+20,mouse_y,TUMBlue);
                    gfxDrawText(output_buttons_D, mouse_x-(SCREEN_WIDTH/2)+20,mouse_y+15,TUMBlue);
 
                    //Drawing moving String depending on x and y coordinates of mouse 
                    if(!gfxGetTextSize((char *)moving_string,
                                    &moving_string_width,
                                    NULL)){
                    gfxDrawText(
                        moving_string,
                        moving_string_x_position + mouse_x,
                        mouse_y-(SCREEN_HEIGHT/2)+ 10,
                        Black);
                                        }   

                    if(!gfxGetTextSize((char *)mouse_coordinates_x,
                                    &mouse_coordinates_width,
                                    NULL)){
                         gfxDrawText(
                            mouse_coordinates_x,
                            mouse_x+(SCREEN_WIDTH/2)-mouse_coordinates_width,
                            mouse_y-40,
                            TUMBlue);

                            gfxDrawText(
                            mouse_coordinates_y,
                            mouse_x+(SCREEN_WIDTH/2)-mouse_coordinates_width,
                            mouse_y+20,
                            TUMBlue);
                    }
                // Calling the function that makes Circle and Box rotate 
                SymbolsRotating(&xCircle, &yCircle,&XpositionBox, &YpositionSymbols, &mouse_x , &mouse_y);
                //Calling the function that changes x coordinate of the upper text 
                TextMoving(&moving_string_x_position, moving_string_width, &text_reached_max_width , &mouse_x);

                //implementing the 3 edges of the triangle depending on x and y coordinates of the mouse and Drawing it
                coord_t TriangleCoordinates[3] = {{(mouse_x), (mouse_y-50)}, {mouse_x-50, mouse_y+50},{ mouse_x+50, mouse_y+50}} ;
                gfxDrawTriangle(TriangleCoordinates, Black);
                //Drawing the box
                gfxDrawBox(XpositionBox,YpositionSymbols, SquareWidth, SquareWidth ,Black);
                //Drawing the circle
                gfxDrawCircle(xCircle, yCircle, 50, Blue);
                // counts the number of loops executed up at the end of every loop
                loop_number++;


                 
                gfxDrawUpdateScreen(); // Refresh the screen to draw string
               
            }
    }
}

int main(int argc, char *argv[])
{
    char *bin_folder_path = gfxUtilGetBinFolderPath(argv[0]);

    prints("Initializing: ");

    //  Note PRINT_ERROR is not thread safe and is only used before the
    //  scheduler is started. There are thread safe print functions in
    //  gfx_Print.h, `prints` and `fprints` that work exactly the same as
    //  `printf` and `fprintf`. So you can read the documentation on these
    //  functions to understand the functionality.

    if (gfxDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to intialize drawing");
        goto err_init_drawing;
    }
    else {
        prints("drawing");
    }

    if (gfxEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }
    else {
        prints(", events");
    }

    if (gfxSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }
    else {
        prints(", and audio\n");

        buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
        if (!buttons.lock) {
            PRINT_ERROR("Failed to create buttons lock");
            goto err_buttons_lock;
        }
    }

    if (gfxSafePrintInit()) {
        PRINT_ERROR("Failed to init safe print");
        goto err_init_safe_print;
    }

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
        goto err_draw_signal;
    }

    if (xTaskCreate(vDemoTask, "DemoTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &DemoTask) != pdPASS) {
        goto err_demotask;
    }

    if (xTaskCreate(vSwapBuffers, "BufferSwapTask",
                    mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
                    &BufferSwap) != pdPASS) {
        PRINT_TASK_ERROR("BufferSwapTask");
        goto err_bufferswap;
    }

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_bufferswap:
    vTaskDelete(DemoTask);
err_demotask:
    vSemaphoreDelete(DrawSignal);
err_draw_signal:
    gfxSafePrintExit();
err_init_safe_print:
    vSemaphoreDelete(buttons.lock);
err_buttons_lock:
    gfxSoundExit();
err_init_audio:
    gfxEventExit();
err_init_events:
    gfxDrawExit();
err_init_drawing:
    return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
