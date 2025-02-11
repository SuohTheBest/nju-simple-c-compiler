#include "generator.h"
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#define next_token(line) strtok(line, " ")

ir_list_node *ir_list;
var_list_node *var_list;
var_list_node *param_list;
FILE *out_put_file;
int reg_free_flag[10];

char *my_strdup(const char *origin) {
    char *ans = malloc(strlen(origin) + 1);
    strcpy(ans, origin);
    return ans;
}

void generate_mips(char *midCode, char *output_file) {
    out_put_file = fopen(output_file, "w+");
    if (out_put_file == NULL) assert(0);
    generate_preCode();
    split_ir(midCode);
    char op[64];
    ir_list_node *p = ir_list;
    while (p != NULL) {
        sscanf(p->line, "%s", op);
        if (strcmp(op, "FUNCTION") == 0) {
            scan_local_var(p);
            p = generate_function(p);
            continue;
        }
        p = p->next;
    }
    fclose(out_put_file);
}

void generate_preCode() {
    fprintf(out_put_file, ".data\n"
                          "_prompt: .asciiz \"Enter an integer:\"\n"
                          "_ret: .asciiz \"\\n\"\n"
                          ".globl main\n"
                          ".text\n"
                          "read:\n"
                          "li      $v0, 4\n"
                          "la      $a0, _prompt\n"
                          "syscall\n"
                          "li      $v0, 5\n"
                          "syscall\n"
                          "jr      $ra\n\n"

                          "write:\n"
                          "li      $v0, 1\n"
                          "syscall\n"
                          "li      $v0, 4\n"
                          "la      $a0, _ret\n"
                          "syscall\n"
                          "move    $v0, $0\n"
                          "jr      $ra\n\n");
}

void split_ir(char *midCode) {
    ir_list_node *p = ir_list = calloc(1, sizeof(ir_list_node));
    p->line = "head_node";
    char *line = strtok(midCode, "\n");
    while (line != NULL) {
        p->next = calloc(1, sizeof(ir_list_node));
        p = p->next;
        p->line = line;
        line = strtok(NULL, "\n");
    }
    p->next = NULL;
}

void scan_local_var(ir_list_node *code) {
    char *line_str = my_strdup(code->line);
    char *token = next_token(line_str);
    token = next_token(NULL); // func_name
    var_list = calloc(1, sizeof(var_list_node));
    var_list->name = my_strdup(token);
    var_list->next = NULL;
    var_list->size = 8;
    param_list = calloc(1, sizeof(var_list_node));
    param_list->name = "head_node";
    param_list->next = NULL;
    param_list->size = 0;
    code = code->next;
    while (code != NULL) {
        line_str = my_strdup(code->line);
        token = next_token(line_str);
        if (strcmp(token, "FUNCTION") == 0) break;
        if (strcmp(token, "PARAM") == 0) {
            token = next_token(NULL);
            var_add_tail(param_list, 4, token);
        } else if (strcmp(token, "DEC") == 0) {
            token = next_token(NULL);
            int size = atoi(next_token(NULL));
            var_list->size += var_add_tail(var_list, size, token);
        } else if (strcmp(token, "READ") == 0) {
            token = next_token(NULL);
            var_list->size += var_add_tail(var_list, 4, token);
        } else if (strcmp(token, "ARG") == 0) {
            var_list->size += 4;
        } else if (strcmp(next_token(NULL), ":=") == 0) {
            var_list->size += var_add_tail(var_list, 4, token);
        }
        code = code->next;
    }
}

int var_add_tail(var_list_node *head, int size, char *name) {
    if (name[0] == '*') return 0;
    var_list_node *p = head;
    while (p->next != NULL) {
        if (strcmp(p->next->name, name) == 0) return 0;
        p = p->next;
    }
    p->next = calloc(1, sizeof(var_list_node));
    p->next->size = size;
    p->next->name = my_strdup(name);
    p->next->next = NULL;
    return size;
}

