#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_PATH 512
#define INTERNAL_STORAGE (100*1024*1024)

typedef enum { INTERNAL, MICROSD } StorageType;

typedef struct GameInfo {
    char name[MAX_PATH];
    char folder[MAX_PATH];
    char thumbnail[MAX_PATH];
    char executable[MAX_PATH];
    size_t size;
    StorageType storage;
    time_t install_time;
    struct GameInfo* next;
} GameInfo;

typedef struct {
    GameInfo* head;
    size_t internal_total;
    size_t internal_used;
    size_t sd_total;
    size_t sd_used;
    int sd_inserted;
} StorageManager;

void init_storage(StorageManager* sm){
    sm->head=NULL;
    sm->internal_total=INTERNAL_STORAGE;
    sm->internal_used=0;
    sm->sd_total=0;
    sm->sd_used=0;
    sm->sd_inserted=0;
}

void insert_sd(StorageManager* sm,size_t bytes){
    sm->sd_inserted=1;
    sm->sd_total=bytes;
    sm->sd_used=0;
    printf("[*] microSD inserted: %zu bytes\n",bytes);
}

// External installer
int install_from_card(StorageManager* sm,const char* card_path,const char* install_dir);

int add_game(StorageManager* sm,const char* folder,const char* thumb,const char* exec){
    size_t sz=0;  // simplified, real size calculation can use folder_size()
    StorageType st=INTERNAL;
    if(sm->internal_used+sz<=sm->internal_total) sm->internal_used+=sz;
    else if(sm->sd_inserted && sm->sd_used+sz<=sm->sd_total){ st=MICROSD; sm->sd_used+=sz; }
    else { printf("[!] Not enough storage\n"); return 0; }

    GameInfo* g=(GameInfo*)malloc(sizeof(GameInfo));
    strncpy(g->folder,folder,MAX_PATH);
    strncpy(g->thumbnail,thumb,MAX_PATH);
    strncpy(g->executable,exec,MAX_PATH);
    const char* slash=strrchr(folder,'/');
    if(slash) strncpy(g->name,slash+1,MAX_PATH);
    else strncpy(g->name,folder,MAX_PATH);
    g->size=sz;
    g->storage=st;
    g->install_time=time(NULL);
    g->next=sm->head;
    sm->head=g;
    printf("[*] Game added: %s\n",g->name);
    return 1;
}

// ---------------- Auto Detect Game Cards ----------------
#ifdef _WIN32
#include <windows.h>
void detect_game_cards(StorageManager* sm,const char* install_dir){
    DWORD drives=GetLogicalDrives();
    for(char c='D';c<='Z';c++){
        if(drives & (1<<(c-'A'))){
            char path[MAX_PATH];
            sprintf(path,"%c:/game_card",c);
            DIR* dir=opendir(path);
            if(dir){
                closedir(dir);
                printf("[*] Detected game card on %c:\\\n",c);
                install_from_card(sm,path,install_dir);
            }
        }
    }
}
#else
#include <unistd.h>
void detect_game_cards(StorageManager* sm,const char* install_dir){
    const char* mounts[]={"/media","/mnt","/run/media"};
    for(int i=0;i<3;i++){
        DIR* d=opendir(mounts[i]);
        if(!d) continue;
        struct dirent* e;
        while((e=readdir(d))){
            if(strcmp(e->d_name,".")==0 || strcmp(e->d_name,"..")==0) continue;
            char path[MAX_PATH];
            snprintf(path,MAX_PATH,"%s/%s/game_card",mounts[i],e->d_name);
            DIR* card=opendir(path);
            if(card){
                closedir(card);
                printf("[*] Detected game card: %s\n",path);
                install_from_card(sm,path,install_dir);
            }
        }
        closedir(d);
    }
}
#endif

// ---------------- Main ----------------
int main(){
    StorageManager sm;
    init_storage(&sm);
    insert_sd(&sm,200*1024*1024);

    char install_dir[MAX_PATH];
    printf("Enter internal install directory: ");
    scanf("%s",install_dir);

    printf("[*] Scanning for game cards...\n");
    while(1){
        detect_game_cards(&sm,install_dir);
        sleep(5); // check every 5 seconds
    }

    return 0;
}
