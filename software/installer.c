#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define THUMB_WIDTH 128
#define THUMB_HEIGHT 128
#define SPACING 20
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

// ---------------- Storage Manager ----------------
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

int add_game(StorageManager* sm,const char* folder,const char* thumb,const char* exec){
    size_t sz=0; // simplified; in real OS calculate folder size
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

// ---------------- Copy Folder ----------------
int copy_file(const char* src,const char* dest){
    FILE *fsrc=fopen(src,"rb");
    if(!fsrc) return 0;
    FILE *fdest=fopen(dest,"wb");
    if(!fdest){ fclose(fsrc); return 0; }
    char buf[8192];
    size_t n;
    while((n=fread(buf,1,sizeof(buf),fsrc))>0) fwrite(buf,1,n,fdest);
    fclose(fsrc); fclose(fdest);
    return 1;
}

int copy_folder(const char* src,const char* dest){
    DIR* dir=opendir(src);
    if(!dir) return 0;
    mkdir(dest,0777);
    struct dirent* entry;
    char srcpath[MAX_PATH], destpath[MAX_PATH];
    struct stat st;
    while((entry=readdir(dir))){
        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;
        snprintf(srcpath, MAX_PATH, "%s/%s", src, entry->d_name);
        snprintf(destpath, MAX_PATH, "%s/%s", dest, entry->d_name);
        stat(srcpath, &st);
        if(S_ISDIR(st.st_mode)) copy_folder(srcpath, destpath);
        else copy_file(srcpath, destpath);
    }
    closedir(dir);
    return 1;
}

// ---------------- Install from Game Card ----------------
int install_from_card(StorageManager* sm,const char* card_path,const char* install_dir){
    DIR* dir=opendir(card_path);
    if(!dir){ printf("[!] Cannot open game card %s\n", card_path); return 0; }

    char thumbnail[MAX_PATH]="", executable[MAX_PATH]="";
    struct dirent* entry;
    while((entry=readdir(dir))){
        if(strstr(entry->d_name,".exe") || strstr(entry->d_name,"run_game.sh"))
            snprintf(executable, MAX_PATH, "%s/%s", card_path, entry->d_name);
        if(strstr(entry->d_name,".png") || strstr(entry->d_name,"thumbnail.jpg"))
            snprintf(thumbnail, MAX_PATH, "%s/%s", card_path, entry->d_name);
    }
    closedir(dir);

    const char* slash=strrchr(card_path,'/');
    char name[MAX_PATH];
    if(slash) snprintf(name, MAX_PATH, "%s", slash+1);
    else snprintf(name, MAX_PATH, "%s", card_path);

    char dest[MAX_PATH];
    snprintf(dest, MAX_PATH, "%s/%s", install_dir, name);

    if(copy_folder(card_path,dest)){
        return add_game(sm,dest,thumbnail,executable);
    }
    return 0;
}

// ---------------- Auto Detect Game Cards ----------------
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

// ---------------- SDL2 GUI ----------------
void display_games(StorageManager* sm){
    if(SDL_Init(SDL_INIT_VIDEO)!=0){ printf("SDL error: %s\n",SDL_GetError()); return; }
    SDL_Window* win=SDL_CreateWindow("Custom Game OS",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WINDOW_WIDTH,WINDOW_HEIGHT,0);
    SDL_Renderer* rend=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);

    int running=1;
    SDL_Event e;
    while(running){
        SDL_SetRenderDrawColor(rend,30,30,30,255);
        SDL_RenderClear(rend);

        GameInfo* cur=sm->head;
        int x=SPACING,y=SPACING;
        while(cur){
            SDL_Surface* surf=IMG_Load(cur->thumbnail);
            if(surf){
                SDL_Texture* tex=SDL_CreateTextureFromSurface(rend,surf);
                SDL_Rect dst={x,y,THUMB_WIDTH,THUMB_HEIGHT};
                SDL_RenderCopy(rend,tex,NULL,&dst);
                SDL_FreeSurface(surf);
                SDL_DestroyTexture(tex);
            }
            x+=THUMB_WIDTH+SPACING;
            if(x+THUMB_WIDTH>WINDOW_WIDTH){ x=SPACING; y+=THUMB_HEIGHT+SPACING; }
            cur=cur->next;
        }
        SDL_RenderPresent(rend);

        // Handle events
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=0;
            if(e.type==SDL_MOUSEBUTTONDOWN){
                int mx=e.button.x,my=e.button.y;
                cur=sm->head; x=SPACING; y=SPACING;
                while(cur){
                    SDL_Rect rect={x,y,THUMB_WIDTH,THUMB_HEIGHT};
                    if(mx>=rect.x && mx<=rect.x+rect.w && my>=rect.y && my<=rect.y+rect.h){
                        printf("[*] Launching game: %s\n",cur->executable);
                        system(cur->executable);
                    }
                    x+=THUMB_WIDTH+SPACING;
                    if(x+THUMB_WIDTH>WINDOW_WIDTH){ x=SPACING; y+=THUMB_HEIGHT+SPACING; }
                    cur=cur->next;
                }
            }
        }

        detect_game_cards(sm,"/home/user/games"); // Auto-detect inserted cards every frame
        SDL_Delay(100); // ~10 fps polling
    }

    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

// ---------------- Main ----------------
int main(){
    StorageManager sm;
    init_storage(&sm);
    insert_sd(&sm,200*1024*1024);

    char install_dir[MAX_PATH];
    printf("Enter install directory: ");
    scanf("%s",install_dir);

    display_games(&sm); // GUI with auto-install
    return 0;
}
