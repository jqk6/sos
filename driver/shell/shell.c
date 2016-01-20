#include <libc.h>

#include "uart.h"
#include "log.h"
#include "mmio.h"

#define SHELL_ARGS_MAX  (5)

u32 argc;
char *argv[SHELL_ARGS_MAX] = {NULL};

struct shell_cmd_info {
    char *name;
    func_0 func;
    char *desc;
};

s32 cmd_read();
s32 cmd_write();
s32 cmd_exec();
s32 cmd_dump();
s32 cmd_help();

struct shell_cmd_info ci[] = {
    { .name = "r",    .func = cmd_read,  .desc = "r [addr]                  read any addr"   },
    { .name = "w",    .func = cmd_write, .desc = "w [addr] [data]           write any addr"  },
    { .name = "x",    .func = cmd_exec,  .desc = "x [addr]                  execute any addr"},
    { .name = "dump", .func = cmd_dump,  .desc = "dump [addr] [word_num]    dump any addr"   },
    { .name = "help", .func = cmd_help,  .desc = "help                      print cmd info"  },
};


s32 cmd_read()
{
    u32 addr;
    u32 data;

    addr = atoi(argv[1]);
    data = readl(addr);
    uart_printf("[0x%x]: 0x%x\n", addr, data);
    return 0;
}

s32 cmd_write()
{
    u32 addr, data;
    u32 old_data, new_data;

    addr = atoi(argv[1]);
    data = atoi(argv[2]);

    uart_printf("[0x%x]\n", addr);
    uart_printf("[0x%x]\n", data);
#if 0
    old_data = readl(addr);
    writel(addr, data);
    new_data = readl(addr);
    uart_printf("[0x%x]: [0x%x]->[0x%x] [0x%x]\n", addr, old_data, data, new_data);
#endif
    return 0;
}

s32 cmd_exec()
{
    s32 ret;
    u32 addr, para1, para2, para3, para4;
    func_4 func;

    addr  = atoi(argv[1]);
    para1 = atoi(argv[2]);
    para2 = atoi(argv[3]);
    para3 = atoi(argv[4]);
    para4 = atoi(argv[5]);

    func = (func_4)addr;

    ret = func(para1, para2, para3, para4);
    uart_printf("execute 0x%x (0x%x 0x%x 0x%x 0x%x)return 0x%x\n", addr, para1, para2, para3, para4, ret);
    return ret;
}

s32 cmd_dump()
{
    u32 i;
    u32 *p;
    u32 addr, word_nr;
    func_4 func;

    addr    = atoi(argv[1]);
    word_nr = atoi(argv[2]);
    p       = (u32*)addr;

    for(i=0;i<word_nr;i++) {
        uart_printf("[0x%x]: 0x%x\r\n", &p[i], p[i]);
    }   

    return 0;
}

s32 cmd_help()
{
    u32 i;
    for(i=0; i<(sizeof(ci)/sizeof(ci[0])); i++) {
        uart_printf("%s:\t\t\t%s\n", ci[i].name, ci[i].desc);
    }
    return 0;
}

s32 parse_cmd(char *cmd)
{
    u32 i,j;
    
    memset(argv, 0, SHELL_ARGS_MAX*sizeof(argv[0]));

    /* first, set the argv */
    j = 0;
    for(i=0;cmd[i]!='\0';) {

        while(cmd[i] == ' ') {
            i++;
        }
        
        argv[j++] = &cmd[i];
        if (j == SHELL_ARGS_MAX) {
            return 0;
        }

        while(cmd[i] != ' ' && cmd[i] != '\0') {
            i++;
        }

    }

    /* replace all ' ' to '\0' */
    for(i=0;cmd[i]!='\0';i++) {
        if (cmd[i] == ' ') {
            cmd[i] = '\0';
        }
    }
}

s32 get_cmd_index(char *cmd)
{
    u32 i;
    for(i=0; i<(sizeof(ci)/sizeof(ci[0])); i++) {
        if (strcmp(ci[i].name, cmd) == 0) {
            return i;
        }
    }
    return -1;
}

s32 shell(char *cmd)
{
    u32 i, len;
    u32 para1, para2;
    s32 ret;

    if ((len = strlen(cmd)) == 0) {
        return 0;
    }

    uart_printf("\n");   

    parse_cmd(cmd);
    for(i=0;i<len;i++) {
        uart_printf("[0x%x]: %x\n", &cmd[i], cmd[i]);
    }

    for(i=0;i<SHELL_ARGS_MAX;i++) {
        uart_printf("argv[%d]: 0x%x [%s]\n", i, argv[i], argv[i]);
    }

    if ((i=get_cmd_index(argv[0])) == -1) {
        uart_printf("illegal cmd [%s] \n", argv[0]);
        return -1;
    }

    ret = ci[i].func();
    uart_printf("return 0x%x\n", ret);
}
