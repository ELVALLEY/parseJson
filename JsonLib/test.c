/*
编写一个极度简单的单元测试框架
*/
#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "leptjson.h"

static int main_ret = 0;    //测试返回值
static int test_count = 0; //测试的数量
static int test_pass = 0;   //测试通过的数量

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do{\
        test_count++;\
        if(equality) {\
            test_pass++;\
        }\
        else{\
            fprintf(stderr, "%s: %d: expect_value: "format" actual_value: "format"\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    }while(0);

//每次使用这个宏时，如果 expect != actual（预期值不等于实际值），便会输出错误信息
#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect)==(actual), expect, actual, "%d")

#define EXPECT_EQ_DOUBLE(expect, actual) \
        EXPECT_EQ_BASE(((expect - actual) < 0.00000001), expect, actual, "%.17g")

#define EXPECT_NUMBER(expect, actual) \
        EXPECT_EQ_BASE(((expect - actual) < 0.00000001), expect, actual, "%f")


#define EXPECT_EQ_STRING(string, get_string, get_string_length) \
        EXPECT_EQ_BASE(memcmp(string, get_string, get_string_length) == 0 && (sizeof(string)-1) == get_string_length, string, get_string, "%s") 

#define EXPECT_TRUE(actual) \
        EXPECT_EQ_BASE( (actual != 0), "true", "false", "%s" );

#define EXPECT_FALSE(actual) \
        EXPECT_EQ_BASE( (actual == 0), "false", "true", "%s" );

//ANSI C（C89）并没有的 `size_t` 打印方法，
//在 C99 则加入了 `"%zu"`，
//但 VS2015 中才有，之前的 VC 版本使用非标准的 `"%Iu"`
#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#endif

//这个宏能够有效的减少代码
#define TEST_ERROR(error, finalType, json) \
    do{ \
        lept_value  v;\
        v.type = LEPT_NULL;\
        EXPECT_EQ_INT(error, lept_parse(&v, json));\
        EXPECT_EQ_INT(finalType, lept_get_type(&v));\
        lept_free(&v);\
    }while(0);

#define TEST_NUMBER(except, json) \
    do{ \
        lept_value  v;\
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json))\
        EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v))\
        EXPECT_EQ_DOUBLE(except, lept_get_number(&v))\
    }while(0);

#define TEST_STRING_PARSE(expect, json) \
    do {\
        lept_value v;\
        v.type = LEPT_NULL;\
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json));\
        EXPECT_EQ_INT(LEPT_STRING, lept_get_type(&v));\
        EXPECT_EQ_STRING(expect, lept_get_string(&v), lept_get_string_length(&v));\
        lept_free(&v);\
    } while(0);

#define TEST_ROUNDTRIP(json)\
    do {\
        lept_value v;\
        char* json2;\
        size_t length;\
        lept_init(&v);\
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json));\
        json2 = lept_stringify(&v, &length);\
        EXPECT_EQ_STRING(json, json2, length);\
        lept_free(&v);\
        free(json2);\
    } while(0)

static void test_parse_null(){
    TEST_ERROR(LEPT_PARSE_OK, LEPT_NULL, " null ")
}

static void test_parse_false(){
    TEST_ERROR(LEPT_PARSE_OK, LEPT_FALSE, " false ")
}

static void test_parse_true(){
    TEST_ERROR(LEPT_PARSE_OK, LEPT_TRUE, " true ")
}
static void test_parse_number() {
    TEST_NUMBER(0.0, " 0")
    TEST_NUMBER(0.0, "-0")
    TEST_NUMBER(0.0, "-0.0")
    TEST_NUMBER(1.0, "1")
    TEST_NUMBER(-1.0, "-1")
    TEST_NUMBER(1.5, "1.5")
    TEST_NUMBER(-1.5, "-1.5")
    TEST_NUMBER(3.1416, "3.1416")
    TEST_NUMBER(1E10, "1E10")
    TEST_NUMBER(1e10, "1e10")
    TEST_NUMBER(1E+10, "1E+10")
    TEST_NUMBER(1E-10, "1E-10")
    TEST_NUMBER(-1E10, "-1E10")
    TEST_NUMBER(-1e10, "-1e10")
    TEST_NUMBER(-1E+10, "-1E+10")
    TEST_NUMBER(-1E-10, "-1E-10")
    TEST_NUMBER(1.234E+10, "1.234E+10")
    TEST_NUMBER(1.234E-10, "1.234E-10")
    TEST_NUMBER(0.0, "1e-10000") /* must underflow */
}

