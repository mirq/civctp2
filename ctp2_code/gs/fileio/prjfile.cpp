#include "c3.h"
#include "prjfile.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <ctype.h>

#ifndef WIN32
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <fcntl.h>
#if !defined(CTP2_AMIGA_BUILD)
#include <sys/mman.h>
#endif
#include <dirent.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif // !WIN32
#include <stdint.h>

#include "CivPaths.h"       // g_civPaths
#include "ctp2_byteorder.h"
#include "rimutils.h"       // RIMHeader

#define MAX_ENTRIES_PER_TABLE 100
#define ZFSFLAG_DELETED 0x0001

#ifdef __linux__
#pragma pack(push, 0)
#endif
struct ZFS_RENTRY {
    char      rname[MAX_RECORDNAME_LENGTH];
    uint32_t  offset;
    uint32_t  rnum;
    uint32_t  size;
    uint32_t  time;
    uint32_t  flags;
};

struct ZFS_DTABLE {
    int32_t       next_dtable;
    ZFS_RENTRY    rentry[MAX_ENTRIES_PER_TABLE];
};

struct ZFS_FHEADER {
    char      filetag[4];
    int32_t   version;
    int32_t   max_recordname_length;
    int32_t   max_entries_per_table;
    int32_t   num_rentries;
    int32_t   encrypt_key;
    int32_t   dtable_head;
};
#ifdef __linux__
#pragma pack(pop, 0)
#endif

// ZFS archives are little-endian: the header and directory tables in the
// machine's order (nothing to do on a little-endian machine). Read through
// addPath_ZFS, which is what the big-endian builds use.
static void SwapHeaderLE(ZFS_FHEADER & header)
{
    ctp2_swap_le(header.version);
    ctp2_swap_le(header.max_recordname_length);
    ctp2_swap_le(header.max_entries_per_table);
    ctp2_swap_le(header.num_rentries);
    ctp2_swap_le(header.encrypt_key);
    ctp2_swap_le(header.dtable_head);
}

static void SwapDTableLE(ZFS_DTABLE & dtable)
{
    ctp2_swap_le(dtable.next_dtable);
    for (int i = 0; i < MAX_ENTRIES_PER_TABLE; ++i)
    {
        ctp2_swap_le(dtable.rentry[i].offset);
        ctp2_swap_le(dtable.rentry[i].rnum);
        ctp2_swap_le(dtable.rentry[i].size);
        ctp2_swap_le(dtable.rentry[i].time);
        ctp2_swap_le(dtable.rentry[i].flags);
    }
}

char *strcasecpy(char *out, char const * in)
{
    char *maxout = out + MAX_RECORDNAME_LENGTH - 1;
    char *rval = out;
    while(*in && (out < maxout)) {
        *out++ = static_cast<char>(tolower(*in++));
    }
    *out = 0;
    return rval;
}

int PFEntry_compare(const void *a, const void *b)
{
    return(stricmp(((PFEntry *) a)->rname, ((PFEntry *) b)->rname));
}

#ifdef WIN32
void * mapFile(char const * path, long *size, HANDLE *mhandle, HANDLE *fhandle)
{

    *fhandle = CreateFile(path,
                          GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          0,
                          NULL);
    if (*fhandle == INVALID_HANDLE_VALUE) {
        return(NULL);
    }

    *size = GetFileSize(*fhandle, NULL);

    *mhandle = CreateFileMapping(*fhandle,
                                 NULL,
                                 PAGE_READONLY,
                                 0,
                                 0,
                                 NULL);
    if (*mhandle == INVALID_HANDLE_VALUE) {
        CloseHandle(*fhandle);
        return(NULL);
    }

    void *  ptr = MapViewOfFile(*mhandle, FILE_MAP_READ, 0, 0, 0);
    if (ptr == NULL)
    {
        CloseHandle(*mhandle);
        CloseHandle(*fhandle);
    }

    return ptr;
}

void unmapFile(void *ptr, HANDLE mhandle, HANDLE fhandle)
{
    UnmapViewOfFile(ptr);
    CloseHandle(mhandle);
    CloseHandle(fhandle);
}
#else

