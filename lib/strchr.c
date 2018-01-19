char *strchr(char *str, int c) 
{
    for (; *str && *str != c; str++)
        ;
    return str;
}
