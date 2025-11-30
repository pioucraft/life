#include <X11/Xlib.h>  // Core X11 library: Display, Window, GC, events, drawing functions
#include <stdlib.h>    // For exit() on errors
#include <stdio.h>     // For fprintf(stderr) error printing
#include <unistd.h>    // For usleep() to throttle FPS (microseconds delay)
#include <math.h>

// Constants for window size and timing
#define WIDTH 1000      // Window width in pixels
#define HEIGHT 1000     // Window height in pixels
#define FPS_DELAY 33333  // Delay in microseconds: ~30 FPS (1000000 / 30 ≈ 33333)
#define DEBUG 0

#define NUM_PARTICLES 600
#define COEFFICIENT 5 * 1e-3
#define SQUARED_RADIUS_MIN 100

typedef struct particle {
    int type;    

    float x;
    float y;
} particle;

particle particles[NUM_PARTICLES];

// Main function: Entry point of the program
int main() {
    // initial position for particles
    srand(42);
    for(int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = rand() % (WIDTH + 1);
        particles[i].y = rand() % (HEIGHT + 1);
        particles[i].type = i % 3;
    }


    // Step 1: Connect to the X11 display server
    // XOpenDisplay(NULL) uses the default display (e.g., :0 on local machine)
    // Returns a Display pointer; NULL on failure (e.g., no X server running)
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        // Error handling: Print to stderr and exit with code 1 (failure)
        fprintf(stderr, "Cannot open display\n");  // stderr for errors, not stdout
        return 1;  // Non-zero exit indicates error
    }

    // Step 2: Get screen information
    // DefaultScreen returns the screen number (usually 0 for single-monitor setups)
    int screen = DefaultScreen(display);  // Current screen index

    // Step 3: Create the window
    // XCreateSimpleWindow: Makes a basic window (no borders beyond the specified)
    // Args: display, parent window (RootWindow for top-level), x/y position (0,0),
    //       width/height, border width (1 pixel), border color (black), background (white)
    // Note: Background is white initially, but we paint black each frame
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        0, 0, WIDTH, HEIGHT, 1,
                                        BlackPixel(display, screen),  // Border: black
                                        WhitePixel(display, screen));  // Background: white (overwritten)

    // Step 4: Select events to listen for
    // ExposureMask: Fires on Expose event (window needs redraw, e.g., uncovered by another window)
    // KeyPressMask: Fires on keyboard input (we check for 'q')
    // StructureNotifyMask: Includes DestroyNotify (window closed via WM)
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);

    // Step 5: Map (show) the window on screen
    // This makes it visible; without it, the window exists but is hidden
    XMapWindow(display, window);

    // Step 6: Create Graphics Context (GC) for drawing
    // GC holds attributes like foreground color, line style; XCreateGC initializes defaults
    GC gc = XCreateGC(display, window, 0, NULL);  // 0: No initial values to set, NULL for defaults

    // Step 7: Define and allocate white color for the pixel
    XColor color1;
    color1.red = 0xFFFF;
    color1.green = 0;
    color1.blue = 0;
    XAllocColor(display, DefaultColormap(display, screen), &color1);

    XColor color2;
    color2.red = 0;
    color2.green = 0xFFFF;
    color2.blue = 0;
    XAllocColor(display, DefaultColormap(display, screen), &color2);

    XColor color3;
    color3.red = 0;
    color3.green = 0;
    color3.blue = 0xFFFF;
    XAllocColor(display, DefaultColormap(display, screen), &color3);

    XSetForeground(display, gc, color1.pixel);

    // Step 8: Event handling and animation loop variables
    XEvent event;          // Struct to hold incoming events
    int running = 1;       // Flag to control the main loop (1=true, 0=false)

    // Main loop: Handles events and updates animation while running
    while (running) {
        // Check for pending events without blocking (non-busy wait)
        // XPending returns count of events in queue; loop processes all before animating
        while (XPending(display)) {
            // XNextEvent: Blocks until next event, then fills 'event' struct
            // Since we check XPending, this is non-blocking if events exist
            XNextEvent(display, &event);

            // Handle specific event types
            if (event.type == Expose) {
                // Expose event: Window needs redraw (e.g., resized or uncovered)
                // Here, we just continue—the animation loop will redraw anyway
                // For efficiency in real apps, redraw only on Expose without full animation step
            } else if (event.type == KeyPress) {
                // KeyPress: User pressed a key
                // XLookupKeysym: Gets the keysym (symbol) from the event; 0 for first state
                char key = XLookupKeysym(&event.xkey, 0);
                if (key == 'q') {
                    // Quit if 'q' pressed (case-sensitive; add 'Q' if needed)
                    running = 0;
                }
            } else if (event.type == DestroyNotify) {
                // DestroyNotify: Window manager requested close (e.g., X button clicked)
                running = 0;
            }
            // Other events ignored (e.g., MotionNotify for mouse)
        }

        // Animation step 1: Clear the window to black background
        // Temporarily set GC foreground to black pixel
        XSetForeground(display, gc, BlackPixel(display, screen));
        // XFillRectangle: Fills a rectangle (x=0,y=0,w=WIDTH,h=HEIGHT) with current foreground
        // This erases previous frame for smooth animation
        XFillRectangle(display, window, gc, 0, 0, WIDTH, HEIGHT);


        for(int i = 0; i < NUM_PARTICLES; i++) {
            if(particles[i].type == 0) {
                XSetForeground(display, gc, color1.pixel);
            }
            else if(particles[i].type == 1) {
                XSetForeground(display, gc, color2.pixel);
            }
            else if (particles[i].type == 2) {
                XSetForeground(display, gc, color3.pixel);
            }
            XFillRectangle(display, window, gc, particles[i].x - 1, particles[i].y - 1, 3, 3);
        }
        
        for(int i = 0; i < NUM_PARTICLES; i++) {
            float new_x = 0;
            float new_y = 0;
            
            for(int j = 0; j < NUM_PARTICLES; j++) {
                if(j == i) continue;

                float x_pos_n = particles[i].x - particles[j].x;
                float x_pos_jb = particles[i].x - particles[j].x + WIDTH;
                float x_pos_ib = particles[i].x - particles[j].x - WIDTH;
                float x_pos = x_pos_n * x_pos_n > x_pos_jb * x_pos_jb ? (x_pos_jb * x_pos_jb > x_pos_ib * x_pos_ib ? x_pos_ib : x_pos_jb) : (x_pos_n * x_pos_n > x_pos_ib * x_pos_ib ? x_pos_ib : x_pos_n);
                float x_squared = x_pos * x_pos;

                float y_pos_n = particles[i].y - particles[j].y;
                float y_pos_jb = particles[i].y - particles[j].y + WIDTH;
                float y_pos_ib = particles[i].y - particles[j].y - WIDTH;
                float y_pos = y_pos_n * y_pos_n > y_pos_jb * y_pos_jb ? (y_pos_jb * y_pos_jb > y_pos_ib * y_pos_ib ? y_pos_ib : y_pos_jb) : (y_pos_n * y_pos_n > y_pos_ib * y_pos_ib ? y_pos_ib : y_pos_n);
                float y_squared = y_pos * y_pos;

                float r_squared = x_squared + y_squared;

                float speed = COEFFICIENT / r_squared;

                float coefficient = -1e4;

                if(particles[i].type == 0 && particles[j].type == 0) {
                    coefficient *= -1.0;
                }
                else if (particles[i].type == particles[j].type) {
                    coefficient *= 1.0;
                }

                else if(particles[i].type == 0 && particles[j].type == 1) {
                    coefficient *= 1.0;
                }
                else if(particles[i].type == 1 && particles[j].type == 0) {
                    coefficient *= -1.0;
                }

                else if(particles[i].type == 0 && particles[j].type == 2) {
                    coefficient *= -1.0;
                }
                else if(particles[i].type == 2 && particles[j].type == 0) {
                    coefficient *= 1.0;
                }

                else if(particles[i].type == 1 && particles[j].type == 2) {
                    coefficient *= 1.0;
                }
                else if(particles[i].type == 2 && particles[j].type == 1) {
                    coefficient *= -1.0;
                }
                else coefficient = 0;

                if(coefficient < 0.0 && r_squared < SQUARED_RADIUS_MIN) continue;

                new_x += coefficient * speed * x_pos / sqrt(r_squared);
                new_y += coefficient * speed * y_pos / sqrt(r_squared);
            }
            particles[i].x += new_x;
            particles[i].y += new_y;
            
            if(particles[i].x > WIDTH) particles[i].x -= WIDTH; 
            if(particles[i].x < 0) particles[i].x += WIDTH; 
            if(particles[i].y > HEIGHT) particles[i].y -= WIDTH; 
            if(particles[i].y < 0) particles[i].y += HEIGHT; 

            if(DEBUG) printf("New speed for particle %d... x : %f and y : %f... Pos x : %f, pos y : %f\n", i, new_x, new_y, particles[i].x, particles[i].y);
        }

        // Step 5: Flush changes to display
        // XFlush: Sends all pending drawing requests to X server immediately
        // Without this, changes might buffer and delay visibility
        XFlush(display);

        // Step 6: Throttle to target FPS
        // usleep: Pauses for microseconds (non-blocking sleep)
        // 33333 μs ≈ 33ms per frame = 30 FPS; adjust for your hardware/feel
        usleep(FPS_DELAY);
    }

    // Cleanup: Free resources to avoid leaks
    // XFreeGC: Releases the graphics context
    XFreeGC(display, gc);
    // XDestroyWindow: Destroys the window (WM may handle, but good practice)
    XDestroyWindow(display, window);
    // XCloseDisplay: Closes connection to X server, frees display resources
    XCloseDisplay(display);

    // Exit successfully
    return 0;
}