static void test_parse_except_value(){
    TEST_ERROR(LEPT_PARSE_EXCEPT_VALUE, LEPT_NULL, "")
    TEST_ERROR(LEPT_PARSE_EXCEPT_VALUE, LEPT_NULL, " ")
}
static void test_parse_invalid_value(){
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "nul")
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "?")

     /* invalid number */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "+0")
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "+1")
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, ".123")/* at least one digit before '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "1.") /* at least one digit after '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "INF")
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "inf")
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "NAN")
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, LEPT_NULL, "nan")
}
static void test_parse_root_not_singular(){
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, LEPT_NULL, "nullx")
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, LEPT_FALSE, "false x")
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, LEPT_TRUE, "trues")

    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, LEPT_NUMBER, "0123")//'0'之后只能是, '.', 'e'或'E'或者只有0
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, LEPT_NUMBER, "0x12")
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, LEPT_NUMBER, "0x0")

}

static void test_parse_number_too_big(){
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, LEPT_NULL, "1.7e309")
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, LEPT_NULL, "-1.7e309")
}

static void test_parse_string(){
    TEST_STRING_PARSE("", "\"\"");
    TEST_STRING_PARSE("Hello", "\"Hello\"");
    TEST_STRING_PARSE("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING_PARSE("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
    TEST_STRING_PARSE("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STRING_PARSE("\x24", "\"\\u0024\"");         /* Dollar sign U+0024 */
    TEST_STRING_PARSE("\xC2\xA2", "\"\\u00A2\"");     /* Cents sign U+00A2 */
    TEST_STRING_PARSE("\xE2\x82\xAC", "\"\\u20AC\""); /* Euro sign U+20AC */
    TEST_STRING_PARSE("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  /* G clef sign U+1D11E */
    TEST_STRING_PARSE("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");  /* G clef sign U+1D11E */
}

static void test_parse_missing_quotation_mark() {
    TEST_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK, LEPT_NULL, "\"");
    TEST_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK, LEPT_NULL, "\"abc");
}

static void test_parse_invalid_string_escape() {
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, LEPT_NULL,  "\"\\v\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, LEPT_NULL, "\"\\'\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, LEPT_NULL, "\"\\0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, LEPT_NULL, "\"\\x12\"");
}

static void test_parse_invalid_string_char() {
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, LEPT_NULL, "\"\x01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, LEPT_NULL, "\"\x1F\"");
}

static void test_parse_invalid_unicode_hex() {
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u012\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u/000\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\uG000\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u0/00\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u0G00\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u00/0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u00G0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u000/\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u000G\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, LEPT_NULL, "\"\\u 123\"");
}

static void test_parse_invalid_unicode_surrogate() {
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, LEPT_NULL, "\"\\uD800\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, LEPT_NULL, "\"\\uDBFF\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, LEPT_NULL, "\"\\uD800\\\\\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, LEPT_NULL, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, LEPT_NULL, "\"\\uD800\\uE000\"");
}

