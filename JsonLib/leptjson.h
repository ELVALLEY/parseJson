/*实现一个轻量级的json库 */

#ifndef LEPTJSON_H_
#define LEPTJSON_H_

#include <stddef.h>

/*
定义json中的六种数据类型
因为 C 语言没有 C++ 的命名空间（namespace）功能
使用项目的简写作为标识符的前缀。 
*/
typedef enum{
    LEPT_NULL,  //节点类型为null
    LEPT_TRUE,  //节点类型为true
    LEPT_FALSE, //节点类型为false
    LEPT_NUMBER, //节点类型为double
    LEPT_STRING, //节点类型为string
    LEPT_ARRAY, //节点类型为array
    LEPT_OBJECT //节点类型为object
} lept_type;

/* 定义obeject的数据结构类型 */
typedef struct lept_member lept_member;
/* 定义json树形结构中的节点数据类型 */
typedef struct lept_value lept_value; 
struct lept_value{
    union 
    {
        struct {lept_member* m; size_t size; }o; //object
        struct {lept_value* e; size_t size;}a; //array
        struct{char* s; size_t len;}s;    //string
        double n;                         //number
    }u;
    
    lept_type type; //通过 `type` 来决定它现时是哪种类型,可以通过type直接为bool类型为true或false;
};

/*  'lept_member' 是一个 'lept_value' 加上键的字符串 */
struct lept_member{
    char* key;     //对象成员键值
    size_t keyLen; //键值长度
    lept_value v; // 对象成员的值的数据结构;
};

/* 解析后返回的异常值, 无错误返回LEPT_PARSE_OK */
enum{
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXCEPT_VALUE,  //解析为异常值
    LEPT_PARSE_INVALID_VALUE, //解析结果为无效值
    LEPT_PARSE_ROOT_NOT_SINGULAR, //解析为有效值, 但是值的空白之后还有其他字符
    LEPT_PARSE_NUMBER_TOO_BIG,//解析后发现数字过大
    LEPT_PARSE_MISS_QUOTATION_MARK,     //缺少' " ';
    LEPT_PARSE_INVALID_STRING_ESCAPE,   //无效的string中的转义字符
    LEPT_PARSE_INVALID_STRING_CHAR,     //无效的string字符
    LEPT_PARSE_INVALID_UNICODE_HEX,   //无效的unicode码点
    LEPT_PARSE_INVALID_UNICODE_SURROGATE, //无效的unicode代理
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, //解析缺少逗号或方括号的错误码
    LEPT_PARSE_MISS_ARRAY_ELEMENT, //缺少数组元素错误码
    LEPT_PARSE_MISS_KEY,            //对象成员键值缺少'"'
    LEPT_PARSE_MISS_COLON,           //缺少冒号
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET //缺少逗号或者右花括号
};

//存储解析过程中json文本的字符串指针和动态空间指针, 以及空间的大小和顶部
typedef struct{
    const char* json;   //json文本中的字符指针
    char* stack;        //动态的堆栈
    size_t size;        //size 是当前的堆栈容量
    size_t top;         //top 是当前栈顶的位置索引
}lept_content;

//初始化节点类型为LEPT_NULL
#define lept_init(v)  do { (v)->type = LEPT_NULL; } while(0)

//解析json文本的接口, 成功返回LEPT_PARSE_OK==0, 错误返回错误码
int lept_parse(lept_value* v, const char* json);

//获取当前节点的类型
lept_type lept_get_type(const lept_value* v);

//设置节点中的string值
void lept_set_string(lept_value* v, const char* s, size_t len);
//获取string节点中string的字符长度
size_t lept_get_string_length(const lept_value* v);
//获取string节点中的string字符串
const char* lept_get_string(const lept_value* v);

//获取节点中的bool值
int lept_get_boolean(const lept_value* v);
//设置节点中的bool值
void lept_set_boolean(lept_value* v, int b);

//获取节点中的Number
double lept_get_number(const lept_value* v);
//设置节点中的Number
void lept_set_number(lept_value* v, double n);

//获取节点中数组成员元素的节点数据结构
lept_value* lept_get_array_element(const lept_value* v, size_t index);
//获取数组中成员的个数
size_t lept_get_array_size(const lept_value* v);

//对象中成员的个数
size_t lept_get_object_size(const lept_value* v);
//对象中成员的键值
const char* lept_get_object_key(const lept_value* v, size_t index);
//对象中成员键值的长度
size_t lept_get_object_key_length(const lept_value* v, size_t index);
//对象成员对应的值
lept_value* lept_get_object_value(const lept_value* v, size_t index);

//Json生成器
char* lept_stringify(const lept_value* v, size_t* length);

//释放string类型节点的指针,存放string字符串的空间是动态的, 并将节点类型置NULL
void lept_free(lept_value* v);

#endif