#if defined(CTP2_AMIGA_BUILD)
// No memory mapped files on AmigaOS: the archive is read into memory. (The
// game asks for this only with use_filemapping, which addPath turns off on the
// Amiga: entries are then read from the file as they are needed.)
void * mapFile(char const *path, long *size, PFPath &pfp)
{
    FILE * fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    void * ptr = malloc(*size);
    if (ptr && fread(ptr, *size, 1, fp) != 1)
    {
        free(ptr);
        ptr = NULL;
    }
    fclose(fp);

    pfp.zms_size = *size;
    pfp.zms_hf   = -1;
    return ptr;
}

void unmapFile(PFPath &pfp)
{
    free(pfp.zms_start);
}
#else
void * mapFile(char const *path, long *size, PFPath &pfp)
{
    void *ptr;
    struct stat tmpstat = { 0 };
    if (stat(CI_FixName(path), &tmpstat) == -1) {
    	return NULL;
    }
    int fd = open(CI_FixName(path), O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    pfp.zms_size = tmpstat.st_size;
    *size = tmpstat.st_size;

    ptr = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!ptr) {
    	close(fd);
        return(NULL);
    }
    pfp.zms_hf = fd;

    return(ptr);
}

void unmapFile(PFPath &pfp)
{
    munmap(pfp.zms_start, pfp.zms_size);
    close(pfp.zms_hf);
}
#endif // CTP2_AMIGA_BUILD
#endif

ProjectFile::ProjectFile()
:
    m_num_paths         (0),
    m_entries           (NULL),
    m_num_entries       (0),
    m_Reported          ()
{
    m_error_string[0] = 0;
    memset(m_paths, 0, sizeof(PFPath) * MAX_PRJFILE_PATHS);
}

ProjectFile::~ProjectFile()
{
    if (m_entries)
    {
        free(m_entries);
    }

    for (size_t i = 0; i < MAX_PRJFILE_PATHS; i++)
    {
        switch(m_paths[i].type)
        {
        default:
            break;

        case PRJFILE_PATH_ZFS:
            fclose(m_paths[i].zfs_fp);
            break;

        case PRJFILE_PATH_ZMS:
#ifdef WIN32
            unmapFile(m_paths[i].zms_start,
                      m_paths[i].zms_hm,
                      m_paths[i].zms_hf
                     );
#else
            unmapFile(m_paths[i]);
#endif
            break;
        }
    }
}

int ProjectFile::mergeEntries(PFEntry *newList, int newCount)
{

    if (m_entries) {
        m_entries =
            (PFEntry *)realloc(m_entries, (sizeof(PFEntry) *
                                           (newCount + m_num_entries)));
    } else {
        m_entries = (PFEntry *)malloc(sizeof(PFEntry) * newCount);
        m_num_entries = 0;
    }
    if (m_entries == NULL) {
        sprintf(m_error_string, "Not enough memory");
        return(0);
    }

    memcpy(m_entries + m_num_entries, newList, sizeof(PFEntry) * newCount);
    m_num_entries += newCount;

    qsort(m_entries, m_num_entries, sizeof(PFEntry), PFEntry_compare);

    return(1);
}

PFEntry *ProjectFile::findRecord(char const * rname) const
{
    PFEntry key;

    strcasecpy(key.rname, rname);

    return((PFEntry *)bsearch(&key, m_entries, m_num_entries,
                              sizeof(PFEntry), PFEntry_compare));
}

bool ProjectFile::exists(char const * rname) const
{
    return findRecord(rname) != NULL;
}

void *ProjectFile::getData_DOS(PFEntry *entry, size_t & size, C3DIR dir)
{
    char tempstr[256];

	if ((dir == C3DIR_DIRECT) || !g_civPaths->FindFile(dir, entry->rname, tempstr))
    {
		sprintf(tempstr, "%s%s%s", m_paths[entry->path].dos_path, FILE_SEP, entry->rname);
	}

    FILE *fp = fopen(tempstr, "rb");

    if (fp == NULL) {
        sprintf(m_error_string, "Couldn't open file \"%s\"", tempstr);
        return(NULL);
    }
#if defined(CTP2_AMIGA_BUILD)
    // libnix reads an unbuffered stream one byte per DOS call.
    setvbuf(fp, NULL, _IOFBF, CTP2_AMIGA_STDIO_BUFFER);
#else
    setvbuf(fp, NULL, _IONBF, 0);
#endif

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    char *data = (char *) malloc(size);
    if (data == NULL) {
        sprintf(m_error_string, "Could not malloc data for record %s\n", tempstr);
        return(NULL);
    }

    fseek(fp, 0, SEEK_SET);
    if (fread(data, size, 1, fp) < 1) {
        sprintf(m_error_string, "Could not read file \"%s\"", tempstr);
        free(data);
        fclose(fp);
        return(NULL);
    }

    fclose(fp);
    return data;
}

