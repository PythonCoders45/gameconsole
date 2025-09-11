// installer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "storage.c"  // Link to storage manager

#define MAX_PATH 512

// Copy folder recursively
int copy_file(const char* src, const char* dest){
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

// Install from game card
int install_from_card(StorageManager* sm, const char* card_path, const char* install_dir){
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
        return add_game(sm, dest, thumbnail, executable);
    }
    return 0;
}
