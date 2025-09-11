#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <time.h>

#define MAX_PATH 512
#define INTERNAL_STORAGE (100*1024*1024) // 100 MB
#define STORAGE_FILE "storage_meta.dat"

typedef enum { INTERNAL, MICROSD } StorageType;

typedef struct GameInfo {
    char name[MAX_PATH];
    char folder[MAX_PATH];
    size_t size;             // bytes
    bool installed;
    time_t install_time;
    StorageType storage;      // where the game is stored
    struct GameInfo* next;
} GameInfo;

typedef struct {
    GameInfo* head;
    size_t internal_total;
    size_t internal_used;
    size_t sd_total;
    size_t sd_used;
    bool sd_inserted;
} StorageManager;

// ---------------- Utility: Folder Size ----------------
size_t folder_size(const char* path){
    size_t total = 0;
    DIR* dir = opendir(path);
    if(!dir) return 0;
    struct dirent* entry;
    char fullpath[MAX_PATH];
    struct stat st;
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;
        snprintf(fullpath, MAX_PATH, "%s/%s", path, entry->d_name);
        if(stat(fullpath,&st)==0){
            if(S_ISDIR(st.st_mode)) total += folder_size(fullpath);
            else total += st.st_size;
        }
    }
    closedir(dir);
    return total;
}

// ---------------- Initialize Storage ----------------
void init_storage(StorageManager* sm){
    sm->head = NULL;
    sm->internal_total = INTERNAL_STORAGE;
    sm->internal_used = 0;
    sm->sd_total = 0;
    sm->sd_used = 0;
    sm->sd_inserted = false;
}

// ---------------- Insert microSD ----------------
void insert_sd(StorageManager* sm, size_t size_bytes){
    sm->sd_inserted = true;
    sm->sd_total = size_bytes;
    sm->sd_used = 0;
    printf("[*] microSD inserted: %zu bytes\n", size_bytes);
}

// ---------------- Remove microSD ----------------
void remove_sd(StorageManager* sm){
    sm->sd_inserted = false;
    sm->sd_total = 0;
    sm->sd_used = 0;
    printf("[*] microSD removed\n");
}

// ---------------- Add Game ----------------
bool add_game(StorageManager* sm, const char* folder, StorageType storage_type){
    size_t size = folder_size(folder);
    if(storage_type == INTERNAL){
        if(sm->internal_used + size > sm->internal_total){
            printf("[!] Not enough internal storage to add %s (%zu bytes)\n", folder, size);
            return false;
        }
    } else if(storage_type == MICROSD){
        if(!sm->sd_inserted){
            printf("[!] No microSD inserted!\n");
            return false;
        }
        if(sm->sd_used + size > sm->sd_total){
            printf("[!] Not enough microSD storage to add %s (%zu bytes)\n", folder, size);
            return false;
        }
    }

    GameInfo* g = (GameInfo*)malloc(sizeof(GameInfo));
    if(!g) return false;

    snprintf(g->folder, MAX_PATH, "%s", folder);
    const char* slash = strrchr(folder,'/');
    if(slash) snprintf(g->name, MAX_PATH, "%s", slash+1);
    else snprintf(g->name, MAX_PATH, "%s", folder);

    g->size = size;
    g->installed = true;
    g->install_time = time(NULL);
    g->storage = storage_type;
    g->next = sm->head;
    sm->head = g;

    if(storage_type == INTERNAL) sm->internal_used += size;
    else sm->sd_used += size;

    printf("[*] Game added: %s (%zu bytes) on %s\n", g->name, size, storage_type==INTERNAL?"Internal":"microSD");
    return true;
}

// ---------------- Remove Game ----------------
bool remove_game(StorageManager* sm, const char* name){
    GameInfo *prev = NULL, *cur = sm->head;
    while(cur){
        if(strcmp(cur->name,name)==0){
            if(prev) prev->next = cur->next;
            else sm->head = cur->next;

            if(cur->storage == INTERNAL) sm->internal_used -= cur->size;
            else sm->sd_used -= cur->size;

            free(cur);
            printf("[*] Game removed: %s\n", name);
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

// ---------------- List Games ----------------
void list_games(StorageManager* sm){
    printf("Installed Games:\n");
    GameInfo* cur = sm->head;
    while(cur){
        char buf[64];
        strftime(buf,64,"%Y-%m-%d %H:%M:%S",localtime(&cur->install_time));
        printf(" - %s (%zu bytes) Installed: %s on %s\n", cur->name, cur->size, buf,
               cur->storage==INTERNAL?"Internal":"microSD");
        cur = cur->next;
    }
    printf("Internal: %zu/%zu bytes, microSD: %zu/%zu bytes\n",
           sm->internal_used, sm->internal_total,
           sm->sd_used, sm->sd_total);
}

// ---------------- Check if Game Exists ----------------
GameInfo* search_game(StorageManager* sm, const char* name){
    GameInfo* cur = sm->head;
    while(cur){
        if(strcmp(cur->name,name)==0) return cur;
        cur = cur->next;
    }
    return NULL;
}

// ---------------- Free All Games ----------------
void free_storage(StorageManager* sm){
    GameInfo* cur = sm->head;
    while(cur){
        GameInfo* tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    sm->head = NULL;
    sm->internal_used = 0;
    sm->sd_used = 0;
}

// ---------------- Example Usage ----------------
#ifdef STORAGE_TEST
int main(){
    StorageManager sm;
    init_storage(&sm);

    insert_sd(&sm, 200*1024*1024); // 200 MB microSD

    add_game(&sm,"./games/super_game", INTERNAL);
    add_game(&sm,"./games/cool_game", MICROSD);
    list_games(&sm);

    remove_game(&sm,"super_game");
    printf("\nAfter removal:\n");
    list_games(&sm);

    remove_sd(&sm);
    list_games(&sm);

    free_storage(&sm);
    return 0;
}
#endif
