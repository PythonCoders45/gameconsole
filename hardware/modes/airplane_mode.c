// airplane_mode_full.c
// Full Airplane Mode with hardware calls + distributed sync

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdint.h>

#define PORT 7075
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_PEERS 10
#define CONTROLLER_COUNT 4
#define CONTROLLER_BASE 0x40000000  // Example memory-mapped base

// ------------------------------
// CONFIG: Add other device IPs here
// ------------------------------
const char* PEERS[MAX_PEERS] = { "192.168.1.101", "192.168.1.102" };
int PEER_COUNT = 2;

// ------------------------------
// Airplane Mode State
// ------------------------------
bool airplane_mode = false;
bool controllers_connected = true;
bool wifi_enabled = true;
bool bluetooth_enabled = true;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// ------------------------------
// Controllers (memory-mapped)
// ------------------------------
volatile uint8_t* controllers = (uint8_t*)CONTROLLER_BASE;

void disconnect_controllers() {
    for(int i=0;i<CONTROLLER_COUNT;i++)
        controllers[i] = 0;  // 0 = disconnected
    controllers_connected = false;
}

void reconnect_controllers() {
    for(int i=0;i<CONTROLLER_COUNT;i++)
        controllers[i] = 1;  // 1 = connected
    controllers_connected = true;
}

// ------------------------------
// WiFi / Bluetooth (driver calls)
// ------------------------------
// Replace these with your OS driver functions
void disable_wifi() { wifi_enabled=false; /* wifi_disable(); */ }
void enable_wifi() { wifi_enabled=true; /* wifi_enable(); */ }
void disable_bluetooth() { bluetooth_enabled=false; /* bt_disable(); */ }
void enable_bluetooth() { bluetooth_enabled=true; /* bt_enable(); */ }

// ------------------------------
// Airplane Mode Logic
// ------------------------------
void enable_airplane_mode() {
    pthread_mutex_lock(&lock);
    airplane_mode = true;
    disable_wifi();
    disable_bluetooth();
    disconnect_controllers();
    printf("✈️ Airplane Mode ENABLED\n");
    pthread_mutex_unlock(&lock);
}

void disable_airplane_mode() {
    pthread_mutex_lock(&lock);
    airplane_mode = false;
    enable_wifi();
    enable_bluetooth();
    reconnect_controllers();
    printf("📶 Airplane Mode DISABLED\n");
    pthread_mutex_unlock(&lock);
}

void toggle_airplane_mode() {
    pthread_mutex_lock(&lock);
    if(airplane_mode) disable_airplane_mode();
    else enable_airplane_mode();
    pthread_mutex_unlock(&lock);
}

void print_status() {
    pthread_mutex_lock(&lock);
    printf("Current Status:\n");
    printf("  Airplane Mode: %s\n", airplane_mode?"ON":"OFF");
    printf("  WiFi: %s\n", wifi_enabled?"ON":"OFF");
    printf("  Bluetooth: %s\n", bluetooth_enabled?"ON":"OFF");
    printf("  Controllers: %s\n", controllers_connected?"CONNECTED":"DISCONNECTED");
    pthread_mutex_unlock(&lock);
}

// ------------------------------
// Networking
// ------------------------------
int clients[MAX_CLIENTS];
int client_count = 0;

void broadcast_state() {
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&lock);
    snprintf(buffer, sizeof(buffer),
        "{\"Airplane Mode\":\"%s\",\"WiFi\":\"%s\",\"Bluetooth\":\"%s\",\"Controllers\":\"%s\"}\n",
        airplane_mode?"ON":"OFF",
        wifi_enabled?"ON":"OFF",
        bluetooth_enabled?"ON":"OFF",
        controllers_connected?"CONNECTED":"DISCONNECTED");
    pthread_mutex_unlock(&lock);

    for(int i=0;i<client_count;i++)
        send(clients[i], buffer, strlen(buffer), 0);
}

void* client_handler(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    while(1){
        int bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
        if(bytes <= 0) break;
        buffer[bytes]='\0';
        pthread_mutex_lock(&lock);
        if(strstr(buffer,"\"Airplane Mode\":\"ON\"")) enable_airplane_mode();
        else if(strstr(buffer,"\"Airplane Mode\":\"OFF\"")) disable_airplane_mode();
        pthread_mutex_unlock(&lock);
    }
    close(sock);
    pthread_mutex_lock(&lock);
    for(int i=0;i<client_count;i++){
        if(clients[i]==sock){
            for(int j=i;j<client_count-1;j++) clients[j]=clients[j+1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* server_thread(void* arg) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd==0){ perror("socket failed"); exit(EXIT_FAILURE); }

    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt));

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address))<0){ perror("bind"); exit(EXIT_FAILURE);}
    if(listen(server_fd,3)<0){ perror("listen"); exit(EXIT_FAILURE);}
    printf("[SERVER] Listening on port %d\n",PORT);

    while(1){
        new_socket = accept(server_fd,(struct sockaddr*)&address,(socklen_t*)&addrlen);
        if(new_socket<0){ perror("accept"); continue;}
        pthread_mutex_lock(&lock);
        if(client_count<MAX_CLIENTS) clients[client_count++]=new_socket;
        pthread_mutex_unlock(&lock);
        pthread_t tid;
        pthread_create(&tid,NULL,client_handler,&new_socket);
    }
    return NULL;
}

void* broadcast_thread(void* arg){
    while(1){
        broadcast_state();
        sleep(3);
    }
    return NULL;
}

void* connect_peer(void* arg){
    const char* peer_ip = (const char*)arg;
    while(1){
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock<0){ perror("socket"); sleep(5); continue;}
        struct sockaddr_in peer_addr;
        peer_addr.sin_family=AF_INET;
        peer_addr.sin_port=htons(PORT);
        inet_pton(AF_INET, peer_ip,&peer_addr.sin_addr);
        if(connect(sock,(struct sockaddr*)&peer_addr,sizeof(peer_addr))<0){
            close(sock);
            sleep(5);
            continue;
        }
        printf("[CONNECTED TO PEER] %s\n",peer_ip);
        char buffer[BUFFER_SIZE];
        while(1){
            int bytes = recv(sock, buffer,sizeof(buffer)-1,0);
            if(bytes<=0) break;
            buffer[bytes]='\0';
            pthread_mutex_lock(&lock);
            if(strstr(buffer,"\"Airplane Mode\":\"ON\"")) enable_airplane_mode();
            else if(strstr(buffer,"\"Airplane Mode\":\"OFF\"")) disable_airplane_mode();
            pthread_mutex_unlock(&lock);
        }
        close(sock);
        sleep(5);
    }
    return NULL;
}

// ------------------------------
// Main
// ------------------------------
int main(){
    pthread_t server_tid, broadcast_tid;
    pthread_create(&server_tid,NULL,server_thread,NULL);
    pthread_create(&broadcast_tid,NULL,broadcast_thread,NULL);

    // Connect to peers
    for(int i=0;i<PEER_COUNT;i++){
        pthread_t tid;
        pthread_create(&tid,NULL,connect_peer,(void*)PEERS[i]);
    }

    char cmd[32];
    while(1){
        printf("Enter command (toggle/status/exit): ");
        scanf("%s",cmd);
        if(strcmp(cmd,"toggle")==0) toggle_airplane_mode();
        else if(strcmp(cmd,"status")==0) print_status();
        else if(strcmp(cmd,"exit")==0) break;
        else printf("Unknown command\n");
    }

    printf("Exiting...\n");
    return 0;
}