void *ProjectFile::getData_ZFS(PFEntry *entry, size_t & size)
{
    FILE * fp = m_paths[entry->path].zfs_fp;

    if (fp == NULL) {
        return(NULL);
    }

    size = entry->size;
    char *  data = (char *) malloc(size);
    if (data == NULL)
    {
        sprintf(m_error_string, "Could not malloc data for record %s\n",
                entry->rname);
        return NULL;
    }

    fseek(fp, entry->offset, SEEK_SET);
    if (fread(data, size, 1, fp) < 1) {
        sprintf(m_error_string, "Could not read record \"%s\" from \"%s\"" ,
                entry->rname, m_paths[entry->path].dos_path);
        free(data);
        return NULL;
    }

    return data;
}

void *ProjectFile::getData_ZMS(PFEntry *entry, size_t & size)
{
    size        = entry->size;

    return m_paths[entry->path].zms_start + entry->offset;
}

void *ProjectFile::getData_ZMS(PFEntry *entry, size_t & size,
                               HANDLE *hFileMap, size_t & offset)
{
#ifdef WIN32
    *hFileMap   = m_paths[entry->path].zms_hm;
#endif
    offset      = entry->offset;

    return getData_ZMS(entry, size);
}

void *ProjectFile::getData(char const * rname, size_t & size, C3DIR dir)
{
    PFEntry *   entry = findRecord(rname);

    if (!entry)
    {
        sprintf(m_error_string, "Couldn't find record \"%s\"", rname);
        size = 0;
        return NULL;
    }

    void * data = NULL;
    switch (m_paths[entry->path].type)
    {
    default:                return NULL;
    case PRJFILE_PATH_DOS:  data = getData_DOS(entry, size, dir); break;
    case PRJFILE_PATH_ZFS:  data = getData_ZFS(entry, size);      break;
    case PRJFILE_PATH_ZMS:  data = getData_ZMS(entry, size);      break;
    }

#if !CTP2_LITTLE_ENDIAN_HOST
    // RIM images (little-endian, 16-bit pixels) in the machine's order, in
    // the one place all their readers get them from. Each call reads a fresh
    // copy (DOS and ZFS paths; the mapped ZMS path is not used on big-endian
    // builds: addPath).
    if (data && size >= sizeof(RIMHeader))
    {
        size_t const len = strlen(rname);
        if (len > 4 && stricmp(rname + len - 4, ".rim") == 0)
        {
            RIMHeader * rhead = static_cast<RIMHeader *>(data);
            ctp2_swap_le(rhead->version);
            ctp2_swap_le(rhead->width);
            ctp2_swap_le(rhead->height);
            ctp2_swap_le(rhead->pitch);
            ctp2_swap_le(rhead->flags);
            ctp2_swap_le16_array(static_cast<uint8 *>(data) + sizeof(RIMHeader),
                                 (size - sizeof(RIMHeader)) / 2);
        }
    }
#endif

    return data;
}

void *ProjectFile::getData(char const * rname, size_t & size,
                           HANDLE * hFileMap, size_t & offset)
{
    PFEntry *entry = findRecord(rname);

    if (!entry)
    {
        sprintf(m_error_string, "Couldn't find record \"%s\"", rname);
        size = 0;
        *hFileMap = NULL;
        offset = 0;
        return NULL;
    }

    if (m_paths[entry->path].type != PRJFILE_PATH_ZMS)
    {
        sprintf(m_error_string, "Record \"%s\" is not file mapped", rname);
        size = 0;
        *hFileMap = NULL;
        offset = 0;
        return NULL;
    }

    return getData_ZMS(entry, size, hFileMap, offset);
}

void ProjectFile::freeData(void *ptr)
{
    for (int i = 0; i < m_num_paths; i++)
    {
        if ((m_paths[i].type == PRJFILE_PATH_ZMS) &&
            (ptr >= m_paths[i].zms_start) &&
            (ptr < m_paths[i].zms_end))
        {
            return;
        }
    }

    if (ptr)
        free(ptr);
}

