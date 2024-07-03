#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

int token;           // 当前词token
char *src, *old_src; // 代码string
int poolsize;        // 用户进程内存
int line;            // line number

int *text,     // 代码段
    *old_text, // for dump text segment
    *stack;    // 栈段
char *data;    // 数据段

int *pc, *bp, *sp, ax, cycle; // 寄存器声明
/**
 * 1. `PC` 程序计数器，它存放的是一个内存地址，该地址中存放着 **下一条** 要执行的计算机指令。
    2. `SP` 指针寄存器，永远指向当前的栈顶。注意的是由于栈是位于高地址并向低地址增长的，所以入栈时 `SP` 的值减小。
    3. `BP` 基址指针。也是用于指向栈的某些位置，在调用函数时会使用到它。
    4. `AX` 通用寄存器，我们的虚拟机中，它用于存放一条指令执行后的结果。
    +------------------+
    |    stack   |     |      high address
    |    ...     v     |
    |                  |
    |                  |
    |                  |
    |                  |
    +------------------+
    | data segment     |
    +------------------+
    | text segment     |      low address
    +------------------+
 */

// 要支持的指令
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

void next() // 语法分析器
{
    token = *src++;
    return;
}

void expression(int level) // 词法分析器
{
    // do nothing
}

void program()
{
    next();
    while (token > 0)
    {
        printf("token is: %c\n", token);
        next();
    }
}

int eval() // 目标执行器
{
    int op, *tmp;

    while (1)
    {   op = *pc++; // get next operation code
        if (op == IMM) {ax = *pc++;} // 加载立即数
        else if (op == LC) {ax = *(char *)ax;} // ax存放地址
        else if (op == LI) {ax = *(int *)ax;} // ax存放int
        else if (op == SC) {ax = *(char *)*sp++ = ax;} // ax内容(字符)放入栈顶
        else if (op == SI) {*(int *)*sp++ = ax;} // ax内容(int)放入栈顶
        else if (op == PUSH) {*--sp = ax;} // ax压栈
        else if (op == JMP)  {pc = (int *)*pc;} // 跳转
        else if (op == JZ)   {pc = ax ? pc + 1 : (int *)*pc;} // jump if ax is zero
        else if (op == JNZ)  {pc = ax ? (int *)*pc : pc + 1;} // jump if ax is zero

        // 函数调用指令
        else if (op == CALL) {*--sp = (int)(pc+1); pc = (int *)*pc;} // call指令
        else if (op == ENT)  {*--sp = (int)bp; bp = sp; sp = sp - *pc++;} // 保存栈指针,栈上保留空间存放局部变量
        else if (op == ADJ)  {sp = sp + *pc++;} // 调用子函数时压入栈中的数据清除
        else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;} // ret,恢复栈顶和指针
        else if (op == LEA)  {ax = (int)(bp + *pc++);} // 为了得到函数参数,计算偏移地址

        //运算符指令
        else if (op == OR)  ax = *sp++ | ax;
        else if (op == XOR) ax = *sp++ ^ ax;
        else if (op == AND) ax = *sp++ & ax;
        else if (op == EQ)  ax = *sp++ == ax;
        else if (op == NE)  ax = *sp++ != ax;
        else if (op == LT)  ax = *sp++ < ax;
        else if (op == LE)  ax = *sp++ <= ax;
        else if (op == GT)  ax = *sp++ >  ax;
        else if (op == GE)  ax = *sp++ >= ax;
        else if (op == SHL) ax = *sp++ << ax;
        else if (op == SHR) ax = *sp++ >> ax;
        else if (op == ADD) ax = *sp++ + ax;
        else if (op == SUB) ax = *sp++ - ax;
        else if (op == MUL) ax = *sp++ * ax;
        else if (op == DIV) ax = *sp++ / ax;
        else if (op == MOD) ax = *sp++ % ax;

        //内置函数
        else if (op == EXIT) { printf("exit(%d)", *sp); return *sp;}
        else if (op == OPEN) { ax = open((char *)sp[1], sp[0]); }
        else if (op == CLOS) { ax = close(*sp);}
        else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
        else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }
        else if (op == MALC) { ax = (int)malloc(*sp);}
        else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp);}
        else if (op == MCMP) { ax = memcmp((char *)sp[2], (char *)sp[1], *sp);}

        //判断
        else {
        printf("unknown instruction:%d\n", op);
        return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
        #define int long long // work with 64bit target
    int i, fd;

    argc--;
    argv++;

    poolsize = 256 * 1024; // arbitrary size
    line = 1;

    if ((fd = open(*argv, 0)) < 0)
    {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    if (!(src = old_src = malloc(poolsize)))
    {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }

    // read the source file
    if ((i = read(fd, src, poolsize - 1)) <= 0)
    {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0; // add EOF character
    close(fd);

    // 为代码段/数据段/栈段分配内存
    if (!(text = old_text = malloc(poolsize)))
    {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize)))
    {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize)))
    {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }

    memset(text, 0, poolsize); // 将内存重置为0
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);

    bp = sp = (int *)((int)stack + poolsize); // 置顶
    ax = 0;                                   // 初始化

// 测试
    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;
    pc = text;
    program();
    return eval();
}