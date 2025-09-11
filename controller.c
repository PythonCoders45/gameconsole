#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define MAX_DEVICES 4
#define MAX_CONTROLLERS 8
#define BASE_FB_WIDTH 1280
#define BASE_FB_HEIGHT 720

typedef enum { CONTROLLER_STANDARD, CONTROLLER_JOYCON_LEFT, CONTROLLER_JOYCON_RIGHT } ControllerType;

typedef struct {
    float ax, ay, az; // accelerometer
    float gx, gy, gz; // gyroscope
} MotionSensor;

typedef struct {
    bool connected;
    ControllerType type;
    int device_index; // assigned device
    int x, y;         // cursor
    int buttons;
    float pitch, roll, yaw; // computed
    MotionSensor sensor;
} WirelessController;

typedef struct {
    int width, height;
    uint32_t* framebuffer;
    bool connected;
    bool combined;
    int combined_index;
    bool snap_detected;
} Device;

typedef struct {
    Device devices[MAX_DEVICES];
    int count;
} DeviceManager;

typedef struct {
    WirelessController controllers[MAX_CONTROLLERS];
    int count;
} ControllerManager;

// ---------------- Initialization ----------------
void init_device_manager(DeviceManager* dm){ dm->count=0; }
void init_controller_manager(ControllerManager* cm){ cm->count=0; }

// ---------------- Device Management ----------------
int plug_device(DeviceManager* dm){
    if(dm->count>=MAX_DEVICES) return -1;
    int idx = dm->count;
    dm->devices[idx].width = BASE_FB_WIDTH;
    dm->devices[idx].height = BASE_FB_HEIGHT;
    dm->devices[idx].framebuffer = (uint32_t*)malloc(BASE_FB_WIDTH*BASE_FB_HEIGHT*sizeof(uint32_t));
    dm->devices[idx].connected = true;
    dm->devices[idx].combined = true;
    dm->devices[idx].combined_index = idx;
    dm->devices[idx].snap_detected = false;
    dm->count++;
    printf("[*] Device %d plugged\n", idx);
    return idx;
}

// ---------------- Controller Management ----------------
int plug_controller(ControllerManager* cm, ControllerType type, int device_index){
    if(cm->count>=MAX_CONTROLLERS) return -1;
    int idx=cm->count;
    cm->controllers[idx].connected=true;
    cm->controllers[idx].type=type;
    cm->controllers[idx].device_index=device_index;
    cm->controllers[idx].x=BASE_FB_WIDTH/2;
    cm->controllers[idx].y=BASE_FB_HEIGHT/2;
    cm->controllers[idx].buttons=0;
    cm->controllers[idx].pitch=0.0f;
    cm->controllers[idx].roll=0.0f;
    cm->controllers[idx].yaw=0.0f;
    cm->count++;
    printf("[*] Controller %d plugged into device %d\n", idx, device_index);
    return idx;
}

// ---------------- Motion Detection ----------------
void read_sensor(WirelessController* ctrl){
    // TODO: Replace with real I2C/SPI sensor read
    ctrl->sensor.ax = sin((float)rand()/RAND_MAX * 3.14f);
    ctrl->sensor.ay = cos((float)rand()/RAND_MAX * 3.14f);
    ctrl->sensor.az = 1.0f;
    ctrl->sensor.gx = sin((float)rand()/RAND_MAX * 3.14f);
    ctrl->sensor.gy = cos((float)rand()/RAND_MAX * 3.14f);
    ctrl->sensor.gz = sin((float)rand()/RAND_MAX * 3.14f)*0.5f;
}

void update_motion(WirelessController* ctrl){
    ctrl->pitch = atan2(ctrl->sensor.ay, ctrl->sensor.az)*180.0f/M_PI;
    ctrl->roll  = atan2(-ctrl->sensor.ax, ctrl->sensor.az)*180.0f/M_PI;
    ctrl->yaw   = ctrl->sensor.gz; // simple
}

void apply_motion(WirelessController* ctrl){
    ctrl->x += (int)(ctrl->roll*5);
    ctrl->y += (int)(ctrl->pitch*5);
    if(ctrl->x<0) ctrl->x=0;
    if(ctrl->y<0) ctrl->y=0;
    if(ctrl->x>=BASE_FB_WIDTH) ctrl->x=BASE_FB_WIDTH-1;
    if(ctrl->y>=BASE_FB_HEIGHT) ctrl->y=BASE_FB_HEIGHT-1;
}

