#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>

#define MAX_PATH_LENGTH 32   // to access /proc/pid/maps
#define MAX_LINE_LENGTH 256  // for each line in maps
#define MAX_FUNC_LENGTH 120  // symbol name associated to a function

int verbose=0;

void *pt_start_text, *pt_end_text;

void load_map();
void load_elf(char *);

extern void load_symbols(char *prog_path)
{
    load_map();
    load_elf(prog_path);
}

void load_map()
{
    FILE *fmaps;
    char line[MAX_LINE_LENGTH];

    char proc_maps[MAX_PATH_LENGTH];
    char *start_address, *end_address;


    /* First : find the process address in virtual memory */

    /* retrieve PID of current process */
    pid_t pid = getpid();

    // Create the string representing /proc/<pid>/maps
    snprintf(proc_maps, sizeof(proc_maps), "/proc/%d/maps", pid);

    // Open file in read mode
    fmaps = fopen(proc_maps, "r");

    if (fmaps == NULL)
    {
        printf("File '%s' could not be opened.\n", proc_maps);
        exit(-1);
    }

    while (fgets(line, sizeof(line), fmaps) != NULL)
    {
        if (verbose) printf("%s", line);
        // need to get the address of the first segment (not the fist code segment, as ELF symbol table offset from there)
        if (strstr(line, " r--p ") != NULL)
        {
            char* token = strtok(line, "-");
            if (token != NULL)
                start_address = token;
            else
            {
                printf("Unable to extract start address of main program.\n");
                exit(-2);
            }
            token = strtok(NULL, " "); // Move to the next token after the hyphen
            if (token != NULL)
                end_address = token;
            else
            {
                printf("Unable to extract end address of main program.\n");
                exit(-3);
            }
            pt_start_text = (void *)strtoul(start_address, NULL, 16);
            pt_end_text = (void *)strtoul(end_address, NULL, 16);
            if (verbose) printf("text start at %p and stops at %p\n",pt_start_text,pt_end_text);
            break;
        }
    }

    fclose(fmaps);
}


struct func_symb
{
    void* pt;
    char func[MAX_FUNC_LENGTH];
};
struct func_symb *tab_symb=NULL ;

void load_elf(char *prog_path)
{
    if (verbose) printf("program path = %s\n",prog_path);

    /* Second : get function symbols from ELF */
    int fd;
    Elf *elf;
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    Elf_Data *data;
    int count = 0;

    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        fprintf(stderr, "Failed to initialize the libelf library.\n");
        exit(-4);
    }

    if ((fd = open(prog_path, O_RDONLY, 0)) < 0)
    {
        perror("open");
        exit(-5);
    }

    // 1st pass to get the number of entries

    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
    {
        fprintf(stderr, "Failed to open ELF file: %s\n", elf_errmsg(-1));
        exit(-6);
    }

    while ((scn = elf_nextscn(elf, scn)) != NULL)
    {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB)
        {
            data = elf_getdata(scn, NULL);
            if (data)
            {
                Elf32_Sym *symtab = (Elf32_Sym *)data->d_buf;
                int symbol_count = shdr.sh_size / shdr.sh_entsize;
                for (int i = 0; i < symbol_count; ++i)
                {
                    GElf_Sym sym;
                    gelf_getsym(data, i, &sym);
                    if ((ELF64_ST_TYPE(sym.st_info)==STT_FUNC) && (sym.st_size!=0))
                        count++;
                }
            }
        }
    }

    // 2nd pass to retrieve the functions addresses

    count++;
    tab_symb = malloc(count * sizeof(struct func_symb));

    // Check if memory allocation succeeded
    if (tab_symb == NULL) {
        fprintf(stderr, "Memory allocation failed. Exiting...\n");
        exit(-7);
    }
    struct func_symb *psymb=tab_symb;

    elf = elf_begin(fd, ELF_C_READ, NULL);

    while ((scn = elf_nextscn(elf, scn)) != NULL)
    {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB)
        {
            data = elf_getdata(scn, NULL);
            if (data)
            {
                Elf32_Sym *symtab = (Elf32_Sym *)data->d_buf;
                int symbol_count = shdr.sh_size / shdr.sh_entsize;
                for (int i = 0; i < symbol_count; ++i)
                {
                    GElf_Sym sym;
                    gelf_getsym(data, i, &sym);
                    if ((ELF64_ST_TYPE(sym.st_info)==STT_FUNC) && (sym.st_size!=0))
                    {
                        void *p=pt_start_text+(unsigned long)sym.st_value;
                        psymb->pt=p;
                        strncpy(psymb->func,elf_strptr(elf, shdr.sh_link, sym.st_name),MAX_FUNC_LENGTH);
                        psymb++;
                        if (verbose) printf("%s at 0x%lx (%p in process virtual memory)\n",
                               elf_strptr(elf, shdr.sh_link, sym.st_name),
                               (unsigned long)sym.st_value,
                               p);
                    }
                }
            }
        }
    }
    // end of table
    psymb->pt=NULL;

    elf_end(elf);
    close(fd);

}

char unknown[]="unknown";
char noinit[]="noinit";

char* func_lookup_by_address(void *) __attribute__((no_instrument_function));

char* func_lookup_by_address(void *f_add)
{
    struct func_symb *p=tab_symb;

    if (p==NULL)
        return noinit;

    while (p->pt != NULL)
    {
        if (f_add == p->pt)
        {
            return p->func;
        }
        p++;
    }
    return unknown;
}

/* same as previous lookup, except this time we try to guess in which function we are */

char* caller_lookup_by_address(void *) __attribute__((no_instrument_function));

char buf[1024];

char* caller_lookup_by_address(void *f_add)
{
    struct func_symb *p=tab_symb;

    if (p==NULL)
        return noinit;
    long min=1<<30;
    char* ret=unknown;

    while (p->pt != NULL)
    {
        if (f_add == p->pt)
            return p->func;
        else if (f_add>p->pt)
        {
            long offset=f_add-p->pt;
            if (offset<min)
            {
                min=offset;
                ret=p->func;
            }
        }
        p++;
    }
    if (ret==unknown)
        return ret;
    sprintf(buf,"%s + %ld",ret,min);
    return buf;
}


char     log_spaces[] = "                                                                                                                                                                                                                                      ";
__thread int      log_indent = 1;

void __cyg_profile_func_enter(void *this_fn, void *call_site)
                              __attribute__((no_instrument_function));

void __cyg_profile_func_enter (void *this_fn, void *call_site) {
    char *n_fn=func_lookup_by_address(this_fn);
    char *n_cl=caller_lookup_by_address(call_site);
    printf ("%.*s+ %p(%s) from %p(%s)\n",
            2 * log_indent, &log_spaces[0],this_fn,n_fn,call_site,n_cl);
    log_indent ++;
}


void __cyg_profile_func_exit(void *this_fn, void *call_site)
                             __attribute__((no_instrument_function));

void __cyg_profile_func_exit  (void *this_fn, void *call_site) {
    log_indent --;
    if (log_indent < 0) {
        printf("ERROR: internal: log_indent < 0\n");
        exit(-8);
    }
}