ir_list_node *generate_function(ir_list_node *code) {
    if (strcmp(var_list->name, "main") != 0)
        fprintf(out_put_file, "fun_%s:\n", var_list->name);
    else
        fprintf(out_put_file, "%s:\n", var_list->name);
#ifdef _DEBUG
    fprintf(out_put_file, "#################\n"
                          "# function init #\n"
                          "#################\n");
#endif
    int frame_size = var_list->size;
    fprintf(out_put_file, "addi    $sp, $sp, -%d\n"
                          "sw      $ra, %d($sp)\n"
                          "sw      $fp, %d($sp)\n"
                          "addi    $fp, $sp, %d\n",
            frame_size, frame_size - 4, frame_size - 8, frame_size);
    code = code->next;
    while (code != NULL) {
        char *line_str = my_strdup(code->line);
#ifdef _DEBUG
        fprintf(out_put_file, "################################\n"
                              "# code: %s #\n"
                              "################################\n",
                line_str);
#endif
        char *token = next_token(line_str);
        if (strcmp(token, "FUNCTION") == 0) break;
        if (strcmp(token, "PARAM") == 0 || strcmp(token, "DEC") == 0) {
            code = code->next;
            continue;
        }
        if (strcmp(token, "LABEL") == 0) {
            token = next_token(NULL);
            fprintf(out_put_file, "%s:\n", token);
        } else if (strcmp(token, "GOTO") == 0) {
            token = next_token(NULL);
            fprintf(out_put_file, "j       %s\n", token);
        } else if (strcmp(token, "RETURN") == 0) {
            token = next_token(NULL);
            int val_reg = reg(token);
            fprintf(out_put_file, "move    $v0, $t%d\n",
                    val_reg);
            re_reg(token, val_reg, 0);
            fprintf(out_put_file, "lw      $ra, %d($sp)\n"
                                  "lw      $fp, %d($sp)\n"
                                  "addi    $sp, $sp, %d\n"
                                  "jr      $ra\n",
                    frame_size - 4, frame_size - 8, frame_size);
        } else if (strcmp(token, "IF") == 0) {
            generate_if(code);
        } else if (strcmp(token, "READ") == 0) {
            fprintf(out_put_file, "jal     read\n");
            token = next_token(NULL);
            int val_reg = reg(token);
            fprintf(out_put_file, "move    $t%d, $v0\n",
                    val_reg);
            re_reg(token, val_reg, 1);
        } else if (strcmp(token, "WRITE") == 0) {
            token = next_token(NULL);
            int val_reg = reg(token);
            fprintf(out_put_file, "move    $a0, $t%d\n",
                    val_reg);
            re_reg(token, val_reg, 0);
            fprintf(out_put_file, "jal     write\n");
        } else if (strcmp(token, "ARG") == 0) {
            code = generate_funcall(code);
        } else if (strcmp(next_token(NULL), ":=") == 0) {
            generate_assign(code);
        }
        code = code->next;
    }
    fprintf(out_put_file, "\n");
    return code;
}

void generate_assign(ir_list_node *code) {
    char *line_str = my_strdup(code->line);
    char *token = next_token(line_str);
    char *var0 = token;
    token = next_token(NULL);
    token = next_token(NULL);
    if (strcmp(token, "CALL") == 0) {
        generate_funcall(code);
        return;
    }
    if (strcmp(var0, "t_") == 0) return;
    char *var1 = token;
    token = next_token(NULL);
    if (token == NULL) {
        if (var0[0] == '*') {
            var0 = var0 + 1;
            int reg0 = reg(var0);
            int reg1 = reg(var1);
            fprintf(out_put_file, "sw      $t%d, 0($t%d)\n",
                    reg1, reg0);
            re_reg(var0, reg0, 0);
            re_reg(var1, reg1, 0);
            return;
        } else {
            int reg0 = reg(var0);
            int reg1 = reg(var1);
            fprintf(out_put_file, "move    $t%d, $t%d\n",
                    reg0, reg1);
            re_reg(var0, reg0, 1);
            re_reg(var1, reg1, 0);
            return;
        }
    }
    char op = token[0];
    token = next_token(NULL);
    char *var2 = token;
    int reg0 = reg(var0);
    int reg1 = reg(var1);
    int reg2 = reg(var2);
    switch (op) {
    case '+':
        fprintf(out_put_file, "add     $t%d, $t%d, $t%d\n",
                reg0, reg1, reg2);
        break;
    case '-':
        fprintf(out_put_file, "sub     $t%d, $t%d, $t%d\n",
                reg0, reg1, reg2);
        break;
    case '*':
        fprintf(out_put_file, "mul     $t%d, $t%d, $t%d\n",
                reg0, reg1, reg2);
        break;
    case '/':
        fprintf(out_put_file, "div     $t%d, $t%d\n"
                              "mflo    $t%d\n",
                reg1, reg2, reg0);
        break;
    default:
        break;
    }
    re_reg(var0, reg0, 1);
    re_reg(var1, reg1, 0);
    re_reg(var2, reg2, 0);
}