// ---------------- Rendering ----------------
void render_device(Device* dev, WirelessController controllers[], int controller_count){
    uint32_t color = 0xFF000000 | ((dev->combined_index+1)*60<<16) | ((dev->combined_index+1)*80<<8);
    for(int i=0;i<dev->width*dev->height;i++) dev->framebuffer[i]=color;

    for(int i=0;i<controller_count;i++){
        if(controllers[i].connected && controllers[i].device_index==dev->combined_index){
            int cx=controllers[i].x;
            int cy=controllers[i].y;
            // motion moves cursor
            cx += (int)(controllers[i].roll*10);
            cy += (int)(controllers[i].pitch*10);
            if(cx>=0 && cx<dev->width && cy>=0 && cy<dev->height){
                uint32_t ccolor=0xFFFFFFFF;
                if(controllers[i].type==CONTROLLER_JOYCON_LEFT) ccolor=0xFFFF0000;
                if(controllers[i].type==CONTROLLER_JOYCON_RIGHT) ccolor=0xFF0000FF;
                dev->framebuffer[cy*dev->width+cx]=ccolor;
            }
        }
    }
}

// ---------------- Combine Screens ----------------
void combine_devices(DeviceManager* dm, uint32_t* combined_fb, int* combined_width, int* combined_height){
    int order[MAX_DEVICES]; int order_count=0;
    for(int i=0;i<dm->count;i++) if(dm->devices[i].combined && dm->devices[i].snap_detected) order[order_count++]=i;
    for(int i=0;i<dm->count;i++) if(dm->devices[i].combined && !dm->devices[i].snap_detected) order[order_count++]=i;

    int total_width=0, max_height=0;
    for(int i=0;i<order_count;i++){
        Device* d=&dm->devices[order[i]];
        total_width+=d->width;
        if(d->height>max_height) max_height=d->height;
    }
    *combined_width=total_width; *combined_height=max_height;
    for(int i=0;i<total_width*max_height;i++) combined_fb[i]=0xFF101010;

    int offset_x=0;
    for(int i=0;i<order_count;i++){
        Device* d=&dm->devices[order[i]];
        for(int y=0;y<d->height;y++){
            for(int x=0;x<d->width;x++){
                combined_fb[y*total_width+x+offset_x]=d->framebuffer[y*d->width+x];
            }
        }
        offset_x+=d->width;
        d->combined_index=i;
    }
}

// ---------------- Update Controllers ----------------
void update_controllers(ControllerManager* cm){
    for(int i=0;i<cm->count;i++){
        if(cm->controllers[i].connected){
            read_sensor(&cm->controllers[i]);
            update_motion(&cm->controllers[i]);
            apply_motion(&cm->controllers[i]);
        }
    }
}

// ---------------- Detect Snaps ----------------
void detect_snap(DeviceManager* dm, int frame){
    if(frame==100) dm->devices[1].snap_detected=true;
    if(frame==300) dm->devices[1].snap_detected=false;
}

// ---------------- Send Framebuffer ----------------
void send_combined_frame(uint32_t* fb,int width,int height){
    printf("[*] Sent combined framebuffer %dx%d\n", width,height);
}

// ---------------- Main ----------------
int main(){
    srand(time(NULL));
    DeviceManager dm; ControllerManager cm;
    init_device_manager(&dm);
    init_controller_manager(&cm);

    int d0=plug_device(&dm);
    int d1=plug_device(&dm);

    plug_controller(&cm,CONTROLLER_JOYCON_LEFT,d0);
    plug_controller(&cm,CONTROLLER_JOYCON_RIGHT,d0);
    plug_controller(&cm,CONTROLLER_STANDARD,d1);

    uint32_t* combined_fb=(uint32_t*)malloc(MAX_DEVICES*BASE_FB_WIDTH*BASE_FB_HEIGHT*sizeof(uint32_t));
    int combined_width=0, combined_height=0;

    int frame=0;
    while(1){
        frame++;
        update_controllers(&cm);
        detect_snap(&dm,frame);

        for(int i=0;i<dm.count;i++)
            render_device(&dm.devices[i],cm.controllers,cm.count);

        combine_devices(&dm,combined_fb,&combined_width,&combined_height);
        send_combined_frame(combined_fb,combined_width,combined_height);

        usleep(16666); // 60 FPS
    }

    for(int i=0;i<dm.count;i++) free(dm.devices[i].framebuffer);
    free(combined_fb);
    return 0;
}
