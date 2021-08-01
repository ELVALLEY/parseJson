#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "leptjson.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h> /* errno, ERANGE */
#include <math.h> /* HUGE_VAL */
#include <string.h> /* memcpy */

//与当前c->json指向的字符进行断言比较, 如果通过, 那么c->json指针加一
#define EXPECT(c,ch) do{ assert((*(c->json)) == (ch)); c->json++; }while(0)
//将一个字节大小的数据存入动态空间中
#define PUTC(c, ch)  do{*(char*)lept_content_push(c, sizeof(char)) = ch;}while(0)
//发生错误时, 恢复动态空间顶部的索引, 并返回错误码
#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

#define PUTS(c, s, len)     memcpy(lept_content_push(c, len), s, len)

//定义了动态内存空间中的大小为256字节;
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif



static int lept_parse_value(lept_content* c, lept_value* v);//forward declare


/* whitespace = *(%x20 / %x09 / %x0A / %x0D) */
static void lept_parse_whiteSpace(lept_content* c){
    const char* p = c->json;
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'){
        p++;
    }
    c->json = p;//移动到指向value值的位置
}

static int lept_parse_null(lept_content* c, lept_value* v){
    assert(*(c->json) == 'n'); //断言判读
    c->json++;
    if(c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l' )
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}
static int lept_parse_false(lept_content* c, lept_value* v){
    assert(*(c->json) == 'f'); //断言判读
    c->json++;

    if(c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}
static int lept_parse_true(lept_content* c, lept_value* v){
    assert(*(c->json) == 't'); //断言判读
    c->json++;

    if(c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static double lept_parse_double(lept_content* c, lept_value* v){
    /*校验部分*/
    //将字符串中的数字转换为double,可包含正负号、小数点或E(e)来表示指数部分

    const char* p = c->json;
    /*验证正负号*/
    if(*p == '-') p++;
    /*验证整数*/
    if(*p == '0') {
        p++;
    }
    else{
        if( !('1' <= *p && *p <= '9') ) return LEPT_PARSE_INVALID_VALUE;

        for(p++; '0' <= *p && *p <= '9'; p++);
    }
    /*验证小数*/
    if(*p == '.'){
        p++;
        if( !('0' <= *p && *p <= '9') ) return LEPT_PARSE_INVALID_VALUE;
        for(p++; '0' <= *p && *p <= '9'; p++);
    }
    /*验证指数*/
    if(*p == 'e' || *p == 'E'){
        p++;
        if(*p == '+' || *p == '-')p++;
        if( !('0' <= *p && *p <= '9') ) return LEPT_PARSE_INVALID_VALUE;
        for(p++; '0' <= *p && *p <= '9'; p++);
    }

    errno = 0;
    v->u.n = strtod(c->json, NULL);
    //当strtod的结果产生的值太大而无法用其返回类型表示时，函数将返回此HUGE_VAL。
    //通过将 errno 设置为 ERANGE 来发出信号。<errno.h>
    //产生溢出时，函数的计算结果可能等于 HUGE_VAL，也可能等于 -HUGE_VAL。<math.h>
    if(errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL) ) 
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}


/*string解析部分*/
//解析 JSON 字符串时，因为在开始时不能知道字符串的长度，
//而又需要进行转义，所以需要一个临时缓冲区去存储解析后的结果
//为此实现了一个动态增长的堆栈，可以不断压入字符，最后一次性把整个字符串弹出，复制至新分配的内存之中
static void* lept_content_push(lept_content* c, size_t size){
    void* ret; 
    assert(size > 0);

    //当空间不足时进行重新分配内存
    if(c->top + size >= c->size){
        //c->size为0时初始化大小
        if(c->size == 0) c->size = LEPT_PARSE_STACK_INIT_SIZE; //初始化大小为256byte

        while(c->top + size >= c->size){
            c->size += c->size >> 1; /*大小右移动1位, 等同于c->size*1.5, 也就是每次扩大1.5倍*/
        }
        //初次申请空间, 或者重新分配空间大小;
        c->stack = (char*)realloc(c->stack, c->size);//c->statck初始为null,之后使用realloc重新分配
    }
    //返回当前空间中指向容器顶部的指针
    ret = c->stack + c->top;
    //容器顶部的位置索引
    c->top += size;
    return ret;
}

//从容器中取出字符
static void* lept_content_pop(lept_content* c, size_t size){
    assert(c->top >= size);
    //Return the pointer at the top of the container after taking out the character;
    return c->stack + (c->top -= size);
}

//解析4位十六进制整数为Unicode码点
static const char* lept_parse_hex4(const char* p , unsigned* u){
    int i;
    *u = 0;
    //将\uXXXX中的十六进制字符解析为数字, 并存储于u中;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - 'a' + 10;
        else return NULL;
    }
    return p;
}

//此函数为Unicode编码为UTF8的接口
//由于UTF8是可变字节编码, 根据不同的字节长度进行编码存储;
//通过Unicode码点计算UTF8的真实码点, 然后将真实码点存入缓冲区;
// (十六进制) |              (十进制)          |（二进制）
// --------------------+---------------------------------------------
// 0000 0000至0000 007F |   0到127            |  0xxxxxxx 
// 0000 0080至0000 07FF |   80到2047          |  110xxxxx 10xxxxxx
// 0000 0800至0000 FFFF |   2048到65535       |  1110xxxx 10xxxxxx 10xxxxxx
// 0001 0000至0010 FFFF |   65536到1114111    |  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
static void lept_encode_utf8(lept_content* c, unsigned u){
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

//解析json中的字符串
static int lept_parse_string_raw(lept_content* c, char** str, size_t* len){
    unsigned u, u2;
    size_t head = c->top;   //当前栈顶位置索引
    assert( *(c->json) == '\"');
    c->json++;
    const char* p = c->json;
    while(1){
        char ch = *p++;
        switch(ch){
            case '\"':
                *len = c->top - head; //计算当前容器中包含的字符长度
                //取出容器中指定长度的字符串存储于节点lept_value中;
                *str = lept_content_pop(c, *len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        //遇到 `\u` 转义时，调用 `lept_parse_hex4()` 解析 4 位十六进数字，存储为码点 `u`。
                        if( !(p = lept_parse_hex4(p,&u)) ) 
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        
                        //然后，我们用下列公式把代理对 (H, L) 变换成真实的码点：
                        //codepoint = 0x10000 + (H − 0xD800) × 0x400 + (L − 0xDC00)
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);

                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        //最后,根据码点按照UTF8规则进行编码,写进缓冲区。
                        lept_encode_utf8(c,u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                //handle invalid char
                //无需转义的字符中 unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
                //' " ' == %x22  ,  ' / ' == %x5C  is already been handled.
                //ASCII码中总共有128个字符; 33个控制字符, 95个可显示字符;
                //可显示的字符范围是0x20~0x7E;
                //0x20以下的字符和0x7F字符(DEL)都是不可见的控制字符;
                //0x7F以上是需要额外处理的Unicode字符范围;
                if ((unsigned char)ch < 0x20) 
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                //将无需转义的, 且可显示的ASCII字符存入缓冲区容器中;
                PUTC(c, ch); //通过宏每次压入一个字符至缓冲区;
        }
    }
}

static int lept_parse_string(lept_content* c, lept_value* v){
    int ret;
    char* s;
    size_t len;
    ret = lept_parse_string_raw(c, &s, &len);
    if( ret == LEPT_PARSE_OK ){
        lept_set_string(v, s, len);
    }
    return ret;
}


static int lept_parse_array(lept_content* c, lept_value* v){
    assert(v != NULL && c != NULL);
    assert(*c->json == '[');
    
    //元素个数初始化
    size_t size = 0;
    size_t i = 0;
    int ret;
    c->json++;
    //去掉开头的空白符
    lept_parse_whiteSpace(c);
    //特殊情况, 内部没有元素
    if(*c->json == ']'){
        c->json++;
        v->type = LEPT_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return LEPT_PARSE_OK;
    }
    //开始解析数组内部元素
    while(1){
        lept_value e; //定义一个临时节点用以保存当前解析的数组成员数据
        lept_init(&e);
        ret = lept_parse_value(c, &e); //解析后存于临时节点e中;
        if(ret != LEPT_PARSE_OK){
            break;
        }
        //将临时节点e的数据拷贝进缓冲区顶部; 每次拷贝一个节点大小;
        memcpy(lept_content_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
        size++; //数组元素个数加一;
        lept_parse_whiteSpace(c);
        //通过逗号确定数组成员
        if (*c->json == ','){
            c->json++;
            lept_parse_whiteSpace(c);
            if(*c->json == ']'){
                //由于json中的数组中不支持尾部','逗号, 返回缺少数组元素的错误码
                ret = LEPT_PARSE_MISS_ARRAY_ELEMENT;
                break; 
            }
        }
        //通过尾部']'确定数组结尾
        else if (*c->json == ']') {
            c->json++;
            v->type = LEPT_ARRAY;
            v->u.a.size = size;
            size *= sizeof(lept_value); //size*节点大小获得总大小
            //将缓冲区中的数据存储于新申请的动态内存空间;
            v->u.a.e = (lept_value*)malloc(size);
            memcpy(v->u.a.e, lept_content_pop(c, size), size);
            return LEPT_PARSE_OK;
        }
        else{
            //解析缺少逗号或方括号
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    /* Pop and free values on the stack */
    for (i = 0; i < size; i++)
        lept_free((lept_value*)lept_content_pop(c, sizeof(lept_value)));
    return ret;
}


static int lept_parse_object(lept_content* c, lept_value* v){
    int ret;
    lept_member m; //临时对象成员
    size_t i, size;

    EXPECT(c, '{');
    lept_parse_whiteSpace(c);

    //当对象内部没有成员时
    if(*c->json == '}'){
        c->json++;
        v->type = LEPT_OBJECT;
        v->u.o.m = NULL;
        v->u.o.size = 0;
        return LEPT_PARSE_OK;
    }
    
    //初始化成员键值
    m.key = NULL;
    //初始化成员个数
    size = 0;
    while(1){
        char* str;
        lept_init(&m.v); //初始化成员value的节点数据结构

        /* parse key */
        if (*c->json != '"') {
            ret = LEPT_PARSE_MISS_KEY; //键值中缺少的'"';
            break;
        }
        //解析键值的字符串, 获取键值的指针str, 和键值的长度;
        ret = lept_parse_string_raw(c, &str, &m.keyLen);
        if(ret != LEPT_PARSE_OK){
            break;
        }
        //额外申请空间存储键值
        m.key = (char*)malloc(m.keyLen + 1);
        assert(m.key != NULL);
        //拷贝键值到内存空间
        memcpy(m.key, str, m.keyLen);
        //记得加上字符串的结尾'\0'
        m.key[m.keyLen + 1] = '\0';

        /* parse ws colon ws */
        lept_parse_whiteSpace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON; //当对象成员缺少冒号时
            break;
        }
        c->json++;
        lept_parse_whiteSpace(c);
        //解析成员的实际值, 存储到临时的成员数据结构中
        ret = lept_parse_value(c, &m.v);
        if(ret != LEPT_PARSE_OK){
            break;
        }
        //将临时成员数据结构存放进缓冲区
        void* push = lept_content_push(c, sizeof(lept_member));
        memcpy(push, &m, sizeof(lept_member));
        size++;
        
        //初始化临时的成员数据结构键值指针
        m.key = NULL;
        lept_parse_whiteSpace(c);
        
        //解析多个成员的情况;
        if (*c->json == ',') {
            c->json++;
            lept_parse_whiteSpace(c);
        }
        else if (*c->json == '}') {  //解析结束时将所有的成员数据结构全部从缓冲区中移除, 并存储于申请的空间;
            size_t s = sizeof(lept_member) * size;
            c->json++;
            v->type = LEPT_OBJECT;
            v->u.o.size = size;
            v->u.o.m = (lept_member*)malloc(s);
            memcpy(v->u.o.m, lept_content_pop(c, s), s);
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET; //对象解析时缺少冒号或者右括号;
            break;
        }
    }

     /* Pop and free members on the stack */
    // 只要以上情况中出现一个错误就会执行以下代码
    // 释放临时成员数据结构中键值的空间;
    // 然后挨个释放缓冲区中每个成员数据结构的键值空间和value值空间;
    free(m.key);
    for (i = 0; i < size; i++) {
        lept_member* m = (lept_member*)lept_content_pop(c, sizeof(lept_member));
        free(m->key);
        lept_free(&m->v);
    }
    v->type = LEPT_NULL;
    return ret;
}


static int lept_parse_iteral(lept_content* c, lept_value* v, int type){
    if(type == LEPT_NULL){
        return lept_parse_null(c, v);
    }
    else if(type == LEPT_TRUE){
        return lept_parse_true(c, v);
    }
    else if (type == LEPT_FALSE){
        return lept_parse_false(c, v);
    }
    else if(type == LEPT_NUMBER){
        return lept_parse_double(c, v);
    }
    else if(type == LEPT_STRING){
        return lept_parse_string(c, v);
    }
    else if(type == LEPT_ARRAY){
        return lept_parse_array(c, v);
    }
    else if(type == LEPT_OBJECT){
        return lept_parse_object(c, v);
    }
    return LEPT_PARSE_INVALID_VALUE;
}

//判断当前值是否是指定字面量;
/* value 可能等于 null / false / true */
static int lept_parse_value(lept_content* c, lept_value* v){
    const char* p = c->json;
    switch(*p){
        case 'n': return lept_parse_iteral(c, v, LEPT_NULL);  //解析null
        case 'f': return lept_parse_iteral(c, v, LEPT_FALSE); //解析false
        case 't': return lept_parse_iteral(c, v, LEPT_TRUE);  //解析true
        case '"': return lept_parse_iteral(c, v, LEPT_STRING); //解析string
        case '[': return lept_parse_iteral(c, v, LEPT_ARRAY); // 解析array
        case '{': return lept_parse_iteral(c, v, LEPT_OBJECT); // 解析对象;
        case '\0': return LEPT_PARSE_EXCEPT_VALUE; //返回异常值错误
        default: return lept_parse_iteral(c, v, LEPT_NUMBER);//返回无效错误码 或者解析数字
    }
}

/* json_text = ws + json + ws  */
int lept_parse(lept_value* v, const char* json){
    //使用断言进行判断输入参数是否正常;
    assert(v != NULL);

    int ret;
    lept_content c;
    //存储json字符串的当前位置
    c.json = json;
    c.stack = NULL;
    c.size = 0;
    c.top = 0;
    //将节点的类型设置为null类型
    v->type = LEPT_NULL;
    //解析空白, 将json指针移动到值的位置;
    lept_parse_whiteSpace(&c);
    //解析值, 并返回enum值;
    ret = lept_parse_value(&c, v);
    if(ret == LEPT_PARSE_OK){
        lept_parse_whiteSpace(&c);
        if(*c.json != '\0'){
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;//说明json文本还有其他字符;
        }
    }
    assert(c.top == 0); //在释放时，加入了断言确保所有数据都被弹出。
    free(c.stack);
    return ret;
}

//type
lept_type lept_get_type(const lept_value* v){
    assert(v != NULL);
    return v->type;
}

//string
void lept_set_string(lept_value* v, const char* s, size_t len){
    assert(v != NULL && (s != NULL || len == 0));
    //释放原来的
    lept_free(v);
    //重新申请空间
    v->u.s.s = (char*)malloc(len + 1);
    //拷贝
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0'; //填补结尾空字符
    v->u.s.len = len;     //字符串长度
    v->type = LEPT_STRING;
}

size_t lept_get_string_length(const lept_value* v){
    assert(v != NULL && (v->type == LEPT_STRING) );
    return v->u.s.len;
}
const char* lept_get_string(const lept_value* v){
    assert(v != NULL && (v->type == LEPT_STRING) );
    return v->u.s.s;
}

//bool类型
int lept_get_boolean(const lept_value* v){
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE) );
    return v->type == LEPT_TRUE;
}
void lept_set_boolean(lept_value* v, int b){
    assert(v != NULL);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

//number
double lept_get_number(const lept_value* v){
    assert(v != NULL && (v->type == LEPT_NUMBER) );
    return v->u.n;

}
void lept_set_number(lept_value* v, double n){
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_NUMBER;
    v->u.n = n;
}

//array获取信息的接口
lept_value* lept_get_array_element(const lept_value* v, size_t index){
    assert( (v != NULL) && v->type == LEPT_ARRAY);
    assert( v->u.a.size > index );
    return &v->u.a.e[index];
}
size_t lept_get_array_size(const lept_value* v){
    assert( v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}

//对象中成员的个数
size_t lept_get_object_size(const lept_value* v){
    assert(v != NULL && v->type == LEPT_OBJECT );
    return v->u.o.size;
}
//对象中成员的键值
const char* lept_get_object_key(const lept_value* v, size_t index){
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert( index < v->u.o.size );
    return v->u.o.m[index].key;
}
//对象中成员键值的长度
size_t lept_get_object_key_length(const lept_value* v, size_t index){
    assert(v != NULL && v->type == LEPT_OBJECT );
    assert( index < v->u.o.size );
    return v->u.o.m[index].keyLen;
}
//对象成员对应的值
lept_value* lept_get_object_value(const lept_value* v, size_t index){
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert( index < v->u.o.size );
    return &v->u.o.m[index].v;
}


#if 1
// Unoptimized
static void lept_stringify_string(lept_content* c, const char* s, size_t len) {
    size_t i;
    assert(s != NULL);
    PUTC(c, '"');

    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': PUTS(c, "\\\"", 2); break;
            case '\\': PUTS(c, "\\\\", 2); break;
            case '\b': PUTS(c, "\\b",  2); break;
            case '\f': PUTS(c, "\\f",  2); break;
            case '\n': PUTS(c, "\\n",  2); break;
            case '\r': PUTS(c, "\\r",  2); break;
            case '\t': PUTS(c, "\\t",  2); break;
            default:
                if (ch < 0x20) {
                    char buffer[7];
                    sprintf(buffer, "\\u%04X", ch);
                    PUTS(c, buffer, 6);
                }
                else
                    PUTC(c, s[i]);
        }
    }
    PUTC(c, '"');
}
#else
//自行编写十六进位输出，避免了 `printf()` 内解析格式的开销
static void lept_stringify_string(lept_content* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != NULL);
    p = head = lept_content_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}
#endif

static void lept_stringify_value(lept_content* c, const lept_value* v) {
    size_t i;
    switch (v->type) {
        case LEPT_NULL:   PUTS(c, "null",  4); break;
        case LEPT_FALSE:  PUTS(c, "false", 5); break;
        case LEPT_TRUE:   PUTS(c, "true",  4); break;
        //将浮点数转换为文本字符串;
        case LEPT_NUMBER: 
            //"%.17g"足够把双精度浮点数转换为可还原的文本;
            c->top -= 32 - sprintf(lept_content_push(c, 32), "%.17g", v->u.n);
            break;
        //转换string字符串
        case LEPT_STRING: 
            lept_stringify_string(c, v->u.s.s, v->u.s.len); 
            break;
        case LEPT_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                lept_stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case LEPT_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                lept_stringify_string(c, v->u.o.m[i].key, v->u.o.m[i].keyLen);
                PUTC(c, ':');
                lept_stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

char* lept_stringify(const lept_value* v, size_t* length) {
    lept_content c;
    assert(v != NULL);
    //申请输出缓冲区
    c.stack = (char*)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
    assert(c.stack != NULL);
    c.top = 0;
    //将节点数据结构中保存的值进行字符串化, 并存入输出缓冲区
    lept_stringify_value(&c, v);
    //传入非空指针, 那么就可以获取生成的json字符串长度;
    if (length)
        *length = c.top;
    //为json结尾添加'\0';
    PUTC(&c, '\0');
    return c.stack;
}

void lept_free(lept_value* v){
    assert(v != NULL);
    size_t i = 0;
    switch(v->type){
        case LEPT_STRING: 
            free(v->u.s.s);
            v->u.s.s = NULL;
            break;
        case LEPT_ARRAY:
            for( i = 0; i < v->u.a.size; i++){
                lept_free(&v->u.a.e[i]);    
            }
            free(v->u.a.e);
            v->u.a.e = NULL;
            break;
        case LEPT_OBJECT:
             for (i = 0; i < v->u.o.size; i++) {
                free(v->u.o.m[i].key);
                lept_free(&v->u.o.m[i].v);
            }
            free(v->u.o.m);
            break;
        default:
            break;
    }

    lept_init(v);
}