//测试array的parse和获取数组元素和元素个数的接口
static void test_parse_array(){
    lept_value v;
    size_t i, j;
    lept_init(&v);

    // 测试零元素, 且空白的数组
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(0, lept_get_array_size(&v));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[  null , false , true , 123 , \"abc\"]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(5, lept_get_array_size(&v));
    EXPECT_EQ_INT(LEPT_NULL,   lept_get_type(lept_get_array_element(&v, 0)));
    EXPECT_EQ_INT(LEPT_FALSE,  lept_get_type(lept_get_array_element(&v, 1)));
    EXPECT_EQ_INT(LEPT_TRUE,   lept_get_type(lept_get_array_element(&v, 2)));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_array_element(&v, 3)));
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_array_element(&v, 3)));
    EXPECT_EQ_STRING("abc", lept_get_string(lept_get_array_element(&v, 4)), lept_get_string_length(lept_get_array_element(&v, 4)));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(4, lept_get_array_size(&v));
    for (i = 0; i < 4; i++) {
        lept_value* a = lept_get_array_element(&v, i);
        EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(a));
        EXPECT_EQ_SIZE_T(i, lept_get_array_size(a));
        for (j = 0; j < i; j++) {
            lept_value* e = lept_get_array_element(a, j);
            EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(e));
            EXPECT_EQ_DOUBLE((double)j, lept_get_number(e));
        }
    }
    lept_free(&v);

}
static void test_parse_miss_key() {
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{1:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{true:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{false:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{null:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{[]:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{{}:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, LEPT_NULL, "{\"a\":1,");
}

static void test_parse_miss_colon() {
    TEST_ERROR(LEPT_PARSE_MISS_COLON, LEPT_NULL, "{\"a\"}");
    TEST_ERROR(LEPT_PARSE_MISS_COLON, LEPT_NULL, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket() {
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, LEPT_NULL, "{\"a\":1");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, LEPT_NULL, "{\"a\":1]");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, LEPT_NULL, "{\"a\":1 \"b\"");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, LEPT_NULL, "{\"a\":{}");
}

static void test_parse_object() {
    lept_value v;
    size_t i;

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, " { } "));
    EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(0, lept_get_object_size(&v));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v,
        " { "
        "\"n\" : null , "
        "\"f\" : false , "
        "\"t\" : true , "
        "\"i\" : 123 , "
        "\"s\" : \"abc\", "
        "\"a\" : [ 1, 2, 3 ],"
        "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
        " } "
    ));
    EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(7, lept_get_object_size(&v));
    EXPECT_EQ_STRING("n", lept_get_object_key(&v, 0), lept_get_object_key_length(&v, 0));
    EXPECT_EQ_INT(LEPT_NULL,   lept_get_type(lept_get_object_value(&v, 0)));
    EXPECT_EQ_STRING("f", lept_get_object_key(&v, 1), lept_get_object_key_length(&v, 1));
    EXPECT_EQ_INT(LEPT_FALSE,  lept_get_type(lept_get_object_value(&v, 1)));
    EXPECT_EQ_STRING("t", lept_get_object_key(&v, 2), lept_get_object_key_length(&v, 2));
    EXPECT_EQ_INT(LEPT_TRUE,   lept_get_type(lept_get_object_value(&v, 2)));
    EXPECT_EQ_STRING("i", lept_get_object_key(&v, 3), lept_get_object_key_length(&v, 3));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_object_value(&v, 3)));
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_object_value(&v, 3)));
    EXPECT_EQ_STRING("s", lept_get_object_key(&v, 4), lept_get_object_key_length(&v, 4));
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("abc", lept_get_string(lept_get_object_value(&v, 4)), lept_get_string_length(lept_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("a", lept_get_object_key(&v, 5), lept_get_object_key_length(&v, 5));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(lept_get_object_value(&v, 5)));
    EXPECT_EQ_SIZE_T(3, lept_get_array_size(lept_get_object_value(&v, 5)));
    for (i = 0; i < 3; i++) {
        lept_value* e = lept_get_array_element(lept_get_object_value(&v, 5), i);
        EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(e));
        EXPECT_EQ_DOUBLE(i + 1.0, lept_get_number(e));
    }
    EXPECT_EQ_STRING("o", lept_get_object_key(&v, 6), lept_get_object_key_length(&v, 6));
    {
        lept_value* o = lept_get_object_value(&v, 6);
        EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(o));
        for (i = 0; i < 3; i++) {
            lept_value* ov = lept_get_object_value(o, i);
            EXPECT_TRUE('1' + i == lept_get_object_key(o, i)[0]);
            EXPECT_EQ_SIZE_T(1, lept_get_object_key_length(o, i));
            EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(ov));
            EXPECT_EQ_DOUBLE(i + 1.0, lept_get_number(ov));
        }
    }
    lept_free(&v);
}

