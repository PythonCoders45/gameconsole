#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <time.h>

#define MAX_PATH 512
#define DEFAULT_STORAGE (100*1024*1024) // 100 MB
#define STORAGE_FILE "storage_meta.dat"

typedef struct GameInfo {
    char name[MAX_PATH];
    char folder[MAX_PATH];
    size_t size;       // bytes
    bool installed;
    time_t install_time;
    struct GameInfo* next;
} GameInfo;

typedef struct {
    GameInfo* head;
    size_t total_storage;
    size_t used_storage;
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
    sm->total_storage = DEFAULT_STORAGE;
    sm->used_storage = 0;
}

// ---------------- Add Game ----------------
bool add_game(StorageManager* sm, const char* folder){
    size_t size = folder_size(folder);
    if(sm->used_storage + size > sm->total_storage){
        printf("[!] Not enough storage to add %s (%zu bytes)\n", folder, size);
        return false;
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
    g->next = sm->head;
    sm->head = g;

    sm->used_storage += size;
    printf("[*] Game added: %s (%zu bytes)\n", g->name, size);
    return true;
}

// ---------------- Remove Game ----------------
bool remove_game(StorageManager* sm, const char* name){
    GameInfo *prev = NULL, *cur = sm->head;
    while(cur){
        if(strcmp(cur->name,name)==0){
            if(prev) prev->next = cur->next;
            else sm->head = cur->next;
            sm->used_storage -= cur->size;
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
        printf(" - %s (%zu bytes) Installed: %s\n", cur->name, cur->size, buf);
        cur = cur->next;
    }
    printf("Used storage: %zu / %zu bytes\n", sm->used_storage, sm->total_storage);
}

// ---------------- Search Game ----------------
GameInfo* search_game(StorageManager* sm, const char* name){
    GameInfo* cur = sm->head;
    while(cur){
        if(strcmp(cur->name,name)==0) return cur;
        cur = cur->next;
    }
    return NULL;
}

// ---------------- Sort Games by Name ----------------
void sort_games_by_name(StorageManager* sm){
    if(!sm->head || !sm->head->next) return;
    bool swapped;
    do{
        swapped = false;
        GameInfo* cur = sm->head;
        GameInfo* prev = NULL;
        while(cur && cur->next){
            if(strcmp(cur->name, cur->next->name) > 0){
                GameInfo* tmp = cur->next;
                cur->next = tmp->next;
                tmp->next = cur;
                if(prev) prev->next = tmp;
                else sm->head = tmp;
                prev = tmp;
                swapped = true;
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
    } while(swapped);
}

// ---------------- Sort Games by Size ----------------
void sort_games_by_size(StorageManager* sm){
    if(!sm->head || !sm->head->next) return;
    bool swapped;
    do{
        swapped = false;
        GameInfo* cur = sm->head;
        GameInfo* prev = NULL;
        while(cur && cur->next){
            if(cur->size < cur->next->size){
                GameInfo* tmp = cur->next;
                cur->next = tmp->next;
                tmp->next = cur;
                if(prev) prev->next = tmp;
                else sm->head = tmp;
                prev = tmp;
                swapped = true;
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
    } while(swapped);
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
    sm->used_storage = 0;
}

// ---------------- Save Storage Metadata ----------------
void save_storage(StorageManager* sm){
    FILE* f = fopen(STORAGE_FILE,"wb");
    if(!f){
        printf("[!] Failed to save storage metadata.\n");
        return;
    }
    fwrite(&sm->total_storage,sizeof(size_t),1,f);
    fwrite(&sm->used_storage,sizeof(size_t),1,f);

    GameInfo* cur = sm->head;
    while(cur){
        fwrite(cur,sizeof(GameInfo),1,f);
        cur = cur->next;
    }
    fclose(f);
    printf("[*] Storage metadata saved.\n");
}

// ---------------- Load Storage Metadata ----------------
void load_storage(StorageManager* sm){
    FILE* f = fopen(STORAGE_FILE,"rb");
    if(!f){
        printf("[!] No existing storage metadata. Starting fresh.\n");
        init_storage(sm);
        return;
    }
    init_storage(sm);
    fread(&sm->total_storage,sizeof(size_t),1,f);
    fread(&sm->used_storage,sizeof(size_t),1,f);

    GameInfo tmp;
    GameInfo* last = NULL;
    while(fread(&tmp,sizeof(GameInfo),1,f) == 1){
        GameInfo* g = (GameInfo*)malloc(sizeof(GameInfo));
        memcpy(g,&tmp,sizeof(GameInfo));
        g->next = NULL;
        if(last) last->next = g;
        else sm->head = g;
        last = g;
    }
    fclose(f);
    printf("[*] Storage metadata loaded.\n");
}

// ---------------- Backup Storage ----------------
void backup_storage(StorageManager* sm, const char* backup_file){
    FILE* f = fopen(backup_file,"wb");
    if(!f){
        printf("[!] Failed to backup storage.\n");
        return;
    }
    GameInfo* cur = sm->head;
    while(cur){
        fwrite(cur,sizeof(GameInfo),1,f);
        cur = cur->next;
    }
    fclose(f);
    printf("[*] Storage backup saved to %s.\n", backup_file);
}

// ---------------- Restore Storage ----------------
void restore_storage(StorageManager* sm, const char* backup_file){
    FILE* f = fopen(backup_file,"rb");
    if(!f){
        printf("[!] Failed to restore storage.\n");
        return;
    }
    free_storage(sm);
    GameInfo tmp, *last=NULL;
    while(fread(&tmp,sizeof(GameInfo),1,f)==1){
        GameInfo* g = (GameInfo*)malloc(sizeof(GameInfo));
        memcpy(g,&tmp,sizeof(GameInfo));
        g->next=NULL;
        if(last) last->next=g;
        else sm->head=g;
        last=g;
        sm->used_storage += g->size;
    }
    fclose(f);
    printf("[*] Storage restored from %s.\n", backup_file);
}

// ---------------- Example Usage ----------------
#ifdef STORAGE_TEST
int main(){
    StorageManager sm;
    load_storage(&sm);

    add_game(&sm,"./games/super_game");
    add_game(&sm,"./games/cool_game");

    list_games(&sm);

    sort_games_by_name(&sm);
    printf("\nSorted by name:\n");
    list_games(&sm);

    sort_games_by_size(&sm);
    printf("\nSorted by size:\n");
    list_games(&sm);

    save_storage(&sm);

    remove_game(&sm,"super_game");
    printf("\nAfter removal:\n");
    list_games(&sm);

    backup_storage(&sm,"backup.dat");
    free_storage(&sm);

    restore_storage(&sm,"backup.dat");
    printf("\nAfter restore:\n");
    list_games(&sm);

    free_storage(&sm);
    return 0;
}
#endif
