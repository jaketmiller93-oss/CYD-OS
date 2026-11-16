#include "pair_internal.h"
#include <dirent.h>
#include "mbedtls/sha256.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* OS file-manager access, not exposed to untrusted Lua packages. The settings
 * namespaces are NVS, never public files. OS metadata/staging cannot be edited. */
static bool path_ok(const char *s, bool write) {
    size_t n = strlen(s);
    if (n > 180)
        return false;
    if (!n)
        return !write;
    if (!cyd_safe_path(s))
        return false;
    const char *reserved[] = {".incoming", ".editing", ".previous"};
    for (unsigned i = 0; i < 3; i++) {
        size_t k = strlen(reserved[i]);
        if (n >= k && !strcmp(s + n - k, reserved[i]))
            return false;
    }
    const char *roots[] = {"documents", "reports", "projects", "data", "backups"};
    for (unsigned i = 0; i < 5; i++) {
        size_t k = strlen(roots[i]);
        if (!strncmp(s, roots[i], k) && (!s[k] || s[k] == '/'))
            return !write || (i < 2 && s[k] == '/');
    }
    return false;
}
static bool make_path(const char *rel, bool write, char *out, size_t cap) {
    if (!path_ok(rel, write))
        return false;
    if (snprintf(out, cap, CYD_ROOT "/%s", rel) >= (int)cap)
        return false;
#ifndef ESP_PLATFORM
    /* Walk components rather than just checking the final node. */
    for (char *p = out + 1;; p++)
        if (*p == '/' || !*p) {
            char c = *p;
            *p = 0;
            struct stat st;
            bool bad = !lstat(out, &st) && S_ISLNK(st.st_mode);
            *p = c;
            if (bad)
                return false;
            if (!c)
                break;
        }
#endif
    if (path_ok(rel, true)) {
        char backup[340], stage[340];
        snprintf(backup, sizeof(backup), "%s.previous", out);
        snprintf(stage, sizeof(stage), "%s.editing", out);
        struct stat old;
#ifdef ESP_PLATFORM
        bool exists = !stat(backup, &old);
#else
        bool exists = !lstat(backup, &old);
#endif
        if (exists) {
            if (!S_ISREG(old.st_mode))
                return false;
            struct stat current;
            if (stat(out, &current)) {
                if (rename(backup, out))
                    return false;
            } else
                unlink(backup);
            unlink(stage);
        }
    }
    return true;
}
esp_err_t cyd_pair_files(unsigned op, const void *bytes, size_t n, void *out, size_t cap,
                         size_t *size) {
    if (!cyd_storage_ready())
        return ESP_ERR_INVALID_STATE;
    if (n > CYD_PAIR_REQUEST_MAX)
        return ESP_ERR_INVALID_SIZE;
    if (op == CYD_PAIR_FILE_PUT) {
        if (cyd_maintenance_active() || n < 6)
            return ESP_ERR_INVALID_STATE;
        const uint8_t *p = bytes;
        uint32_t offset;
        memcpy(&offset, p, 4);
        size_t path_size = p[4] | (size_t)p[5] << 8;
        if (!path_size || path_size > 180 || path_size > n - 6 || memchr(p + 6, 0, path_size) ||
            offset > 2 * 1024 * 1024)
            return ESP_ERR_INVALID_ARG;
        char rel[181], dest[320], stage[340];
        memcpy(rel, p + 6, path_size);
        rel[path_size] = 0;
        if (!make_path(rel, true, dest, sizeof(dest)))
            return ESP_ERR_INVALID_ARG;
        struct stat st;
        if (!stat(dest, &st))
            return ESP_ERR_INVALID_STATE;
        snprintf(stage, sizeof(stage), "%s.incoming", dest);
        size_t take = n - 6 - path_size;
        if (take > 2048 || offset + take > 2 * 1024 * 1024)
            return ESP_ERR_INVALID_SIZE;
        if (offset && (stat(stage, &st) || (uint32_t)st.st_size != offset))
            return ESP_ERR_INVALID_STATE;
        cyd_mkdirs(CYD_ROOT "/documents");
        cyd_mkdirs(CYD_ROOT "/reports");
#ifndef ESP_PLATFORM
        struct stat link;
        if (!lstat(stage, &link) && S_ISLNK(link.st_mode))
            return ESP_ERR_INVALID_ARG;
#endif
        FILE *f = fopen(stage, offset ? "ab" : "wb");
        if (!f)
            return ESP_FAIL;
        bool ok = fwrite(p + 6 + path_size, 1, take, f) == take;
        if (fflush(f))
            ok = false;
        if (fclose(f))
            ok = false;
        return ok ? ESP_OK : ESP_FAIL;
    }
    cJSON *o = cyd_json_parse(bytes, n, CYD_PAIR_REQUEST_MAX);
    if (!cJSON_IsObject(o)) {
        cJSON_Delete(o);
        return ESP_ERR_INVALID_ARG;
    }
    const char *rel = cyd_pair_string(o, "path");
    char path[320];
    bool write = op >= CYD_PAIR_FILE_WRITE;
    esp_err_t e = ESP_ERR_INVALID_ARG;
    if (!make_path(rel, write, path, sizeof(path)))
        goto done;
    if (write && cyd_maintenance_active()) {
        e = ESP_ERR_INVALID_STATE;
        goto done;
    }
    if (op == CYD_PAIR_FILE_LIST) {
        cJSON *a = cJSON_CreateArray();
        if (!rel[0]) {
            const char *roots[] = {"documents", "reports", "projects", "data", "backups"};
            for (unsigned i = 0; i < 5; i++) {
                char p[256];
                snprintf(p, sizeof(p), CYD_ROOT "/%s", roots[i]);
                if (i < 2)
                    cyd_mkdirs(p);
                cJSON *v = cJSON_CreateObject();
                cJSON_AddStringToObject(v, "name", roots[i]);
                cJSON_AddBoolToObject(v, "directory", true);
                cJSON_AddBoolToObject(v, "writable", i < 2);
                cJSON_AddItemToArray(a, v);
            }
        } else {
            DIR *d = opendir(path);
            if (!d) {
                cJSON_Delete(a);
                e = ESP_ERR_NOT_FOUND;
                goto done;
            }
            struct dirent *ent;
            unsigned count = 0, scanned = 0;
            while ((ent = readdir(d)) && scanned++ < 256) {
                if (ent->d_name[0] == '.' || !cyd_safe_path(ent->d_name) ||
                    strchr(ent->d_name, '/'))
                    continue;
                char relative[280];
                snprintf(relative, sizeof(relative), "%.180s/%.96s", rel, ent->d_name);
                if (!path_ok(relative, false))
                    continue;
                if (++count > 64) {
                    closedir(d);
                    cJSON_Delete(a);
                    e = ESP_ERR_INVALID_SIZE;
                    goto done;
                }
                char child[640];
                snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
                struct stat st;
                if (stat(child, &st))
                    continue;
                cJSON *v = cJSON_CreateObject();
                cJSON_AddStringToObject(v, "name", ent->d_name);
                cJSON_AddBoolToObject(v, "directory", S_ISDIR(st.st_mode));
                cJSON_AddNumberToObject(v, "size", (double)st.st_size);
                cJSON_AddItemToArray(a, v);
            }
            closedir(d);
        }
        e = cyd_pair_encode(a, out, cap, size);
    } else if (op == CYD_PAIR_FILE_READ) {
        int64_t off, want;
        if (!cyd_pair_number(o, "offset", 0, 32 * 1024 * 1024, &off) ||
            !cyd_pair_number(o, "length", 1, 2048, &want))
            goto done;
        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode)) {
            e = ESP_ERR_NOT_FOUND;
            goto done;
        }
        FILE *f = fopen(path, "rb");
        if (!f) {
            e = ESP_FAIL;
            goto done;
        }
        if ((size_t)want > cap || fseek(f, (long)off, SEEK_SET)) {
            fclose(f);
            e = ESP_ERR_INVALID_SIZE;
            goto done;
        }
        *size = fread(out, 1, (size_t)want, f);
        e = ferror(f) ? ESP_FAIL : ESP_OK;
        fclose(f);
    } else if (op == CYD_PAIR_FILE_WRITE) {
        const char *text = cyd_pair_string(o, "text");
        int64_t offset;
        if (!cyd_pair_number(o, "offset", 0, 32 * 1024 * 1024, &offset))
            goto done;
        bool append = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "append"));
        if (strlen(text) > 4096)
            goto done;
        struct stat st;
        if (!append && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "replace")) &&
            !stat(path, &st)) {
            e = ESP_ERR_INVALID_STATE;
            goto done;
        }
        if (append && (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size != offset))
            goto done;
        char stage[340], backup[340];
        snprintf(stage, sizeof(stage), "%s.editing", path);
        snprintf(backup, sizeof(backup), "%s.previous", path);
        if (!append) {
            struct stat old;
            if (!stat(backup, &old)) {
                if (stat(path, &old))
                    rename(backup, path);
                else
                    unlink(backup);
            }
            unlink(stage);
        }
        FILE *f = fopen(append ? path : stage, append ? "ab" : "wb");
        if (!f) {
            e = ESP_FAIL;
            goto done;
        }
        bool ok = fwrite(text, 1, strlen(text), f) == strlen(text);
        if (fflush(f) || fsync(fileno(f)))
            ok = false;
        if (fclose(f))
            ok = false;
        if (!append && ok) {
            bool existed = !stat(path, &st);
            if (existed && rename(path, backup))
                ok = false;
            if (ok && rename(stage, path)) {
                ok = false;
                if (existed)
                    rename(backup, path);
            }
            if (ok)
                unlink(backup);
        }
        if (!ok && !append)
            unlink(stage);
        e = ok ? ESP_OK : ESP_FAIL;
    } else if (op == CYD_PAIR_FILE_COMMIT) {
        int64_t wanted;
        const char *digest = cyd_pair_string(o, "sha256");
        if (strlen(digest) != 64 || !cyd_pair_number(o, "size", 0, 2 * 1024 * 1024, &wanted))
            goto done;
        char stage[340];
        snprintf(stage, sizeof(stage), "%s.incoming", path);
        struct stat st;
#ifdef ESP_PLATFORM
        int staged = stat(stage, &st);
#else
        int staged = lstat(stage, &st);
#endif
        if (staged || !S_ISREG(st.st_mode) || st.st_size != wanted) {
            e = ESP_ERR_INVALID_STATE;
            goto done;
        }
        if (!stat(path, &st)) {
            e = ESP_ERR_INVALID_STATE;
            goto done;
        }
        FILE *f = fopen(stage, "rb");
        if (!f) {
            e = ESP_FAIL;
            goto done;
        }
        mbedtls_sha256_context hash;
        mbedtls_sha256_init(&hash);
        mbedtls_sha256_starts(&hash, 0);
        uint8_t buffer[512], sum[32];
        size_t got;
        while ((got = fread(buffer, 1, sizeof(buffer), f)))
            mbedtls_sha256_update(&hash, buffer, got);
        bool ok = !ferror(f);
        fclose(f);
        mbedtls_sha256_finish(&hash, sum);
        mbedtls_sha256_free(&hash);
        char hex[65];
        for (unsigned i = 0; i < 32; i++)
            snprintf(hex + i * 2, 3, "%02x", sum[i]);
        e = ok && !strcmp(hex, digest) && !rename(stage, path) ? ESP_OK : ESP_FAIL;
    } else if (op == CYD_PAIR_FILE_REMOVE) {
        struct stat st;
        if (stat(path, &st)) {
            e = ESP_ERR_NOT_FOUND;
            goto done;
        }
        e = (S_ISDIR(st.st_mode) ? rmdir(path) : unlink(path)) ? ESP_FAIL : ESP_OK;
    } else if (op == CYD_PAIR_FILE_MKDIR) {
        e = cyd_mkdirs(path);
    } else if (op == CYD_PAIR_FILE_RENAME) {
        char target[320];
        if (!make_path(cyd_pair_string(o, "target"), true, target, sizeof(target)))
            goto done;
        struct stat st;
        if (!stat(target, &st)) {
            e = ESP_ERR_INVALID_STATE;
            goto done;
        }
        e = rename(path, target) ? ESP_FAIL : ESP_OK;
    }
done:
    cJSON_Delete(o);
    return e;
}