static void test_stringify_number() {
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("-1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
    TEST_ROUNDTRIP("1e+20");
    TEST_ROUNDTRIP("1.234e+20");
    TEST_ROUNDTRIP("1.234e-20");

    TEST_ROUNDTRIP("1.0000000000000002"); /* the smallest number > 1 */
    TEST_ROUNDTRIP("4.9406564584124654e-324"); /* minimum denormal */
    TEST_ROUNDTRIP("-4.9406564584124654e-324");
    TEST_ROUNDTRIP("2.2250738585072009e-308");  /* Max subnormal double */
    TEST_ROUNDTRIP("-2.2250738585072009e-308");
    TEST_ROUNDTRIP("2.2250738585072014e-308");  /* Min normal positive double */
    TEST_ROUNDTRIP("-2.2250738585072014e-308");
    TEST_ROUNDTRIP("1.7976931348623157e+308");  /* Max double */
    TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

static void test_stringify_string() {
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
    TEST_ROUNDTRIP("\"Hello\\u0000World\"");
}

static void test_stringify_array() {
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

static void test_stringify_object() {
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

static void test_stringify() {
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");
    test_stringify_number();
    test_stringify_string();
    test_stringify_array();
    test_stringify_object();
}



static void test_parse(){
    //测试能否正确解析json文本的value值
    test_parse_null(); 
    test_parse_false();
    test_parse_true();

    //测试能否正确解析json中的数字字符
    test_parse_number();
    test_parse_number_too_big();

    //测试能否正确get和set, json中的字符串
    //测试解析json字符串
    test_parse_string();

    test_parse_array();

    test_parse_missing_quotation_mark();
    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();

    //测试_能否正确的在异常时得到错误码
    test_parse_except_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();


    test_parse_object();
    //解析对象时可能产生的错误码测试
    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();
}


static void test_access_boolean(){
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "a", 1);
    lept_free(&v);

    lept_init(&v);
    lept_set_boolean(&v, 0);
    EXPECT_FALSE(lept_get_boolean(&v));

    lept_init(&v);
    lept_set_boolean(&v, 1);
    EXPECT_TRUE(lept_get_boolean(&v));
    lept_free(&v);
}

static void test_access_null() {
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "a", 1);
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
    lept_free(&v);
}

static void test_access_number(){
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "a", 1);
    lept_free(&v);

    lept_init(&v);
    lept_set_number(&v, 123.5);
    EXPECT_NUMBER(123.5, lept_get_number(&v));

    lept_set_number(&v, 0.0333);
    EXPECT_NUMBER(0.0333, lept_get_number(&v));
    lept_free(&v);
}

static void test_access_string(){
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "", 0);
    EXPECT_EQ_STRING("", lept_get_string(&v), lept_get_string_length(&v))
    lept_set_string(&v, "Hello", 5);
    EXPECT_EQ_STRING("Hello", lept_get_string(&v), lept_get_string_length(&v))
    lept_free(&v);
}

static void test_access(){
    test_access_boolean();
    test_access_number();
    test_access_string();
    test_access_null();
}

static int parse_file(const char* filename, const char* mode){
    FILE* fp = NULL;
    FILE* fp2 = NULL;
    errno_t err;
    char buff[1024];
    char buffer[4096];
    lept_value v;
    lept_init(&v);
    size_t object_size;
    int ret;
    int size;
    char* read_str ;
    char* new_str = NULL;
    size_t str_length = 0;

    err = fopen_s(&fp, filename, mode);
    if(err == 0){
        printf("The file was opened\n");

        while(1){
            fgets(buff, sizeof(buff), fp);
            
            // strcat(buffer, buff); //可能在使用时产生溢出

            strcat_s(buffer, sizeof(buffer), buff); //如果产生溢出时会抛出异常;

            if(feof(fp))//到文件结尾跳出循环
                break;
        }
        printf("Content : %s\n length: %zu\n",  buffer, strlen(buffer)+1);


        ret = lept_parse(&v, buffer);
        if( ret == LEPT_PARSE_OK){
            if(lept_get_type(&v) == LEPT_OBJECT){
                object_size = lept_get_object_size(&v);
                printf(" Have Json Object : %zu\n", object_size);

                new_str = lept_stringify(&v, &str_length);
                if(!new_str){
                    printf("stringify is faild\n");
                    return -1;
                }
                fp2 = fopen("C:\\Users\\EL_Valley\\Desktop\\newJson.json", "w");
                if(!fp2){
                    printf("create new json file is faild\n");
                    return -1;
                }
                fprintf(fp2, "%s", new_str);
                fclose(fp2);

            }

        

            lept_free(&v);
            return 10;

        }
        else{
            printf("Parse is faild number: %d\n", ret);
        }
    
        fclose(fp);
    }
    else
        printf("Open the file was failed\n");
    
    return -1;
}

static int test_parse_file(){
    
    return parse_file("C:\\Users\\EL_Valley\\Desktop\\testJson_1.json", "r+");
}

int main(){
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    test_parse();
    test_access();
    test_stringify();
    printf("pass: %d sum: %d (%3.2f%%)passed\n", test_pass, test_count, test_pass*100.0/test_count);

    // TEST_STRING_PARSE("\xE2\x82\xAC", "\"\\u20AC\""); /* Euro sign U+20AC */
    // TEST_STRING_PARSE("\xC2\xA2", "\"\\u00A2\"");     /* Cents sign U+00A2 */
    // TEST_STRING_PARSE("\xE2\x82\xAC", "\"\\u20AC\""); /* Euro sign U+20AC */
    // TEST_STRING_PARSE("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  /* G clef sign U+1D11E */
    // TEST_STRING_PARSE("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");  /* G clef sign U+1D11E */

    printf("num = %c\n", '\x24');

    // printf("num = %c\n", '\u4E25');
    // printf("num = %c\n", \xF0\x9D\x84\x9E);

    test_parse_file();

    return main_ret;
}