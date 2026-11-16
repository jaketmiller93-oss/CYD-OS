/* Read-only OS wall clock; explicitly permissioned, flat shared Documents.
 * No app-private directory, credential, arbitrary path, or SD-root access. */
#include "cyd_device.h"
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#define DOC_ROOT CYD_ROOT "/documents"
#define DOC_MAX 64
#define DOC_BYTES 16384
/* FAT has no symlinks and IDF VFS provides stat, not lstat. Host validation
 * additionally refuses symlinks rather than following them out of Documents. */
#ifdef ESP_PLATFORM
#define doc_stat stat
#else
#define doc_stat lstat
#endif
static bool doc_name(const char *name) {
    size_t n=strlen(name);
    return n && n<=48 && cyd_safe_path(name) && !strchr(name,'/');
}
static int doc_error(lua_State *L, const char *s) {
    lua_pushnil(L); lua_pushstring(L,s); return 2;
}
static bool doc_root(lua_State *L) {
    if (!context(L)->manifest.documents) luaL_error(L,"documents permission required");
    if (!cyd_storage_ready() || cyd_mkdirs(DOC_ROOT)) return false;
    struct stat st;
    if (doc_stat(DOC_ROOT,&st) || !S_ISDIR(st.st_mode)) return false;
    /* Recover interrupted replacements. The previous file wins if the new
     * name was not committed. FAT rename cannot overwrite an existing file. */
    DIR *dir=opendir(DOC_ROOT);if(!dir)return false;
    struct dirent *e;unsigned seen=0;bool ok=true;
    while((e=readdir(dir)) && ++seen<=256) {
        bool old=!strncmp(e->d_name,".old-",5),fresh=!strncmp(e->d_name,".new-",5);
        if ((!old&&!fresh) || !doc_name(e->d_name+5)) continue;
        char p[256],q[256];snprintf(p,sizeof(p),DOC_ROOT "/%.53s",e->d_name);
        if (doc_stat(p,&st) || !S_ISREG(st.st_mode)) {ok=false;break;}
        if (fresh) {if(unlink(p))ok=false;continue;}
        snprintf(q,sizeof(q),DOC_ROOT "/%.48s",e->d_name+5);
        if (!doc_stat(q,&st)) {if(!S_ISREG(st.st_mode)||unlink(p))ok=false;}
        else if(rename(p,q))ok=false;
    }
    closedir(dir);return ok && seen<=256;
}
static bool doc_path(lua_State *L,int index,char *out,size_t size) {
    size_t n; const char *name=luaL_checklstring(L,index,&n);
    if (!n || n>48 || memchr(name,0,n) || !doc_name(name)) return false;
    snprintf(out,size,DOC_ROOT "/%s",name); return true;
}
static bool doc_regular(const char *path,struct stat *st) {
    return !doc_stat(path,st) && S_ISREG(st->st_mode);
}
typedef struct { char name[49]; unsigned size; } doc_entry_t;
static int doc_compare(const void *a,const void *b) {
    return strcmp(((const doc_entry_t *)a)->name,((const doc_entry_t *)b)->name);
}
static int documents_list(lua_State *L) {
    if (!doc_root(L)) return doc_error(L,"SD documents unavailable");
    native_holder_t *h=native_holder(L);
    doc_entry_t *items=heap_caps_calloc(DOC_MAX,sizeof(*items),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!items) return doc_error(L,"Documents memory unavailable");
    h->data=items;
    DIR *dir=opendir(DOC_ROOT); if (!dir) { free(items);h->data=NULL;return doc_error(L,"Cannot list documents"); }
    struct dirent *e; unsigned count=0; bool overflow=false;
    while ((e=readdir(dir))) {
        if (e->d_name[0]=='.') continue;
        char p[256]; struct stat st;
        if (strlen(e->d_name)>48 || !cyd_safe_path(e->d_name) || strchr(e->d_name,'/')) continue;
        snprintf(p,sizeof(p),DOC_ROOT "/%.48s",e->d_name);
        if (!doc_regular(p,&st)) continue;
        if (count==DOC_MAX) {overflow=true;break;}
        memcpy(items[count].name,e->d_name,strlen(e->d_name)+1);
        items[count++].size=(unsigned)st.st_size;
    }
    closedir(dir);
    if (overflow) {free(items);h->data=NULL;return doc_error(L,"Documents limit: 64 files");}
    qsort(items,count,sizeof(*items),doc_compare);
    lua_createtable(L,count,0);
    for (unsigned i=0;i<count;i++) {
        lua_createtable(L,0,2);
        lua_pushstring(L,items[i].name);lua_setfield(L,-2,"name");
        lua_pushinteger(L,items[i].size);lua_setfield(L,-2,"size");
        lua_rawseti(L,-2,i+1);
    }
    free(items);h->data=NULL;return 1;
}
static int documents_read(lua_State *L) {
    if (!doc_root(L)) return doc_error(L,"SD documents unavailable");
    char p[256];struct stat st;
    if (!doc_path(L,1,p,sizeof(p))) return doc_error(L,"Use a lowercase filename, at most 48 bytes");
    if (!doc_regular(p,&st)) return doc_error(L,"File not found or not regular");
    if (st.st_size<0 || st.st_size>DOC_BYTES) return doc_error(L,"File limit: 16 KiB");
    native_holder_t *h=native_holder(L);uint8_t *bytes;size_t n;
    if (cyd_read_file(p,&bytes,&n,DOC_BYTES)) return doc_error(L,"Cannot read document");
    h->data=bytes;lua_pushlstring(L,(char *)bytes,n);free(bytes);h->data=NULL;return 1;
}
static int documents_write(lua_State *L) {
    if (!doc_root(L)) return doc_error(L,"SD documents unavailable");
    char p[256];struct stat st;
    if (!doc_path(L,1,p,sizeof(p))) return doc_error(L,"Use a lowercase filename, at most 48 bytes");
    size_t n;const char *bytes=luaL_checklstring(L,2,&n);
    if (n>DOC_BYTES) return doc_error(L,"File limit: 16 KiB");
    bool exists=!doc_stat(p,&st);
    if (exists && (!S_ISREG(st.st_mode) || !lua_toboolean(L,3))) return doc_error(L,"File exists; explicit overwrite required");
    DIR *dir=opendir(DOC_ROOT);if (!dir) return doc_error(L,"Cannot open documents");
    unsigned count=0;struct dirent *e;
    while ((e=readdir(dir))) if (e->d_name[0]!='.') ++count;
    closedir(dir);
    if (!exists && count>=DOC_MAX) return doc_error(L,"Documents limit: 64 files");
    char tmp[256],backup[256];const char *name=strrchr(p,'/')+1;
    snprintf(tmp,sizeof(tmp),DOC_ROOT "/.new-%s",name);
    snprintf(backup,sizeof(backup),DOC_ROOT "/.old-%s",name);
    if (!doc_stat(tmp,&st) || !doc_stat(backup,&st)) return doc_error(L,"Pending replacement exists");
    if (cyd_write_file(tmp,bytes,n)) {unlink(tmp);return doc_error(L,"Document write failed");}
    if (exists && rename(p,backup)) {unlink(tmp);return doc_error(L,"Cannot preserve old document");}
    if (rename(tmp,p)) {if(exists)rename(backup,p);unlink(tmp);return doc_error(L,"Document replacement failed");}
    if (exists) unlink(backup);
    lua_pushboolean(L,true);return 1;
}
static int documents_rename(lua_State *L) {
    if (!doc_root(L)) return doc_error(L,"SD documents unavailable");
    char p[256],q[256];struct stat st;
    if (!doc_path(L,1,p,sizeof(p)) || !doc_path(L,2,q,sizeof(q))) return doc_error(L,"Invalid filename");
    if (!doc_regular(p,&st)) return doc_error(L,"File not found or not regular");
    if (!strcmp(p,q)) {lua_pushboolean(L,true);return 1;}
    if (!doc_stat(q,&st)) return doc_error(L,"Destination exists");
    if (rename(p,q)) return doc_error(L,"Rename failed");
    lua_pushboolean(L,true);return 1;
}
static int documents_delete(lua_State *L) {
    if (!doc_root(L)) return doc_error(L,"SD documents unavailable");
    char p[256];struct stat st;
    if (!doc_path(L,1,p,sizeof(p))) return doc_error(L,"Invalid filename");
    if (!doc_regular(p,&st) || unlink(p)) return doc_error(L,"Cannot delete document");
    lua_pushboolean(L,true);return 1;
}
static int wall_clock(lua_State *L) {
    cyd_device_status_t s;cyd_device_snapshot(&s);
    time_t utc=time(NULL),local=utc+(time_t)s.utc_offset_minutes*60;struct tm t;
    lua_createtable(L,0,12);
    lua_pushboolean(L,s.clock_valid);lua_setfield(L,-2,"valid");
    lua_pushinteger(L,s.utc_offset_minutes);lua_setfield(L,-2,"offset_minutes");
    lua_pushboolean(L,s.clock_12h);lua_setfield(L,-2,"clock_12h");
    if (s.clock_valid && gmtime_r(&local,&t)) {
        lua_pushinteger(L,utc);lua_setfield(L,-2,"epoch");
        const char *keys[]={"year","month","day","hour","minute","second","weekday"};
        int values[]={t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,t.tm_wday};
        for (unsigned i=0;i<7;i++){lua_pushinteger(L,values[i]);lua_setfield(L,-2,keys[i]);}
    }
    return 1;
}