int reg(char *name) {
    int new_reg = 0;
    while (reg_free_flag[new_reg] == 1) new_reg++;
    reg_free_flag[new_reg] = 1;
    if (name[0] == '#') {
        fprintf(out_put_file, "li      $t%d, %s\n",
                new_reg, name + 1);
        return new_reg;
    }
    char *var_name = name;
    if (name[0] == '*' || name[0] == '&')
        var_name = var_name + 1;
    int offset = var_offset(var_name);
    if (offset == -1) {
        fprintf(out_put_file, "li      $t%d, 0\n",
                new_reg);
    } else if (name[0] == '*') {
        fprintf(out_put_file, "lw      $t%d, %d($fp)\n"
                              "lw      $t%d, 0($t%d)\n",
                new_reg, offset, new_reg, new_reg);
    } else if (name[0] == '&') {
        fprintf(out_put_file, "addi    $t%d, $fp, %d\n",
                new_reg, offset);
    } else {
        fprintf(out_put_file, "lw      $t%d, %d($fp)\n",
                new_reg, offset);
    }
    return new_reg;
}

int var_offset(char *var_name) {
    int offset = 0;
    for (var_list_node *p = param_list; p != NULL; p = p->next) {
        if (strcmp(p->name, var_name) == 0)
            return offset;
        offset += p->size;
    }
    offset = -8;
    for (var_list_node *p = var_list->next; p != NULL; p = p->next) {
        offset -= p->size;
        if (strcmp(p->name, var_name) == 0)
            return offset;
    }
    return -1;
}

void re_reg(char *name, int reg, int need_write_back) {
    reg_free_flag[reg] = 0;
    if (!need_write_back || name[0] == '*' || name[0] == '&' || name[0] == '#')
        return;
    int offset = var_offset(name);
    if (offset == -1) return;
    fprintf(out_put_file, "sw      $t%d, %d($fp)\n",
            reg, offset);
}

ir_list_node *generate_funcall(ir_list_node *code) {
    // count args numbers
    int argCnt = 0;
    ir_list_node *cur_code = code;
    while (cur_code != NULL) {
        char *line_str = my_strdup(cur_code->line);
        char *token = next_token(line_str);
        if (strcmp(token, "ARG") != 0) break;
        cur_code = cur_code->next;
        argCnt++;
    }
    // insert inversely
    while (code != NULL) {
        char *line_str = my_strdup(code->line);
        char *token = next_token(line_str);
        if (strcmp(token, "ARG") != 0) break;
        token = next_token(NULL);
        int var_arg = reg(token);
        fprintf(out_put_file, "sw      $t%d, %d($sp)\n",
                var_arg, (--argCnt) * 4);
        re_reg(token, var_arg, 0);
        code = code->next;
    }
    char *line_str = my_strdup(code->line);
    char *token = next_token(line_str);
    char *val0 = token;
    token = next_token(NULL);
    token = next_token(NULL);
    token = next_token(NULL);
    char *fun_name = token;
    if (strcmp(fun_name, "main") == 0)
        fprintf(out_put_file, "jal     %s\n",
                fun_name);
    else
        fprintf(out_put_file, "jal     fun_%s\n",
                fun_name);
    int reg0 = reg(val0);
    fprintf(out_put_file, "move    $t%d, $v0\n",
            reg0);
    re_reg(val0, reg0, 1);
    return code;
}

void generate_if(ir_list_node *code) {
    char *line_str = my_strdup(code->line);
    char *token = next_token(line_str);
    char *var_x = next_token(NULL);
    char *op = next_token(NULL);
    char *var_y = next_token(NULL);
    token = next_token(NULL);
    char *lable_z = next_token(NULL);
    int reg_x = reg(var_x);
    int reg_y = reg(var_y);
    if (strcmp(op, "==") == 0) {
        fprintf(out_put_file, "beq     $t%d, $t%d, %s\n",
                reg_x, reg_y, lable_z);
    } else if (strcmp(op, "!=") == 0) {
        fprintf(out_put_file, "bne     $t%d, $t%d, %s\n",
                reg_x, reg_y, lable_z);
    } else if (strcmp(op, ">") == 0) {
        fprintf(out_put_file, "bgt     $t%d, $t%d, %s\n",
                reg_x, reg_y, lable_z);
    } else if (strcmp(op, "<") == 0) {
        fprintf(out_put_file, "blt     $t%d, $t%d, %s\n",
                reg_x, reg_y, lable_z);
    } else if (strcmp(op, ">=") == 0) {
        fprintf(out_put_file, "bge     $t%d, $t%d, %s\n",
                reg_x, reg_y, lable_z);
    } else if (strcmp(op, "<=") == 0) {
        fprintf(out_put_file, "ble     $t%d, $t%d, %s\n",
                reg_x, reg_y, lable_z);
    } else {
        assert(0);
    }
    re_reg(var_x, reg_x, 0);
    re_reg(var_y, reg_y, 0);
}