int ProjectFile::readDOSdir(long path, PFEntry *table)
{
    char tmp[256];
    sprintf(tmp, "%s%s*.*", m_paths[path].dos_path, FILE_SEP);

#ifdef WIN32
    WIN32_FIND_DATA dirent;
    HANDLE          dirhandle = FindFirstFile(tmp, &dirent);
    if (dirhandle == INVALID_HANDLE_VALUE)
#else
    DIR *dir;
    struct dirent *dent = 0;
    struct stat tmpstat;
    dir = opendir(CI_FixName(m_paths[path].dos_path));
    if (!dir)
#endif
    {
        sprintf(m_error_string, "Couldn't find \"%s\"", tmp);
        return(0);
    }

    int count = 0;
    do {
#ifdef WIN32
        MBCHAR *name = dirent.cFileName;
#else
	dent = readdir(dir);
	if (!dent)
            continue;
	MBCHAR *name = dent->d_name;
#endif
        if (name[0] == '.')
            continue;

#ifdef WIN32
        if (!(dirent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
#else
        snprintf(tmp, sizeof(tmp), "%s%s%s", m_paths[path].dos_path, FILE_SEP, name);
        if (stat(tmp, &tmpstat) == -1)
            continue;

        if (!S_ISDIR(tmpstat.st_mode)) {
#endif
            if ((table) && (!exists(name))) {
                strcasecpy(table->rname, name);
                table->offset = 0;
                table->size = 0;
                table->path = path;
                table++;
                count++;
            } else if (!table) {
                count++;
            }
        }
#ifdef WIN32
    } while (FindNextFile(dirhandle, &dirent) == TRUE);
    FindClose(dirhandle);
#else
    } while (dent);
    closedir(dir);
#endif
    return(count);
}

int ProjectFile::addPath_DOS(char const * path)
{
    int pathnum = m_num_paths;

    m_paths[pathnum].type = PRJFILE_PATH_DOS;
    strcpy(m_paths[pathnum].dos_path, path);

    int count = readDOSdir(pathnum, NULL);
    if (count == 0)
    {
        return(1);
    }
    m_num_paths++;

    PFEntry *   tmpList = new PFEntry[count];
    mergeEntries(tmpList, readDOSdir(pathnum, tmpList));
    delete [] tmpList;

    return(1);
}

int ProjectFile::verify_ZFS_header(ZFS_FHEADER *header)
{
	if ((header->filetag[0] != 'Z') ||
		(header->filetag[1] != 'F') ||
		(header->filetag[2] != 'S') ||
		(header->filetag[3] != '3') ||
		(header->version != 1) ||
		(header->max_recordname_length != MAX_RECORDNAME_LENGTH) ||
		(header->max_entries_per_table != MAX_ENTRIES_PER_TABLE)) {
        return(0);
	}

    return(1);
}

void ProjectFile::read_ZFS_dtable(int pathnum, ZFS_DTABLE *dtable,
                                 PFEntry **tlp, long *rcount)
{
    for
    (
        int i = 0;
        (i<MAX_ENTRIES_PER_TABLE) && (dtable->rentry[i].rname[0]);
        i++
    )
    {
        if (!(dtable->rentry[i].flags & ZFSFLAG_DELETED)) {
            ZFS_RENTRY *rentry = dtable->rentry + i;
            if (!exists(rentry->rname)) {
                strcasecpy((*tlp)->rname, rentry->rname);
                (*tlp)->offset = rentry->offset;
                (*tlp)->size = rentry->size;
                (*tlp)->path = pathnum;
                (*tlp)++;
                (*rcount)++;
            }
        }
    }
}

int ProjectFile::addPath_ZFS(char const * path)
{
    FILE * fp = fopen(path, "rb");
	if (fp == NULL) {
		sprintf(m_error_string, "Could not open file \"%s\"", path);
		return(0);
	}
#if defined(CTP2_AMIGA_BUILD)
    // libnix reads an unbuffered stream one byte per DOS call.
    setvbuf(fp, NULL, _IOFBF, CTP2_AMIGA_STDIO_BUFFER);
#else
    setvbuf(fp, NULL, _IONBF, 0);
#endif

    int pathnum = m_num_paths;

    m_paths[pathnum].type   = PRJFILE_PATH_ZFS;
    strcpy(m_paths[pathnum].dos_path, path);
    m_paths[pathnum].zfs_fp = fp;

    m_num_paths++;


 	fseek(fp, 0, SEEK_SET);

    ZFS_FHEADER header;
	if (fread(&header, sizeof(ZFS_FHEADER), 1, fp) < 1) {
		sprintf(m_error_string, "Could not read header of file \"%s\"", path);
		return(0);
	}
    SwapHeaderLE(header);

    if (!verify_ZFS_header(&header)) {
		sprintf(m_error_string, "Invalid header in file \"%s\"", path);
		return(0);
	}

    int         count   = header.num_rentries;
    PFEntry *   tmpList = new PFEntry[count];
    PFEntry *   tlp     = tmpList;
    long        dhead   = header.dtable_head;
    long        rcount  = 0;
    ZFS_DTABLE  dtable;
    do
    {
        fseek(fp, dhead, SEEK_SET);

        if (fread(&dtable, sizeof(ZFS_DTABLE), 1, fp) < 1) {
            sprintf(m_error_string, "File \"%s\" is corrupt", path);
            delete [] tmpList;
            return(0);
        }
        SwapDTableLE(dtable);

        read_ZFS_dtable(pathnum, &dtable, &tlp, &rcount);
    }
    while ((dhead = dtable.next_dtable) != 0);

    mergeEntries(tmpList, rcount);

    delete [] tmpList;

    return(1);
}

int ProjectFile::addPath_ZMS(char const * path)
{
    long            fsize;
    int             pathnum = m_num_paths;
    unsigned char * fbase   = reinterpret_cast<unsigned char *>
#ifdef WIN32
        (mapFile(path, &fsize,
                 &(m_paths[pathnum].zms_hm),
                 &(m_paths[pathnum].zms_hf)
                )
        );
#else
        (mapFile(path, &fsize, m_paths[pathnum])
        );
#endif
if (fbase == NULL) {
        sprintf(m_error_string, "Could not open file \"%s\"", path);
        return(0);
    }

    m_paths[pathnum].type       = PRJFILE_PATH_ZMS;
    m_paths[pathnum].zms_start  = fbase;
    m_paths[pathnum].zms_end    = fbase + fsize;
    strcpy(m_paths[pathnum].dos_path, path);
    m_num_paths++;

    ZFS_FHEADER * header = (ZFS_FHEADER *) fbase;
    if (!verify_ZFS_header(header)) {
		sprintf(m_error_string, "Invalid header in file \"%s\"", path);
		return(0);
	}

    int             count   = header->num_rentries;
    PFEntry *       tmpList = new PFEntry[count];
    PFEntry *       tlp     = tmpList;
    long            rcount  = 0;
    long            dhead   = header->dtable_head;
    ZFS_DTABLE *    dtable;
    do
    {
        dtable = (ZFS_DTABLE *)(fbase + dhead);
        read_ZFS_dtable(pathnum, dtable, &tlp, &rcount);
    }
    while ((dhead = dtable->next_dtable) != 0);

    mergeEntries(tmpList, rcount);

    delete [] tmpList;

    return(1);
}

int ProjectFile::addPath(char const * path, bool use_filemapping)
{

    if (m_num_paths >= MAX_PRJFILE_PATHS) {
        sprintf(m_error_string, "Couldn't add \"%s\", too many paths", path);
        return(0);
    }

    char const * ext = strrchr(path, '.');

    if (ext)
    {
        ext++;

        if ((toupper(ext[0]) == 'Z') &&
            (toupper(ext[1]) == 'F') &&
            (toupper(ext[2]) == 'S') &&
            (toupper(ext[3]) == 0))
        {
#if defined(CTP2_AMIGA_BUILD)
            // Read entries from the file as needed rather than the whole
            // archive into memory (no mmap on AmigaOS).
            use_filemapping = false;
#endif
            return (use_filemapping) ? addPath_ZMS(path) : addPath_ZFS(path);
        }
    }

    return addPath_DOS(path);
}

bool ProjectFile::IsReported(char const * a_FileName) const
{
    return m_Reported.find(std::string(a_FileName)) != m_Reported.end();
}

void ProjectFile::MarkReported(char const * a_FileName)
{
    m_Reported.insert(std::string(a_FileName));
}
