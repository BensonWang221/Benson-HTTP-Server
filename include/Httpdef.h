#ifndef UNIDEF_INCLUDED
#define UNIDEF_INCLUDED

#include <string>
enum HTTP_CODE
{
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

// 根目录
const std::string RootDir("./www/html");
#endif