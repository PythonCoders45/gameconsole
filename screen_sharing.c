#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX_HDMI_OUTPUTS 2
#define BASE_FB_WIDTH 1280
#define BASE_FB_HEIGHT 720

typedef struct {
    int width;
    int height;
    uint32_t* framebuffer;
    bool connected;
    bool was_connected;
} HDMIOutput;

// Base framebuffer (OS framebuffer)
uint32_t base_framebuffer[BASE_FB_WIDTH * BASE_FB_HEIGHT];

// ---- Simple nearest-neighbor scaling ----
void scale_framebuffer(uint32_t* src, int src_w, int src_h,
                       uint32_t* dst, int dst_w, int dst_h) {
    for(int y=0; y<dst_h; y++) {
        int src_y = y * src_h / dst_h;
        for(int x=0; x<dst_w; x++) {
            int src_x = x * src_w / dst_w;
            dst[y*dst_w + x] = src[src_y*src_w + src_x];
        }
    }
}

// ---- Stub: detect HDMI outputs and get resolution dynamically ----
void detect_hdmi_outputs(HDMIOutput outputs[], int* count) {
    *count = 2; // Example: two HDMI outputs
    for(int i=0; i<*count; i++) {
        if(i == 0) { outputs[i].width = 1280; outputs[i].height = 720; }
        else       { outputs[i].width = 1920; outputs[i].height = 1080; }
        outputs[i].framebuffer = (uint32_t*)malloc(outputs[i].width * outputs[i].height * sizeof(uint32_t));
        outputs[i].connected = false;
        outputs[i].was_connected = false;
    }
}

// ---- Stub: check if HDMI cable is connected ----
bool check_hdmi_connected(int port) {
    // Replace with actual hardware detection
    return true; 
}

// ---- HDMI send function (replace with your driver) ----
void hdmi_send_frame(HDMIOutput* output) {
    // Send output->framebuffer to actual HDMI hardware
}

// ---- Render OS UI into base framebuffer ----
void render_os_ui(uint32_t* framebuffer, int width, int height) {
    static uint32_t color = 0xFF00FF00;
    for(int i=0; i<width*height; i++) framebuffer[i] = color;
    color += 0x00010101;
}

// ---- Overlay a message on the framebuffer ----
void overlay_message(uint32_t* framebuffer, int width, int height, const char* msg) {
    // Simple stub: fill top-left rectangle if HDMI event occurs
    int rect_w = 400;
    int rect_h = 50;
    uint32_t overlay_color = 0xFFFF0000; // Red rectangle
    for(int y=0; y<rect_h && y<height; y++) {
        for(int x=0; x<rect_w && x<width; x++) {
            framebuffer[y*width + x] = overlay_color;
        }
    }
    // In real OS, you'd render the text `msg` here
}

// ---- Sleep for ~16ms ----
void sleep_for_16ms() { usleep(16666); }

// ---- Main loop ----
int main() {
    HDMIOutput hdmi_outputs[MAX_HDMI_OUTPUTS];
    int hdmi_count = 0;

    detect_hdmi_outputs(hdmi_outputs, &hdmi_count);
    printf("[+] Detected %d HDMI outputs\n", hdmi_count);

    while(1) {
        render_os_ui(base_framebuffer, BASE_FB_WIDTH, BASE_FB_HEIGHT);

        for(int i=0; i<hdmi_count; i++) {
            hdmi_outputs[i].connected = check_hdmi_connected(i);

            if(hdmi_outputs[i].connected && !hdmi_outputs[i].was_connected) {
                printf("[+] HDMI port %d plugged in (Resolution: %dx%d)\n", i, hdmi_outputs[i].width, hdmi_outputs[i].height);
                overlay_message(base_framebuffer, BASE_FB_WIDTH, BASE_FB_HEIGHT, "HDMI Connected");
            } else if(!hdmi_outputs[i].connected && hdmi_outputs[i].was_connected) {
                printf("[-] HDMI port %d unplugged\n", i);
                overlay_message(base_framebuffer, BASE_FB_WIDTH, BASE_FB_HEIGHT, "HDMI Disconnected");
            }

            hdmi_outputs[i].was_connected = hdmi_outputs[i].connected;

            if(hdmi_outputs[i].connected) {
                scale_framebuffer(base_framebuffer, BASE_FB_WIDTH, BASE_FB_HEIGHT,
                                  hdmi_outputs[i].framebuffer,
                                  hdmi_outputs[i].width, hdmi_outputs[i].height);
                hdmi_send_frame(&hdmi_outputs[i]);
            }
        }

        sleep_for_16ms(); // ~60 FPS
    }

    for(int i=0; i<hdmi_count; i++) free(hdmi_outputs[i].framebuffer);
    return 0;
}
