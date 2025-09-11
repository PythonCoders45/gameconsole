#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_DEVICES 4
#define BASE_FB_WIDTH 1280
#define BASE_FB_HEIGHT 720

typedef struct {
    int width, height;
    uint32_t* framebuffer;
    bool connected;
    bool combined;       // part of virtual large screen
    int combined_index;  // horizontal order
} Device;

// ---- Initialize devices ----
void init_devices(Device devices[], int* count) {
    *count = 3; // example: 3 devices
    for(int i=0;i<*count;i++){
        devices[i].width = BASE_FB_WIDTH;
        devices[i].height = BASE_FB_HEIGHT;
        devices[i].framebuffer = (uint32_t*)malloc(devices[i].width*devices[i].height*sizeof(uint32_t));
        devices[i].connected = true;
        devices[i].combined = true;     // mark for combining
        devices[i].combined_index = i;  // default horizontal order
    }
}

// ---- Fill device framebuffer with demo color ----
void render_device(Device* dev, uint32_t color) {
    for(int i=0;i<dev->width*dev->height;i++)
        dev->framebuffer[i] = color;
}

// ---- Combine devices into one large framebuffer ----
void combine_devices(Device devices[], int count, uint32_t* combined_fb, int combined_width, int combined_height) {
    // clear combined framebuffer
    for(int i=0;i<combined_width*combined_height;i++)
        combined_fb[i] = 0xFF101010;

    int offset_x = 0;
    for(int i=0;i<count;i++){
        if(devices[i].combined){
            for(int y=0;y<devices[i].height && y<combined_height;y++){
                for(int x=0;x<devices[i].width && x+offset_x<combined_width;x++){
                    combined_fb[y*combined_width + x + offset_x] = devices[i].framebuffer[y*devices[i].width + x];
                }
            }
            offset_x += devices[i].width; // next device to the right
        }
    }
}

// ---- Output combined framebuffer (stub for HDMI) ----
void send_combined_frame(uint32_t* fb, int width, int height){
    // Replace with actual HDMI/framebuffer output
    printf("[*] Sent combined framebuffer of size %dx%d\n", width, height);
}

// ---- Main loop ----
int main() {
    Device devices[MAX_DEVICES];
    int device_count = 0;
    init_devices(devices, &device_count);

    int combined_width = BASE_FB_WIDTH * device_count;
    int combined_height = BASE_FB_HEIGHT;
    uint32_t* combined_fb = (uint32_t*)malloc(combined_width*combined_height*sizeof(uint32_t));

    while(1){
        // Render each device with unique demo colors
        for(int i=0;i<device_count;i++){
            uint32_t color = 0xFF000000 | ((i+1)*60 <<16) | ((i+1)*80<<8);
            render_device(&devices[i], color);
        }

        // Combine into virtual large screen
        combine_devices(devices, device_count, combined_fb, combined_width, combined_height);

        // Send combined framebuffer to output
        send_combined_frame(combined_fb, combined_width, combined_height);

        usleep(16666); // ~60 FPS
    }

    for(int i=0;i<device_count;i++) free(devices[i].framebuffer);
    free(combined_fb);
    return 0;
}
